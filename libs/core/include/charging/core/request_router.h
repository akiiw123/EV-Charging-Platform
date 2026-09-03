#pragma once

#include "charging/core/message_protocol.h"

#include <QSqlDatabase>
#include <optional>

namespace charging::core {

class RequestRouter final {
public:
    explicit RequestRouter(QSqlDatabase database);
    Message route(const Message& request);

    // C4 冻结踢会话:查询当前会话用户是否已被冻结;被冻结则标记会话关闭并返回 true
    bool refreshSession();
    bool sessionClosed() const { return sessionClosed_; }

private:
    Message success(const Message& request, const QJsonObject& payload) const;
    Message error(const Message& request, const QString& code, const QString& message) const;
    void expireStaleReservations();
    bool sessionClosed_ = false;

    QSqlDatabase database_;
    std::optional<qint64> authenticatedUserId_;
    std::optional<qint64> authenticatedAdminId_;
};

} // namespace charging::core
