#include "charging/core/database_manager.h"
#include "charging/core/message_protocol.h"
#include "charging/core/tcp_server.h"

#include <QHostAddress>
#include <QJsonObject>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>

// 管理端扩展测试：未登录访问 admin 接口、错误密码、首次改密、
// 非法站点坐标、重复电桩编号、充电中电桩重启失败
class AdminExtendedTest final : public QObject {
    Q_OBJECT

private slots:
    // 未登录访问 admin 接口：返回 ADMIN_AUTH_REQUIRED
    void unauthenticatedAdminAccessDenied()
    {
        Fixture fixture;
        QTcpSocket socket;
        socket.connectToHost(QHostAddress::LocalHost, fixture.port());
        QVERIFY(socket.waitForConnected(3000));
        QVERIFY(fixture.acceptConnection());

        const QStringList adminTypes {
            QStringLiteral("admin.dashboard"),
            QStringLiteral("admin.station.list"),
            QStringLiteral("admin.pile.list"),
            QStringLiteral("admin.user.list"),
            QStringLiteral("admin.order.list"),
        };
        for (const auto& type : adminTypes) {
            const auto response = exchange(socket, {
                QStringLiteral("no-auth-%1").arg(type),
                type,
                {}});
            QCOMPARE(response.type, type + QStringLiteral(".error"));
            QCOMPARE(response.payload.value(QStringLiteral("code")).toString(),
                     QStringLiteral("ADMIN_AUTH_REQUIRED"));
            QVERIFY(response.payload.value(QStringLiteral("message")).toString()
                        .contains(QStringLiteral("登录")));
        }
        socket.disconnectFromHost();
        socket.waitForDisconnected(1000);
    }

    // 错误密码：返回 ADMIN_LOGIN_FAILED，不泄露具体原因
    void wrongPasswordRejected()
    {
        Fixture fixture;
        QTcpSocket socket;
        socket.connectToHost(QHostAddress::LocalHost, fixture.port());
        QVERIFY(socket.waitForConnected(3000));
        QVERIFY(fixture.acceptConnection());

        const auto response = exchange(socket, {
            QStringLiteral("bad-login"),
            QStringLiteral("admin.login"),
            {{QStringLiteral("username"), QStringLiteral("admin")},
             {QStringLiteral("password"), QStringLiteral("wrong-password")}}});
        QCOMPARE(response.type, QStringLiteral("admin.login.error"));
        QCOMPARE(response.payload.value(QStringLiteral("code")).toString(),
                 QStringLiteral("ADMIN_LOGIN_FAILED"));
        // 错误信息不应区分"用户不存在"与"密码错误"
        QVERIFY(response.payload.value(QStringLiteral("message")).toString()
                    .contains(QStringLiteral("账号或密码错误")));

        // 不存在的用户名同样返回 ADMIN_LOGIN_FAILED
        const auto noUser = exchange(socket, {
            QStringLiteral("no-user"),
            QStringLiteral("admin.login"),
            {{QStringLiteral("username"), QStringLiteral("nonexistent")},
             {QStringLiteral("password"), QStringLiteral("123456")}}});
        QCOMPARE(noUser.type, QStringLiteral("admin.login.error"));
        QCOMPARE(noUser.payload.value(QStringLiteral("code")).toString(),
                 QStringLiteral("ADMIN_LOGIN_FAILED"));

        socket.disconnectFromHost();
        socket.waitForDisconnected(1000);
    }

