#pragma once
#include <QString>
namespace charging::core::password {
QString hash(const QString& plainText);
bool verify(const QString& plainText, const QString& storedValue);
bool needsUpgrade(const QString& storedValue);
}
