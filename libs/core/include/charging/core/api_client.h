#pragma once

#include "charging/core/message_protocol.h"

#include <QObject>
#include <QTcpSocket>
#include <QTimer>

namespace charging::core {

class ApiClient final : public QObject {
    Q_OBJECT

public:
    explicit ApiClient(QObject* parent = nullptr);

    void connectToServer(const QString& host, quint16 port);
    QString send(const QString& type, const QJsonObject& payload = {});
    bool isConnected() const;

signals:
    void connected();
    void disconnected();
    void responseReceived(const charging::core::Message& message);
    void clientError(const QString& message);

private slots:
    void reconnect();
    void readAvailable();

private:
    QTcpSocket socket_;
    QTimer reconnectTimer_;
    QByteArray buffer_;
    QString host_;
    quint16 port_ = 0;
};

} // namespace charging::core

Q_DECLARE_METATYPE(charging::core::Message)
