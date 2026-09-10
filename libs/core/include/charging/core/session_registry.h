#pragma once

#include <QHash>
#include <QMutex>
#include <QtGlobal>

namespace charging::core {

// 车主在线会话注册表:保证同一账号同时只在一个客户端连接上处于登录态。
//
// TcpServer 持有唯一实例并共享给所有连接线程,内部以互斥锁保护,
// 因此可以在多个连接工作线程中安全调用。
//
// 采用"后登录优先"策略:新连接登录成功即接管账号(claim 返回一枚新令牌),
// 旧连接持有的令牌随即失效,在其下一次会话复查时被服务端踢下线。
// 令牌单调递增且释放时校验归属,因此旧连接迟到的析构不会误删接管者的占用。
//
// 仅登记车主账号(auth.phone_login);管理员会话相互独立,不参与本注册表。
class SessionRegistry final {
public:
    // 占用账号并返回本次登录的会话令牌,返回值恒为非 0
    quint64 claim(qint64 userId);
    // 释放占用:仅当令牌仍是该账号的当前令牌时才移除,否则视为已被接管而忽略
    void release(qint64 userId, quint64 token);
    // 复查:token 是否仍是该账号的当前有效令牌
    bool isCurrent(qint64 userId, quint64 token) const;

private:
    mutable QMutex mutex_;
    QHash<qint64, quint64> tokenByUser_;
    quint64 nextToken_ = 0;
};

} // namespace charging::core
