#pragma once

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
};

} // namespace charging::core
