#pragma once
#include <QDateTime>
#include <QRegularExpression>
#include <QString>

namespace charging::core {
inline QDateTime parseTimestamp(const QString& value) {
    QDateTime date = QDateTime::fromString(value, Qt::ISODate);
    if (!date.isValid()) date = QDateTime::fromString(value, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    static const QRegularExpression zone(QStringLiteral("(Z|[+-]\\d{2}:?\\d{2})$"), QRegularExpression::CaseInsensitiveOption);
    if (date.isValid() && !zone.match(value).hasMatch()) date.setTimeSpec(Qt::UTC);
    return date;
}
inline QString beijingTime(const QString& value) {
    const auto date = parseTimestamp(value);
    return date.isValid() ? date.toOffsetFromUtc(8 * 3600).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) : QStringLiteral("—");
}
}
