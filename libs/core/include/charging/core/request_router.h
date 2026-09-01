#pragma once

#include "charging/core/message_protocol.h"

#include <QSqlDatabase>
#include <optional>

namespace charging::core {

class RequestRouter final {
public:
    explicit RequestRouter(QSqlDatabase database);
    Message route(const Message& request);

private:
    Message success(const Message& request, const QJsonObject& payload) const;
    Message error(const Message& request, const QString& code, const QString& message) const;

    QSqlDatabase database_;
    std::optional<qint64> authenticatedUserId_;
};

} // namespace charging::core
