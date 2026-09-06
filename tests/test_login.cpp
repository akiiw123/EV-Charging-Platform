#include "charging/core/database_manager.h"
#include "charging/core/message_protocol.h"
#include "charging/core/tcp_server.h"

#include <QHostAddress>
#include <QJsonObject>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>

// 登录测试：非法手机号、首次注册、正常登录、冻结用户禁止登录、在线冻结后断开
class LoginTest final : public QObject {
    Q_OBJECT

private slots:
    // 非法手机号：格式不正确时返回 AUTH_INVALID_PHONE
    void invalidPhoneRejected()
    {
        Fixture fixture;
        QTcpSocket socket;
        socket.connectToHost(QHostAddress::LocalHost, fixture.port());
        QVERIFY(socket.waitForConnected(3000));
        QVERIFY(fixture.acceptConnection());

        const QStringList badPhones {
            QStringLiteral(""),             // 空字符串
            QStringLiteral("123"),          // 太短
            QStringLiteral("1380013800"),   // 10 位
            QStringLiteral("138001380001"), // 12 位
            QStringLiteral("23800138000"),  // 不以 1 开头
            QStringLiteral("1380013800a"),  // 含字母
            QStringLiteral("abcdefghijk"),  // 全字母
        };
        for (const auto& phone : badPhones) {
            const auto response = exchange(socket, {
                QStringLiteral("login-bad-%1").arg(phone.size()),
                QStringLiteral("auth.phone_login"),
                {{QStringLiteral("phone"), phone}}});
            QCOMPARE(response.type, QStringLiteral("auth.phone_login.error"));
            QCOMPARE(response.payload.value(QStringLiteral("code")).toString(),
                     QStringLiteral("AUTH_INVALID_PHONE"));
        }
        socket.disconnectFromHost();
        socket.waitForDisconnected(1000);
    }

    // 首次注册：新手机号自动创建用户，昵称格式为"用户XXXX"，余额为 0
    void firstRegistrationCreatesUser()
    {
        Fixture fixture;
        QTcpSocket socket;
        socket.connectToHost(QHostAddress::LocalHost, fixture.port());
        QVERIFY(socket.waitForConnected(3000));
        QVERIFY(fixture.acceptConnection());

        const QString newPhone = QStringLiteral("13912345678");
        const auto response = exchange(socket, {
            QStringLiteral("register"),
            QStringLiteral("auth.phone_login"),
            {{QStringLiteral("phone"), newPhone}}});
        QCOMPARE(response.type, QStringLiteral("auth.phone_login.ok"));

        const QJsonObject user = response.payload.value(QStringLiteral("user")).toObject();
        QCOMPARE(user.value(QStringLiteral("phone")).toString(), newPhone);
        QCOMPARE(user.value(QStringLiteral("nickname")).toString(), QStringLiteral("用户5678"));
        QCOMPARE(user.value(QStringLiteral("status")).toString(), QStringLiteral("active"));
        QCOMPARE(user.value(QStringLiteral("wallet_balance")).toDouble(), 0.0);
        QVERIFY(user.value(QStringLiteral("id")).toInteger() > 0);

        // 再次登录同一手机号，应返回同一用户（不重复创建）
        const auto second = exchange(socket, {
            QStringLiteral("login-again"),
            QStringLiteral("auth.phone_login"),
            {{QStringLiteral("phone"), newPhone}}});
        QCOMPARE(second.type, QStringLiteral("auth.phone_login.ok"));
        QCOMPARE(second.payload.value(QStringLiteral("user")).toObject()
                     .value(QStringLiteral("id")).toInteger(),
                 user.value(QStringLiteral("id")).toInteger());

        socket.disconnectFromHost();
        socket.waitForDisconnected(1000);
    }

    // 正常登录：seed 中已有用户（18800000001，余额 200）登录成功
    void normalLoginReturnsExistingUser()
    {
        Fixture fixture;
        QTcpSocket socket;
        socket.connectToHost(QHostAddress::LocalHost, fixture.port());
        QVERIFY(socket.waitForConnected(3000));
        QVERIFY(fixture.acceptConnection());

        const auto response = exchange(socket, {
            QStringLiteral("login"),
            QStringLiteral("auth.phone_login"),
            {{QStringLiteral("phone"), QStringLiteral("18800000001")}}});
        QCOMPARE(response.type, QStringLiteral("auth.phone_login.ok"));

        const QJsonObject user = response.payload.value(QStringLiteral("user")).toObject();
        QCOMPARE(user.value(QStringLiteral("phone")).toString(), QStringLiteral("18800000001"));
        QCOMPARE(user.value(QStringLiteral("nickname")).toString(), QStringLiteral("余额充足用户"));
        QCOMPARE(user.value(QStringLiteral("wallet_balance")).toDouble(), 200.0);
        QCOMPARE(user.value(QStringLiteral("status")).toString(), QStringLiteral("active"));

        socket.disconnectFromHost();
        socket.waitForDisconnected(1000);
    }

