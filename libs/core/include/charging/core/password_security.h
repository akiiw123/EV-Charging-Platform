// 实现管理员密码 PBKDF2 哈希、校验及旧口令升级。
#pragma once
#include <QString>
namespace charging::core::password {
QString hash(const QString& plainText);
bool verify(const QString& plainText, const QString& storedValue);
bool needsUpgrade(const QString& storedValue);
}
