#include "login_view.h"
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpressionValidator>
#include <QVBoxLayout>

namespace charging::user {
LoginView::LoginView(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("loginView"));
    auto* layout = new QVBoxLayout(this); layout->setContentsMargins(240,70,240,55); layout->setSpacing(16);
    auto* logo = new QLabel(QStringLiteral("⚡"), this); logo->setAlignment(Qt::AlignCenter);
    logo->setStyleSheet(QStringLiteral("font-size:64px;color:#16a085;"));
    auto* title = new QLabel(QStringLiteral("充电客户端"), this); title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(QStringLiteral("font-size:30px;font-weight:700;color:#17324d;"));
    auto* subtitle = new QLabel(QStringLiteral("便捷找桩 · 安心充电"), this); subtitle->setAlignment(Qt::AlignCenter);
    subtitle->setStyleSheet(QStringLiteral("color:#718096;font-size:15px;"));
    phone_ = new QLineEdit(this); phone_->setPlaceholderText(QStringLiteral("请输入 11 位手机号"));
    phone_->setMaxLength(11); phone_->setMinimumHeight(46);
    phone_->setValidator(new QRegularExpressionValidator(QRegularExpression(QStringLiteral("\\d{0,11}")), phone_));
    login_ = new QPushButton(QStringLiteral("登录"), this); login_->setObjectName(QStringLiteral("primaryButton")); login_->setMinimumHeight(46);
    status_ = new QLabel(QStringLiteral("正在连接服务…"), this); status_->setAlignment(Qt::AlignCenter);
    auto* demoTitle = new QLabel(QStringLiteral("测试演示账号"), this);
    demoTitle->setStyleSheet(QStringLiteral("font-weight:600;color:#334155;margin-top:16px;"));
    auto* demos = new QLabel(
        QStringLiteral("有余额  18800000001    待结算  18800000002\n"
                       "低余额  18800000003    已冻结  18800000004"), this);
    demos->setTextInteractionFlags(Qt::TextSelectableByMouse);
    demos->setStyleSheet(QStringLiteral("background:#f1f5f9;border-radius:10px;padding:14px;color:#475569;line-height:1.7;"));
    layout->addStretch(); layout->addWidget(logo); layout->addWidget(title); layout->addWidget(subtitle);
    layout->addSpacing(12); layout->addWidget(phone_); layout->addWidget(login_); layout->addWidget(status_);
    layout->addWidget(demoTitle); layout->addWidget(demos); layout->addStretch();
    connect(login_, &QPushButton::clicked, this, [this] { emit loginRequested(phone()); });
    connect(phone_, &QLineEdit::returnPressed, this, [this] { emit loginRequested(phone()); });
}
void LoginView::setConnected(bool connected) { login_->setEnabled(connected); status_->setText(connected ? QStringLiteral("服务已连接") : QStringLiteral("服务连接中…")); }
void LoginView::setMessage(const QString& message, bool error) { status_->setText(message); status_->setStyleSheet(error ? QStringLiteral("color:#dc2626;") : QStringLiteral("color:#16a085;")); }
QString LoginView::phone() const { return phone_->text().trimmed(); }
}
