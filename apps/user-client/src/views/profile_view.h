#pragma once
#include <QJsonObject>
#include <QWidget>
class QLabel; class QLineEdit; class QVBoxLayout;
namespace charging::user {
class ProfileView final : public QWidget {
    Q_OBJECT
public:
    explicit ProfileView(QWidget* parent=nullptr);
    void setUser(const QJsonObject& user);
    void setHistory(const QJsonArray& orders);
signals:
    void nicknameSaveRequested(const QString& nickname);
    void rechargeRequested(double amount);
    void logoutRequested();
private:
    QLabel* avatar_=nullptr; QLabel* phone_=nullptr; QLabel* created_=nullptr; QLabel* balance_=nullptr;
    QLineEdit* nickname_=nullptr; QVBoxLayout* history_=nullptr;
};
}
