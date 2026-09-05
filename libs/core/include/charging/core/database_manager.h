#pragma once

#include <QSqlDatabase>
#include <QString>

namespace charging::core {

class DatabaseManager final {
public:
    explicit DatabaseManager(QString connectionName = QStringLiteral("charging-main"));
    ~DatabaseManager();

    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    bool open(const QString& databasePath, QString* errorMessage = nullptr);
    bool initialize(QString* errorMessage = nullptr);
    bool migrate(QString* errorMessage);
    bool isOpen() const;
    QSqlDatabase database() const;

private:
    bool executeScript(const QString& resourceOrFilePath, QString* errorMessage);

    QString connectionName_;
    QSqlDatabase database_;
};

} // namespace charging::core
