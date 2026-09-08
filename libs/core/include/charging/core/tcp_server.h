#pragma once

#include "charging/core/session_registry.h"

#include <QHostAddress>
#include <QTcpServer>
#include <QThreadPool>
#include <atomic>
#include <memory>

namespace charging::core {

class TcpServer final : public QTcpServer {
    Q_OBJECT

public:
    explicit TcpServer(QString databasePath, QObject* parent = nullptr);
    ~TcpServer() override;

    bool start(const QHostAddress& address = QHostAddress::Any, quint16 port = 45454,
               QString* errorMessage = nullptr);

protected:
    void incomingConnection(qintptr socketDescriptor) override;

private:
    QString databasePath_;
    QThreadPool pool_;
    std::shared_ptr<std::atomic_bool> stopping_;
    // 全部连接线程共享的车主在线会话表,用于限制同一账号只在一个客户端登录
    std::shared_ptr<SessionRegistry> sessions_;
};

} // namespace charging::core
