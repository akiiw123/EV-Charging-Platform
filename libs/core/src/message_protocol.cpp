#include "charging/core/message_protocol.h"

#include <QJsonDocument>
#include <QJsonParseError>

namespace charging::core {

QByteArray MessageProtocol::encode(const Message& message)
{
    const QJsonObject object {
        {QStringLiteral("id"), message.id},
        {QStringLiteral("type"), message.type},
        {QStringLiteral("payload"), message.payload},
    };
    return QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n';
}

bool MessageProtocol::decodeLine(const QByteArray& line, Message* message, QString* errorMessage)
{
    if (!message) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("输出消息指针不能为空");
        }
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(line.trimmed(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (errorMessage) {
            *errorMessage = parseError.errorString();
        }
        return false;
    }

    const QJsonObject object = document.object();
    message->id = object.value(QStringLiteral("id")).toString();
    message->type = object.value(QStringLiteral("type")).toString();
    message->payload = object.value(QStringLiteral("payload")).toObject();
    if (message->id.isEmpty() || message->type.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("消息缺少 id 或 type");
        }
        return false;
    }
    return true;
}

} // namespace charging::core
