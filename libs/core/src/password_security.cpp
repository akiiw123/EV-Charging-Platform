#include "charging/core/password_security.h"
#include <QCryptographicHash>
#include <QPasswordDigestor>
#include <QRandomGenerator>
#include <QStringList>

namespace charging::core::password {
namespace {
constexpr int iterations = 120000;
constexpr int saltSize = 16;
constexpr int keySize = 32;
QByteArray derive(const QString& plainText, const QByteArray& salt, int rounds)
{
    return QPasswordDigestor::deriveKeyPbkdf2(QCryptographicHash::Sha256,
                                               plainText.toUtf8(), salt, rounds, keySize);
}
bool constantTimeEquals(const QByteArray& left, const QByteArray& right)
{
    if (left.size() != right.size()) return false;
    unsigned char difference = 0;
    for (qsizetype index = 0; index < left.size(); ++index)
        difference |= static_cast<unsigned char>(left[index] ^ right[index]);
    return difference == 0;
}
}

QString hash(const QString& plainText)
{
    QByteArray salt(saltSize, Qt::Uninitialized);
    QRandomGenerator::system()->fillRange(reinterpret_cast<quint32*>(salt.data()),
                                           saltSize / int(sizeof(quint32)));
    const QByteArray key = derive(plainText, salt, iterations);
    return QStringLiteral("PBKDF2-SHA256$%1$%2$%3")
        .arg(iterations).arg(QString::fromLatin1(salt.toHex()), QString::fromLatin1(key.toHex()));
}
bool verify(const QString& plainText, const QString& storedValue)
{
    if (storedValue.startsWith(QStringLiteral("DEV_ONLY:")))
        return storedValue.mid(9) == plainText;
    const QStringList parts = storedValue.split(QLatin1Char('$'));
    bool roundsOk = false;
    const int rounds = parts.size() == 4 ? parts[1].toInt(&roundsOk) : 0;
    const QByteArray salt = parts.size() == 4 ? QByteArray::fromHex(parts[2].toLatin1()) : QByteArray();
    const QByteArray expected = parts.size() == 4 ? QByteArray::fromHex(parts[3].toLatin1()) : QByteArray();
    if (parts.value(0) != QStringLiteral("PBKDF2-SHA256") || !roundsOk || rounds < 10000
        || salt.size() < 8 || expected.size() != keySize)
        return false;
    return constantTimeEquals(derive(plainText, salt, rounds), expected);
}
bool needsUpgrade(const QString& storedValue)
{
    return !storedValue.startsWith(QStringLiteral("PBKDF2-SHA256$"));
}
}
