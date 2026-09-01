#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>

namespace charging::core {

struct Message final {
    QString id;
    QString type;
    QJsonObject payload;
};

class MessageProtocol final {
public:
    static QByteArray encode(const Message& message);
    static bool decodeLine(const QByteArray& line, Message* message, QString* errorMessage = nullptr);
};

} // namespace charging::core
