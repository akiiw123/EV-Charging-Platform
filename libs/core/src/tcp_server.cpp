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

class ConnectionTask final : public QRunnable {
public:
    ConnectionTask(qintptr socketDescriptor, QString databasePath,
                   std::shared_ptr<std::atomic_bool> stopping)
        : socketDescriptor_(socketDescriptor), databasePath_(std::move(databasePath)),
          stopping_(std::move(stopping))
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

        RequestRouter router(database.database());
        QByteArray buffer;
        while (!stopping_->load() && socket.state() == QAbstractSocket::ConnectedState) {
            if (!socket.waitForReadyRead(250)) {
                continue;
            }
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
};

} // namespace

TcpServer::TcpServer(QString databasePath, QObject* parent)
    : QTcpServer(parent), databasePath_(std::move(databasePath)),
      stopping_(std::make_shared<std::atomic_bool>(false))
{
    pool_.setMaxThreadCount(qMax(2, QThread::idealThreadCount()));
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
    pool_.start(new ConnectionTask(socketDescriptor, databasePath_, stopping_));
}

} // namespace charging::core
