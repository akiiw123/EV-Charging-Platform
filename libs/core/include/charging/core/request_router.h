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
    void expireStaleReservations();

    QSqlDatabase database_;
    std::optional<qint64> authenticatedUserId_;
    std::optional<qint64> authenticatedAdminId_;
};

} // namespace charging::core
