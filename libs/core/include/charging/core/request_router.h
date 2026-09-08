// 校验会话和参数并把协议请求路由到仓储及业务规则，是服务端业务编排中心。
#pragma once

#include "charging/core/message_protocol.h"
#include "charging/core/session_registry.h"

#include <QSqlDatabase>
#include <memory>
#include <optional>

namespace charging::core {

class RequestRouter final {
public:
    // 会话被服务端关闭的原因:连接层据此选择 server.session.closed 的错误码与文案
    enum class SessionCloseReason { None, UserFrozen, TakenOver };

    // sessions 为空时不启用单点登录限制,便于单独构造路由器做单元测试
    explicit RequestRouter(QSqlDatabase database,
                           std::shared_ptr<SessionRegistry> sessions = nullptr);
    // 连接结束时释放本连接对账号的占用,保证该账号可立即在新连接上重新登录
    ~RequestRouter();
    Message route(const Message& request);

    // C4 冻结踢会话:查询当前会话用户是否已被冻结;被冻结则标记会话关闭并返回 true
    bool refreshSession();
    // 单点登录复查:本连接的会话令牌是否已被其他客户端接管;
    // 被接管则标记会话关闭并返回 true。纯内存判断,可由连接层高频轮询。
    bool checkSessionOwnership();
    bool sessionClosed() const { return sessionCloseReason_ != SessionCloseReason::None; }
    SessionCloseReason sessionCloseReason() const { return sessionCloseReason_; }

private:
    Message success(const Message& request, const QJsonObject& payload) const;
    Message error(const Message& request, const QString& code, const QString& message) const;
    // 数据库错误统一出口:原始错误只写服务端日志,客户端仅收到通用可读提示
    Message storageError(const Message& request, const QString& context,
                         const QString& detail) const;
    void expireStaleReservations();
    // 结束会话:登记关闭原因并释放账号占用(已被接管时释放自动失效)
    void closeSession(SessionCloseReason reason);

    QSqlDatabase database_;
    std::shared_ptr<SessionRegistry> sessions_;
    std::optional<qint64> authenticatedUserId_;
    std::optional<qint64> authenticatedAdminId_;
    // 本连接在 SessionRegistry 中持有的令牌,未登录或未启用限制时为空
    std::optional<quint64> sessionToken_;
    SessionCloseReason sessionCloseReason_ = SessionCloseReason::None;
};

} // namespace charging::core
