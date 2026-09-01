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
        emit responseReceived(message);
    }
}

} // namespace charging::core
