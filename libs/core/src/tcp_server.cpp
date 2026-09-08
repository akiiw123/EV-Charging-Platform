// 管理监听端口、客户端连接、线程池和消息收发，并把请求交给路由器。
// start() 建立监听；incomingConnection() 检查容量；连接任务逐行解码消息并调用 RequestRouter。
// 每个工作线程创建自己的 SQLite 连接，连接结束后释放会话和容量计数。
#include "charging/core/tcp_server.h"

#include "charging/core/database_manager.h"
#include "charging/core/message_protocol.h"
#include "charging/core/request_router.h"

#include <QRunnable>
#include <QTcpSocket>
#include <QThread>
#include <QUuid>

namespace charging::core {
namespace {

constexpr qsizetype kMaximumMessageBytes = 1024 * 1024;

// 会话被服务端关闭时主动下发的通知:根据关闭原因选择错误码与文案
Message sessionClosedMessage(RequestRouter::SessionCloseReason reason)
{
    const bool takenOver = reason == RequestRouter::SessionCloseReason::TakenOver;
    return {QStringLiteral("server"), QStringLiteral("server.session.closed"),
            {{QStringLiteral("code"), takenOver ? QStringLiteral("AUTH_SESSION_TAKEOVER")
                                                : QStringLiteral("AUTH_USER_FROZEN")},
             {QStringLiteral("message"), takenOver
                  ? QStringLiteral("账号已在其他设备登录,本连接已下线")
                  : QStringLiteral("账号已被冻结,连接已断开")}}};
}

class ConnectionTask final : public QRunnable {
public:
    ConnectionTask(qintptr socketDescriptor, QString databasePath,
                   std::shared_ptr<std::atomic_bool> stopping,
                   std::shared_ptr<SessionRegistry> sessions)
        : socketDescriptor_(socketDescriptor), databasePath_(std::move(databasePath)),
          stopping_(std::move(stopping)), sessions_(std::move(sessions))
    {
        setAutoDelete(true);
    }

    void run() override
    {
        QTcpSocket socket;
        if (!socket.setSocketDescriptor(socketDescriptor_)) {
            return;
        }
        DatabaseManager database(QStringLiteral("socket-") + QUuid::createUuid().toString());
        QString databaseError;
        if (!database.open(databasePath_, &databaseError)) {
            write(&socket, {QStringLiteral("server"), QStringLiteral("server.error"),
                            {{QStringLiteral("code"), QStringLiteral("DATABASE_ERROR")},
                             {QStringLiteral("message"), databaseError}}});
            socket.disconnectFromHost();
            return;
        }

        RequestRouter router(database.database(), sessions_);
        QByteArray buffer;
        int idleCycles = 0;   // 250ms/次,20 次 ≈ 5 秒做一次冻结复查
        while (!stopping_->load() && socket.state() == QAbstractSocket::ConnectedState) {
            if (!socket.waitForReadyRead(250)) {
                // 单点登录:令牌复查只读内存,每个空闲周期(250ms)都做一次,
                // 让被其他客户端接管的旧连接尽快下线
                if (router.checkSessionOwnership()) {
                    write(&socket, sessionClosedMessage(router.sessionCloseReason()));
                    break;
                }
                // C4:空闲时也定期复查登录用户是否被冻结,被冻结则断开
                if (++idleCycles >= 20) {
                    idleCycles = 0;
                    if (router.refreshSession()) {
                        write(&socket, sessionClosedMessage(router.sessionCloseReason()));
                        break;
                    }
                }
                continue;
            }
            idleCycles = 0;
            buffer.append(socket.readAll());
            if (buffer.size() > kMaximumMessageBytes && !buffer.contains('\n')) {
                write(&socket, protocolError(QStringLiteral("消息超过 1 MiB 限制")));
                break;
            }
            qsizetype newline = -1;
            while ((newline = buffer.indexOf('\n')) >= 0) {
                const QByteArray line = buffer.left(newline);
                buffer.remove(0, newline + 1);
                if (line.size() > kMaximumMessageBytes) {
                    write(&socket, protocolError(QStringLiteral("消息超过 1 MiB 限制")));
                    continue;
                }
                Message request;
                QString parseError;
                if (!MessageProtocol::decodeLine(line, &request, &parseError)) {
                    write(&socket, protocolError(parseError));
                    continue;
                }
                write(&socket, router.route(request));
            }
            // 处理完本批请求后再复查一次:即使旧客户端持续发送无需鉴权的请求,
            // 被接管的连接也能在一个来回内收到通知并下线
            if (router.checkSessionOwnership()) {
                write(&socket, sessionClosedMessage(router.sessionCloseReason()));
            }
            // 会话被关闭(冻结踢出或被其他客户端接管)时,发完响应即断开连接
            if (router.sessionClosed()) {
                break;
            }
        }
        socket.disconnectFromHost();
        socket.waitForDisconnected(250);
    }

private:
    static Message protocolError(const QString& detail)
    {
        return {QStringLiteral("invalid"), QStringLiteral("protocol.error"),
                {{QStringLiteral("code"), QStringLiteral("INVALID_MESSAGE")},
                 {QStringLiteral("message"), detail}}};
    }

    static void write(QTcpSocket* socket, const Message& message)
    {
        socket->write(MessageProtocol::encode(message));
        socket->waitForBytesWritten(1000);
    }

    qintptr socketDescriptor_;
    QString databasePath_;
    std::shared_ptr<std::atomic_bool> stopping_;
    std::shared_ptr<SessionRegistry> sessions_;
};

} // namespace

TcpServer::TcpServer(QString databasePath, QObject* parent)
    : QTcpServer(parent), databasePath_(std::move(databasePath)),
      stopping_(std::make_shared<std::atomic_bool>(false)),
      sessions_(std::make_shared<SessionRegistry>())
{
   // TCP 客户端为长连接，每个连接会持续占用一个工作线程。
// 至少保留 16 个线程，保证管理端和多个用户端可以同时在线。
    pool_.setMaxThreadCount(qMax(16, QThread::idealThreadCount() * 2));
    pool_.setExpiryTimeout(30000);
}

TcpServer::~TcpServer()
{
    close();
    stopping_->store(true);
    pool_.waitForDone(3000);
}

bool TcpServer::start(const QHostAddress& address, quint16 port, QString* errorMessage)
{
    stopping_->store(false);
    if (listen(address, port)) {
        return true;
    }
    if (errorMessage) {
        *errorMessage = errorString();
    }
    return false;
}

void TcpServer::incomingConnection(qintptr socketDescriptor)
{
    pool_.start(new ConnectionTask(socketDescriptor, databasePath_, stopping_, sessions_));
}

} // namespace charging::core
