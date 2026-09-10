#include "charging/core/api_client.h"

#include <QUuid>

namespace charging::core {

ApiClient::ApiClient(QObject* parent) : QObject(parent)
{
    reconnectTimer_.setInterval(2000);
    reconnectTimer_.setSingleShot(true);
    connect(&reconnectTimer_, &QTimer::timeout, this, &ApiClient::reconnect);
    connect(&socket_, &QTcpSocket::connected, this, [this] {
        reconnectTimer_.stop();
        emit connected();
    });
    connect(&socket_, &QTcpSocket::disconnected, this, [this] {
        emit disconnected();
        if (!host_.isEmpty()) {
            reconnectTimer_.start();
        }
    });
    connect(&socket_, &QTcpSocket::readyRead, this, &ApiClient::readAvailable);
    connect(&socket_, &QTcpSocket::errorOccurred, this,
            [this](QAbstractSocket::SocketError) { emit clientError(socket_.errorString()); });
}

void ApiClient::connectToServer(const QString& host, quint16 port)
{
    host_ = host;
    port_ = port;
    reconnect();
}

QString ApiClient::send(const QString& type, const QJsonObject& payload)
{
    if (!isConnected()) {
        emit clientError(QStringLiteral("尚未连接管理端"));
        return {};
    }
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const quint64 seq = ++sendCounter_;
    seqById_[id] = seq;
    latestSeqByType_[type] = seq;   // 同类型的新请求使先前请求"过期"
    socket_.write(MessageProtocol::encode({id, type, payload}));
    return id;
}

bool ApiClient::isConnected() const
{
    return socket_.state() == QAbstractSocket::ConnectedState;
}

void ApiClient::reconnect()
{
    if (host_.isEmpty() || port_ == 0 || socket_.state() != QAbstractSocket::UnconnectedState) {
        return;
    }
    // 断线后旧请求不会再有响应,清空序号簿避免残留
    seqById_.clear();
    socket_.connectToHost(host_, port_);
}

void ApiClient::readAvailable()
{
    buffer_.append(socket_.readAll());
    qsizetype newline = -1;
    while ((newline = buffer_.indexOf('\n')) >= 0) {
        const QByteArray line = buffer_.left(newline);
        buffer_.remove(0, newline + 1);
        Message message;
        QString error;
        if (!MessageProtocol::decodeLine(line, &message, &error)) {
            emit clientError(error);
            continue;
        }
        // 过期判定:响应 id 对应的发送序号落后于该请求类型的最新序号时,
        // 说明客户端已经发出过更新的同类请求,旧响应不再驱动界面
        if (seqById_.contains(message.id)) {
            const quint64 seq = seqById_.take(message.id);
            QString base = message.type;
            if (base.endsWith(QStringLiteral(".ok")) || base.endsWith(QStringLiteral(".error")))
                base = base.section(QStringLiteral("."), 0, -2);
            if (seq < latestSeqByType_.value(base, seq)) {
                emit staleResponseReceived(message);
                continue;
            }
        }
        emit responseReceived(message);
    }
}

} // namespace charging::core
