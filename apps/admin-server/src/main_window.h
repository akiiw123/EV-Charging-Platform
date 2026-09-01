#pragma once

#include "charging/core/api_client.h"
#include <QJsonArray>
#include <QMainWindow>
#include <QStringList>

class QLabel; class QLineEdit; class QPushButton; class QStackedWidget; class QTableWidget;
class QChartView;

namespace charging::admin {

class MainWindow final : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    void login();
    void refreshAll();
    void handleResponse(const charging::core::Message& message);
    void populateTable(QTableWidget* table, const QJsonArray& rows,
                       const QStringList& keys);

    charging::core::ApiClient api_;
    QStackedWidget* pages_ = nullptr;
    QLineEdit* username_ = nullptr;
    QLineEdit* password_ = nullptr;
    QLabel* loginStatus_ = nullptr;
    QLabel* revenueSummary_ = nullptr;
    QLabel* pileSummary_ = nullptr;
    QChartView* revenueChart_ = nullptr;
    QTableWidget* stationTable_ = nullptr;
    QLineEdit* stationName_ = nullptr;
    QLineEdit* stationAddress_ = nullptr;
    QLineEdit* latitude_ = nullptr;
    QLineEdit* longitude_ = nullptr;
    QLineEdit* price_ = nullptr;
    QLineEdit* pileCount_ = nullptr;
    QTableWidget* pileTable_ = nullptr;
    QLineEdit* userSearch_ = nullptr;
    QTableWidget* userTable_ = nullptr;
};

} // namespace charging::admin