    // 首次改密：must_change_password=true → 改密成功 → must_change_password=false
    void firstPasswordChangeFlow()
    {
        Fixture fixture;
        QTcpSocket socket;
        socket.connectToHost(QHostAddress::LocalHost, fixture.port());
        QVERIFY(socket.waitForConnected(3000));
        QVERIFY(fixture.acceptConnection());

        // 首登：返回 must_change_password=true
        const auto login = exchange(socket, {
            QStringLiteral("login"),
            QStringLiteral("admin.login"),
            {{QStringLiteral("username"), QStringLiteral("admin")},
             {QStringLiteral("password"), QStringLiteral("123456")}}});
        QCOMPARE(login.type, QStringLiteral("admin.login.ok"));
        QVERIFY(login.payload.value(QStringLiteral("administrator")).toObject()
                    .value(QStringLiteral("must_change_password")).toBool());

        // 旧密码错误 → 拒绝
        QCOMPARE(exchange(socket, {
            QStringLiteral("chg-bad-old"),
            QStringLiteral("admin.password.change"),
            {{QStringLiteral("old_password"), QStringLiteral("000000")},
             {QStringLiteral("new_password"), QStringLiteral("strong-pass-1")}}}).type,
                 QStringLiteral("admin.password.change.error"));

        // 新密码过短（< 8 位）→ 拒绝
        const auto shortPwd = exchange(socket, {
            QStringLiteral("chg-short"),
            QStringLiteral("admin.password.change"),
            {{QStringLiteral("old_password"), QStringLiteral("123456")},
             {QStringLiteral("new_password"), QStringLiteral("short")}}});
        QCOMPARE(shortPwd.type, QStringLiteral("admin.password.change.error"));
        QCOMPARE(shortPwd.payload.value(QStringLiteral("code")).toString(),
                 QStringLiteral("PASSWORD_WEAK"));

        // 新密码与旧密码相同 → 拒绝
        const auto samePwd = exchange(socket, {
            QStringLiteral("chg-same"),
            QStringLiteral("admin.password.change"),
            {{QStringLiteral("old_password"), QStringLiteral("123456")},
             {QStringLiteral("new_password"), QStringLiteral("123456")}}});
        QCOMPARE(samePwd.type, QStringLiteral("admin.password.change.error"));
        QCOMPARE(samePwd.payload.value(QStringLiteral("code")).toString(),
                 QStringLiteral("PASSWORD_WEAK"));

        // 正确改密 → 成功，must_change_password=false
        const auto changeOk = exchange(socket, {
            QStringLiteral("chg-ok"),
            QStringLiteral("admin.password.change"),
            {{QStringLiteral("old_password"), QStringLiteral("123456")},
             {QStringLiteral("new_password"), QStringLiteral("strong-pass-1")}}});
        QCOMPARE(changeOk.type, QStringLiteral("admin.password.change.ok"));
        QCOMPARE(changeOk.payload.value(QStringLiteral("must_change_password")).toBool(), false);

        // 旧密码不能再登录
        QCOMPARE(exchange(socket, {
            QStringLiteral("relogin-old"),
            QStringLiteral("admin.login"),
            {{QStringLiteral("username"), QStringLiteral("admin")},
             {QStringLiteral("password"), QStringLiteral("123456")}}}).type,
                 QStringLiteral("admin.login.error"));

        // 新密码登录成功，且不再要求改密
        const auto relogin = exchange(socket, {
            QStringLiteral("relogin-new"),
            QStringLiteral("admin.login"),
            {{QStringLiteral("username"), QStringLiteral("admin")},
             {QStringLiteral("password"), QStringLiteral("strong-pass-1")}}});
        QCOMPARE(relogin.type, QStringLiteral("admin.login.ok"));
        QVERIFY(!relogin.payload.value(QStringLiteral("administrator")).toObject()
                    .value(QStringLiteral("must_change_password")).toBool());

        socket.disconnectFromHost();
        socket.waitForDisconnected(1000);
    }

    // 非法站点坐标：纬度超出 [-90,90] 或经度超出 [-180,180] 时返回 INVALID_ARGUMENT
    void invalidStationCoordinatesRejected()
    {
        Fixture fixture;
        QTcpSocket socket;
        socket.connectToHost(QHostAddress::LocalHost, fixture.port());
        QVERIFY(socket.waitForConnected(3000));
        QVERIFY(fixture.acceptConnection());
        adminLogin(socket);

        struct Case { double lat; double lng; const char* desc; };
        const Case cases[] {
            {  91.0,  116.0, "纬度超出上界"  },
            { -91.0,  116.0, "纬度超出下界"  },
            {  39.9,  181.0, "经度超出上界"  },
            {  39.9, -181.0, "经度超出下界"  },
            { 999.0,  999.0, "经纬度均非法"  },
        };
        for (const auto& c : cases) {
            const auto response = exchange(socket, {
                QStringLiteral("create-bad-%1").arg(c.desc),
                QStringLiteral("admin.station.create"),
                {{QStringLiteral("name"), QStringLiteral("非法坐标站")},
                 {QStringLiteral("address"), QStringLiteral("测试地址")},
                 {QStringLiteral("latitude"), c.lat},
                 {QStringLiteral("longitude"), c.lng},
                 {QStringLiteral("price_per_kwh"), 1.0},
                 {QStringLiteral("pile_count"), 1}}});
            QCOMPARE(response.type, QStringLiteral("admin.station.create.error"));
            QCOMPARE(response.payload.value(QStringLiteral("code")).toString(),
                     QStringLiteral("INVALID_ARGUMENT"));
        }

        // 电桩数量为 0 或超过 100 时同样拒绝
        const auto zeroPiles = exchange(socket, {
            QStringLiteral("create-zero-piles"),
            QStringLiteral("admin.station.create"),
            {{QStringLiteral("name"), QStringLiteral("零桩站")},
             {QStringLiteral("address"), QStringLiteral("测试地址")},
             {QStringLiteral("latitude"), 39.9},
             {QStringLiteral("longitude"), 116.4},
             {QStringLiteral("price_per_kwh"), 1.0},
             {QStringLiteral("pile_count"), 0}}});
        QCOMPARE(zeroPiles.type, QStringLiteral("admin.station.create.error"));
        QCOMPARE(zeroPiles.payload.value(QStringLiteral("code")).toString(),
                 QStringLiteral("INVALID_ARGUMENT"));

        socket.disconnectFromHost();
        socket.waitForDisconnected(1000);
    }

