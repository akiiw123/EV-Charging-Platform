// 实现一行一条 JSON 消息的编码、解码、请求 ID 和结构化错误响应。
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
