#pragma once

#include "charging/core/message_protocol.h"

#include <QSqlDatabase>

namespace charging::core {

class RequestRouter final {
public:
    explicit RequestRouter(QSqlDatabase database);
    Message route(const Message& request) const;

private:
    Message success(const Message& request, const QJsonObject& payload) const;
    Message error(const Message& request, const QString& code, const QString& message) const;

    QSqlDatabase database_;
};

} // namespace charging::core
