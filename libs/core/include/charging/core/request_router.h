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
    // 数据库错误统一出口:原始错误只写服务端日志,客户端仅收到通用可读提示
    Message storageError(const Message& request, const QString& context,
                         const QString& detail) const;
    void expireStaleReservations();
    bool sessionClosed_ = false;

    QSqlDatabase database_;
    std::optional<qint64> authenticatedUserId_;
    std::optional<qint64> authenticatedAdminId_;
};

} // namespace charging::core
