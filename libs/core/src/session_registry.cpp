#include "charging/core/session_registry.h"

#include <QMutexLocker>

namespace charging::core {

quint64 SessionRegistry::claim(qint64 userId)
{
    QMutexLocker locker(&mutex_);
    // 令牌从 1 开始单调递增,0 保留为"未持有令牌"的空值
    const quint64 token = ++nextToken_;
    tokenByUser_.insert(userId, token);
    return token;
}

void SessionRegistry::release(qint64 userId, quint64 token)
{
    if (token == 0) {
        return;
    }
    QMutexLocker locker(&mutex_);
    const auto found = tokenByUser_.find(userId);
    // 令牌不匹配说明账号已被其他连接接管,此时不得清除接管者的占用
    if (found != tokenByUser_.end() && found.value() == token) {
        tokenByUser_.erase(found);
    }
}

bool SessionRegistry::isCurrent(qint64 userId, quint64 token) const
{
    if (token == 0) {
        return false;
    }
    QMutexLocker locker(&mutex_);
    return tokenByUser_.value(userId) == token;
}

} // namespace charging::core
