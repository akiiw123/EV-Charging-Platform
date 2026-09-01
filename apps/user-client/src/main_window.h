#pragma once

#include "charging/core/api_client.h"

#include <QMainWindow>

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

namespace charging::user {

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    void login();
    void handleResponse(const charging::core::Message& message);
    void loadStations();

    charging::core::ApiClient api_;
    QLineEdit* phoneInput_ = nullptr;
    QPushButton* loginButton_ = nullptr;
    QLabel* userLabel_ = nullptr;
    QListWidget* stations_ = nullptr;
};

} // namespace charging::user