    // 冻结用户禁止登录：seed 中 18800000004 状态为 frozen，登录返回 AUTH_USER_FROZEN
    void frozenUserCannotLogin()
    {
        Fixture fixture;
        QTcpSocket socket;
        socket.connectToHost(QHostAddress::LocalHost, fixture.port());
        QVERIFY(socket.waitForConnected(3000));
        QVERIFY(fixture.acceptConnection());

        const auto response = exchange(socket, {
            QStringLiteral("login-frozen"),
            QStringLiteral("auth.phone_login"),
            {{QStringLiteral("phone"), QStringLiteral("18800000004")}}});
        QCOMPARE(response.type, QStringLiteral("auth.phone_login.error"));
        QCOMPARE(response.payload.value(QStringLiteral("code")).toString(),
                 QStringLiteral("AUTH_USER_FROZEN"));
        QVERIFY(response.payload.value(QStringLiteral("message")).toString()
                    .contains(QStringLiteral("冻结")));

        socket.disconnectFromHost();
        socket.waitForDisconnected(1000);
    }

    // 在线冻结后断开：用户登录后被管理员冻结，下一个请求返回 AUTH_USER_FROZEN 并断开连接
    void frozenWhileOnlineDisconnectedOnNextRequest()
    {
        Fixture fixture;

        QTcpSocket user;
        user.connectToHost(QHostAddress::LocalHost, fixture.port());
        QVERIFY(user.waitForConnected(3000));
        QVERIFY(fixture.acceptConnection());
        const auto login = exchange(user, {
            QStringLiteral("login"),
            QStringLiteral("auth.phone_login"),
            {{QStringLiteral("phone"), QStringLiteral("18800000001")}}});
        QCOMPARE(login.type, QStringLiteral("auth.phone_login.ok"));
        const qint64 userId = login.payload.value(QStringLiteral("user")).toObject()
                                  .value(QStringLiteral("id")).toInteger();

        QTcpSocket admin;
        admin.connectToHost(QHostAddress::LocalHost, fixture.port());
        QVERIFY(admin.waitForConnected(3000));
        QVERIFY(fixture.acceptConnection());
        QCOMPARE(exchange(admin, {
            QStringLiteral("alogin"),
            QStringLiteral("admin.login"),
            {{QStringLiteral("username"), QStringLiteral("admin")},
             {QStringLiteral("password"), QStringLiteral("123456")}}}).type,
                 QStringLiteral("admin.login.ok"));
        QCOMPARE(exchange(admin, {
            QStringLiteral("freeze"),
            QStringLiteral("admin.user.status"),
            {{QStringLiteral("user_id"), userId},
             {QStringLiteral("status"), QStringLiteral("frozen")}}}).type,
                 QStringLiteral("admin.user.status.ok"));

        // 用户下一个请求被拒绝，返回 AUTH_USER_FROZEN
        const auto denied = exchange(user, {
            QStringLiteral("profile"),
            QStringLiteral("user.profile"),
            {}});
        QCOMPARE(denied.type, QStringLiteral("user.profile.error"));
        QCOMPARE(denied.payload.value(QStringLiteral("code")).toString(),
                 QStringLiteral("AUTH_USER_FROZEN"));

        // 连接被服务端关闭
        QTRY_COMPARE_WITH_TIMEOUT(user.state(), QAbstractSocket::UnconnectedState, 3000);

        admin.disconnectFromHost();
        admin.waitForDisconnected(1000);
    }

private:
    class Fixture {
    public:
        Fixture()
            : manager_(QStringLiteral("login-fixture-") + QUuid::createUuid().toString()),
              server_(directory_.filePath(QStringLiteral("test.db")))
        {
            QString error;
            QVERIFY(directory_.isValid());
            QVERIFY2(manager_.open(directory_.filePath(QStringLiteral("test.db")), &error),
                     qPrintable(error));
            QVERIFY2(manager_.initialize(&error), qPrintable(error));
            QVERIFY2(server_.start(QHostAddress::LocalHost, 0, &error), qPrintable(error));
        }
        quint16 port() const { return server_.serverPort(); }
        bool acceptConnection() { return server_.waitForNewConnection(1000); }
        QSqlDatabase database() const { return manager_.database(); }
    private:
        QTemporaryDir directory_;
        charging::core::DatabaseManager manager_;
        charging::core::TcpServer server_;
    };

    static charging::core::Message exchange(QTcpSocket& socket,
                                             const charging::core::Message& request)
    {
        socket.write(charging::core::MessageProtocol::encode(request));
        if (!socket.waitForBytesWritten(1000) || !socket.waitForReadyRead(3000)) {
            QTest::qFail("等待服务端响应超时", __FILE__, __LINE__);
            return {};
        }
        charging::core::Message response;
        QString error;
        if (!charging::core::MessageProtocol::decodeLine(socket.readLine(), &response, &error)) {
            QTest::qFail(qPrintable(error), __FILE__, __LINE__);
            return {};
        }
        return response;
    }
};

QTEST_GUILESS_MAIN(LoginTest)
#include "test_login.moc"
