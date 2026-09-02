#pragma once
#include <QWidget>
class QLabel; class QLineEdit; class QPushButton;
namespace charging::user {
class LoginView final : public QWidget {
    Q_OBJECT
public:
    explicit LoginView(QWidget* parent = nullptr);
    void setConnected(bool connected);
    void setMessage(const QString& message, bool error = false);
    QString phone() const;
signals:
    void loginRequested(const QString& phone);
private:
    QLineEdit* phone_ = nullptr;
    QPushButton* login_ = nullptr;
    QLabel* status_ = nullptr;
};
}
