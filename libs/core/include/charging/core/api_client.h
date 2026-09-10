#pragma once

#include "charging/core/message_protocol.h"

#include <QHash>
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
    // 正常响应;同类型的过期响应(其请求已被更新的同类请求取代)只发 stale 信号,
    // 订阅方应仅用它做收尾(如 busy 计数),不要更新界面状态
    void responseReceived(const charging::core::Message& message);
    void staleResponseReceived(const charging::core::Message& message);
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
    // 请求 ID -> 发送序号;按请求类型记录最新序号,用于丢弃过期响应
    QHash<QString, quint64> seqById_;
    QHash<QString, quint64> latestSeqByType_;
    quint64 sendCounter_ = 0;
};

} // namespace charging::core

Q_DECLARE_METATYPE(charging::core::Message)