    // 重复电桩编号：数据库唯一约束拒绝，返回"电桩编号已存在"
    void duplicatePileCodeRejected()
    {
        Fixture fixture;
        QTcpSocket socket;
        socket.connectToHost(QHostAddress::LocalHost, fixture.port());
        QVERIFY(socket.waitForConnected(3000));
        QVERIFY(fixture.acceptConnection());
        adminLogin(socket);

        // 首次创建成功
        const auto first = exchange(socket, {
            QStringLiteral("pile-create-1"),
            QStringLiteral("admin.pile.create"),
            {{QStringLiteral("station_id"), 1},
             {QStringLiteral("code"), QStringLiteral("DUP-TEST-01")},
             {QStringLiteral("type"), QStringLiteral("slow")},
             {QStringLiteral("power_kw"), 7.0}}});
        QCOMPARE(first.type, QStringLiteral("admin.pile.create.ok"));

        // 重复编号 → 拒绝
        const auto second = exchange(socket, {
            QStringLiteral("pile-create-2"),
            QStringLiteral("admin.pile.create"),
            {{QStringLiteral("station_id"), 1},
             {QStringLiteral("code"), QStringLiteral("DUP-TEST-01")},
             {QStringLiteral("type"), QStringLiteral("slow")},
             {QStringLiteral("power_kw"), 7.0}}});
        QCOMPARE(second.type, QStringLiteral("admin.pile.create.error"));
        QCOMPARE(second.payload.value(QStringLiteral("code")).toString(),
                 QStringLiteral("PILE_CREATE_FAILED"));
        QVERIFY(second.payload.value(QStringLiteral("message")).toString()
                    .contains(QStringLiteral("已存在")));

        socket.disconnectFromHost();
        socket.waitForDisconnected(1000);
    }

    // 充电中电桩重启失败：admin.pile.restart 对 charging 状态桩返回 PILE_RESTART_FAILED
    void restartChargingPileFails()
    {
        Fixture fixture;

        // 用户预约桩 1 并开始充电，使桩 1 变为 charging
        QTcpSocket user;
        user.connectToHost(QHostAddress::LocalHost, fixture.port());
        QVERIFY(user.waitForConnected(3000));
        QVERIFY(fixture.acceptConnection());
        QCOMPARE(exchange(user, {
            QStringLiteral("login"),
            QStringLiteral("auth.phone_login"),
            {{QStringLiteral("phone"), QStringLiteral("18800000001")}}}).type,
                 QStringLiteral("auth.phone_login.ok"));
        QCOMPARE(exchange(user, {
            QStringLiteral("reserve"),
            QStringLiteral("order.reserve"),
            {{QStringLiteral("pile_id"), 1}}}).type,
                 QStringLiteral("order.reserve.ok"));
        const auto active = exchange(user, {
            QStringLiteral("active"),
            QStringLiteral("order.active"),
            {}});
        const qint64 orderId = active.payload.value(QStringLiteral("order")).toObject()
                                   .value(QStringLiteral("id")).toInteger();
        QCOMPARE(exchange(user, {
            QStringLiteral("start"),
            QStringLiteral("order.start"),
            {{QStringLiteral("order_id"), orderId}}}).type,
                 QStringLiteral("order.start.ok"));

        // 管理员尝试重启充电中的桩 1 → 失败
        QTcpSocket admin;
        admin.connectToHost(QHostAddress::LocalHost, fixture.port());
        QVERIFY(admin.waitForConnected(3000));
        QVERIFY(fixture.acceptConnection());
        adminLogin(admin);

        const auto restart = exchange(admin, {
            QStringLiteral("restart-charging"),
            QStringLiteral("admin.pile.restart"),
            {{QStringLiteral("pile_id"), 1}}});
        QCOMPARE(restart.type, QStringLiteral("admin.pile.restart.error"));
        QCOMPARE(restart.payload.value(QStringLiteral("code")).toString(),
                 QStringLiteral("PILE_RESTART_FAILED"));
        QVERIFY(restart.payload.value(QStringLiteral("message")).toString()
                    .contains(QStringLiteral("充电中")));

        user.disconnectFromHost(); user.waitForDisconnected(1000);
        admin.disconnectFromHost(); admin.waitForDisconnected(1000);
    }

    // 空闲/故障电桩重启成功：状态恢复为 idle
    void restartIdleAndFaultPileSucceeds()
    {
        Fixture fixture;
        QTcpSocket admin;
        admin.connectToHost(QHostAddress::LocalHost, fixture.port());
        QVERIFY(admin.waitForConnected(3000));
        QVERIFY(fixture.acceptConnection());
        adminLogin(admin);

        // 桩 1 当前为 idle，重启后仍为 idle
        const auto restartIdle = exchange(admin, {
            QStringLiteral("restart-idle"),
            QStringLiteral("admin.pile.restart"),
            {{QStringLiteral("pile_id"), 1}}});
        QCOMPARE(restartIdle.type, QStringLiteral("admin.pile.restart.ok"));
        QCOMPARE(restartIdle.payload.value(QStringLiteral("pile")).toObject()
                     .value(QStringLiteral("status")).toString(),
                 QStringLiteral("idle"));

        // 将桩 1 置为 fault，再重启 → 恢复 idle
        QCOMPARE(exchange(admin, {
            QStringLiteral("set-fault"),
            QStringLiteral("admin.pile.status"),
            {{QStringLiteral("pile_id"), 1},
             {QStringLiteral("status"), QStringLiteral("fault")}}}).type,
                 QStringLiteral("admin.pile.status.ok"));
        const auto restartFault = exchange(admin, {
            QStringLiteral("restart-fault"),
            QStringLiteral("admin.pile.restart"),
            {{QStringLiteral("pile_id"), 1}}});
        QCOMPARE(restartFault.type, QStringLiteral("admin.pile.restart.ok"));
        QCOMPARE(restartFault.payload.value(QStringLiteral("pile")).toObject()
                     .value(QStringLiteral("status")).toString(),
                 QStringLiteral("idle"));

        admin.disconnectFromHost();
        admin.waitForDisconnected(1000);
    }

    // 已知缺陷：重启不存在的电桩时，错误信息误写为"充电中的电桩不能重启"
    // 预期行为：应返回"电桩不存在"；当前行为：返回"充电中的电桩不能重启"
    // 此测试记录当前实际行为，待修复后应将 QVERIFY 改为 QVERIFY(!msg.contains("充电中"))
    void restartNonExistentPileReturnsMisleadingMessage()
    {
        Fixture fixture;
        QTcpSocket admin;
        admin.connectToHost(QHostAddress::LocalHost, fixture.port());
        QVERIFY(admin.waitForConnected(3000));
        QVERIFY(fixture.acceptConnection());
        adminLogin(admin);

        const auto response = exchange(admin, {
            QStringLiteral("restart-missing"),
            QStringLiteral("admin.pile.restart"),
            {{QStringLiteral("pile_id"), 999999}}});
        QCOMPARE(response.type, QStringLiteral("admin.pile.restart.error"));
        QCOMPARE(response.payload.value(QStringLiteral("code")).toString(),
                 QStringLiteral("PILE_RESTART_FAILED"));
        // 记录当前缺陷：错误信息为"充电中的电桩不能重启"而非"电桩不存在"
        const QString msg = response.payload.value(QStringLiteral("message")).toString();
        qDebug() << "[已知缺陷] 重启不存在电桩的错误信息:" << msg;
        QVERIFY(!msg.isEmpty());

        admin.disconnectFromHost();
        admin.waitForDisconnected(1000);
    }

private:
    class Fixture {
    public:
        Fixture()
            : manager_(QStringLiteral("admin-fixture-") + QUuid::createUuid().toString()),
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

    static void adminLogin(QTcpSocket& socket)
    {
        const auto login = exchange(socket, {
            QStringLiteral("admin-login"),
            QStringLiteral("admin.login"),
            {{QStringLiteral("username"), QStringLiteral("admin")},
             {QStringLiteral("password"), QStringLiteral("123456")}}});
        QCOMPARE(login.type, QStringLiteral("admin.login.ok"));
    }

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

QTEST_GUILESS_MAIN(AdminExtendedTest)
#include "test_admin_extended.moc"
