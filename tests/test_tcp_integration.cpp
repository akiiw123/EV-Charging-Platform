#include "charging/core/database_manager.h"
#include "charging/core/message_protocol.h"
#include "charging/core/tcp_server.h"

#include <QDateTime>
#include <QHostAddress>
#include <QJsonArray>
#include <QSqlQuery>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>

class TcpIntegrationTest final : public QObject {
    Q_OBJECT

private slots:
    void loginAndListStations()
    {
        Fixture fixture;
        QTcpSocket socket;
        socket.connectToHost(QHostAddress::LocalHost, fixture.port());
        QVERIFY(socket.waitForConnected(3000));
        QVERIFY(fixture.acceptConnection());

        const auto login = exchange(socket, {QStringLiteral("login-1"),
                                              QStringLiteral("auth.phone_login"),
                                              {{QStringLiteral("phone"), QStringLiteral("13600136000")}}});
        QCOMPARE(login.type, QStringLiteral("auth.phone_login.ok"));
        QCOMPARE(login.payload.value(QStringLiteral("user")).toObject()
                     .value(QStringLiteral("nickname")).toString(), QStringLiteral("用户6000"));

        const auto stations = exchange(socket, {QStringLiteral("stations-1"),
                                                 QStringLiteral("station.list"), {}});
        QCOMPARE(stations.type, QStringLiteral("station.list.ok"));
        QVERIFY(!stations.payload.value(QStringLiteral("stations")).toArray().isEmpty());
        socket.disconnectFromHost();
        socket.waitForDisconnected(1000);
    }

    void stationDetailAndPileList()
    {
        Fixture fixture;
        QTcpSocket socket;
        socket.connectToHost(QHostAddress::LocalHost, fixture.port());
        QVERIFY(socket.waitForConnected(3000));
        QVERIFY(fixture.acceptConnection());
        const QJsonObject id {{QStringLiteral("station_id"), 1}};
        QCOMPARE(exchange(socket, {QStringLiteral("detail"), QStringLiteral("station.detail"), id}).type,
                 QStringLiteral("station.detail.ok"));
        const auto piles = exchange(socket, {QStringLiteral("piles"), QStringLiteral("pile.list"), id});
        QCOMPARE(piles.type, QStringLiteral("pile.list.ok"));
        QCOMPARE(piles.payload.value(QStringLiteral("piles")).toArray().size(), 2);
        socket.disconnectFromHost();
        socket.waitForDisconnected(1000);
    }

    void staleReservationAutoExpires()
    {
        Fixture fixture;
        QTcpSocket socket;
        socket.connectToHost(QHostAddress::LocalHost, fixture.port());
        QVERIFY(socket.waitForConnected(3000));
        QVERIFY(fixture.acceptConnection());

        QCOMPARE(exchange(socket, {QStringLiteral("login"), QStringLiteral("auth.phone_login"),
                                   {{QStringLiteral("phone"), QStringLiteral("13600136000")}}}).type,
                 QStringLiteral("auth.phone_login.ok"));
        const auto reserve = exchange(socket, {QStringLiteral("reserve"), QStringLiteral("order.reserve"),
                                               {{QStringLiteral("pile_id"), 1}}});
        QCOMPARE(reserve.type, QStringLiteral("order.reserve.ok"));

        // 把预约创建时间改到 20 分钟前,模拟超时未开始充电
        QSqlQuery backdate(fixture.database());
        QVERIFY(backdate.exec(QStringLiteral(
            "UPDATE charging_orders SET created_at=datetime('now','-20 minutes') "
            "WHERE status='reserved'")));

        // 任意请求入口都会触发过期清理:活动订单应已被自动取消
        const auto active = exchange(socket, {QStringLiteral("active"), QStringLiteral("order.active"), {}});
        QCOMPARE(active.type, QStringLiteral("order.active.ok"));
        QVERIFY(active.payload.value(QStringLiteral("order")).isNull());

        // 配额释放后,同一用户可再次预约同一电桩
        QCOMPARE(exchange(socket, {QStringLiteral("reserve-2"), QStringLiteral("order.reserve"),
                                   {{QStringLiteral("pile_id"), 1}}}).type,
                 QStringLiteral("order.reserve.ok"));
        socket.disconnectFromHost();
        socket.waitForDisconnected(1000);
    }

    void authenticatedChargingLifecycle()
    {
        Fixture fixture;
        QTcpSocket socket;
        socket.connectToHost(QHostAddress::LocalHost, fixture.port());
        QVERIFY(socket.waitForConnected(3000));
        QVERIFY(fixture.acceptConnection());

        QCOMPARE(exchange(socket, {QStringLiteral("unauthorized"), QStringLiteral("wallet.recharge"),
                                   {{QStringLiteral("amount"), 100}}}).type,
                 QStringLiteral("wallet.recharge.error"));
        QCOMPARE(exchange(socket, {QStringLiteral("login"), QStringLiteral("auth.phone_login"),
                                   {{QStringLiteral("phone"), QStringLiteral("13500135000")}}}).type,
                 QStringLiteral("auth.phone_login.ok"));
        const auto recharge = exchange(socket, {QStringLiteral("recharge"), QStringLiteral("wallet.recharge"),
                                                 {{QStringLiteral("amount"), 100}}});
        QCOMPARE(recharge.type, QStringLiteral("wallet.recharge.ok"));
        QCOMPARE(recharge.payload.value(QStringLiteral("user")).toObject()
                     .value(QStringLiteral("wallet_balance")).toDouble(), 100.0);
        QCOMPARE(exchange(socket, {QStringLiteral("profile"), QStringLiteral("user.profile.update"),
                                   {{QStringLiteral("nickname"), QStringLiteral("测试车主")}}}).type,
                 QStringLiteral("user.profile.update.ok"));

        const auto reservation = exchange(socket, {QStringLiteral("reserve"), QStringLiteral("order.reserve"),
                                                     {{QStringLiteral("pile_id"), 1}}});
        QCOMPARE(reservation.type, QStringLiteral("order.reserve.ok"));
        const qint64 orderId = reservation.payload.value(QStringLiteral("order")).toObject()
                                   .value(QStringLiteral("id")).toInteger();
        const auto started = exchange(socket, {QStringLiteral("start"), QStringLiteral("order.start"),
                                                {{QStringLiteral("order_id"), orderId}}});
        QCOMPARE(started.type, QStringLiteral("order.start.ok"));
        const QDateTime startedAt = QDateTime::fromString(
            started.payload.value(QStringLiteral("order")).toObject()
                .value(QStringLiteral("started_at")).toString(), Qt::ISODate);
        QVERIFY(startedAt.isValid());
        QVERIFY(qAbs(startedAt.secsTo(QDateTime::currentDateTime())) < 5);
        const auto chargingPiles = exchange(socket, {QStringLiteral("charging-piles"),
            QStringLiteral("pile.list"), {{QStringLiteral("station_id"), 1}}});
        QCOMPARE(chargingPiles.payload.value(QStringLiteral("piles")).toArray().first().toObject()
                     .value(QStringLiteral("status")).toString(), QStringLiteral("charging"));
        QTest::qWait(20);
        const auto stopped = exchange(socket, {QStringLiteral("stop"), QStringLiteral("order.stop"),
                                                {{QStringLiteral("order_id"), orderId}}});
        QCOMPARE(stopped.type, QStringLiteral("order.stop.ok"));
        QCOMPARE(stopped.payload.value(QStringLiteral("order")).toObject()
                     .value(QStringLiteral("status")).toString(), QStringLiteral("awaiting_payment"));
        const auto idlePiles = exchange(socket, {QStringLiteral("idle-piles"), QStringLiteral("pile.list"),
                                                  {{QStringLiteral("station_id"), 1}}});
        QCOMPARE(idlePiles.payload.value(QStringLiteral("piles")).toArray().first().toObject()
                     .value(QStringLiteral("status")).toString(), QStringLiteral("idle"));
        const auto settled = exchange(socket, {QStringLiteral("settle"), QStringLiteral("order.settle"),
                                                {{QStringLiteral("order_id"), orderId}}});
        QCOMPARE(settled.type, QStringLiteral("order.settle.ok"));
        QCOMPARE(settled.payload.value(QStringLiteral("order")).toObject()
                     .value(QStringLiteral("status")).toString(), QStringLiteral("completed"));
        const auto history = exchange(socket, {QStringLiteral("history"), QStringLiteral("order.history"), {}});
        QCOMPARE(history.type, QStringLiteral("order.history.ok"));
        QCOMPARE(history.payload.value(QStringLiteral("orders")).toArray().size(), 1);
        socket.disconnectFromHost();
        socket.waitForDisconnected(1000);
    }

    void invalidJsonDoesNotBreakConnection()
    {
        Fixture fixture;
        QTcpSocket socket;
        socket.connectToHost(QHostAddress::LocalHost, fixture.port());
        QVERIFY(socket.waitForConnected(3000));
        QVERIFY(fixture.acceptConnection());
        socket.write("not-json\n");
        QVERIFY(socket.waitForReadyRead(3000));
        charging::core::Message response;
        QString error;
        QVERIFY(charging::core::MessageProtocol::decodeLine(socket.readLine(), &response, &error));
        QCOMPARE(response.type, QStringLiteral("protocol.error"));
        QCOMPARE(exchange(socket, {QStringLiteral("after-error"), QStringLiteral("station.list"), {}}).type,
                 QStringLiteral("station.list.ok"));
        socket.disconnectFromHost();
        socket.waitForDisconnected(1000);
    }

    void administratorOperations()
    {
        Fixture fixture;
        QTcpSocket socket;
        socket.connectToHost(QHostAddress::LocalHost, fixture.port());
        QVERIFY(socket.waitForConnected(3000));
        QVERIFY(fixture.acceptConnection());
        QCOMPARE(exchange(socket, {QStringLiteral("no-auth"), QStringLiteral("admin.dashboard"), {}}).type,
                 QStringLiteral("admin.dashboard.error"));
        QCOMPARE(exchange(socket, {QStringLiteral("bad-login"), QStringLiteral("admin.login"),
                                   {{QStringLiteral("username"), QStringLiteral("admin")},
                                    {QStringLiteral("password"), QStringLiteral("wrong")}}}).type,
                 QStringLiteral("admin.login.error"));
        QCOMPARE(exchange(socket, {QStringLiteral("login"), QStringLiteral("admin.login"),
                                   {{QStringLiteral("username"), QStringLiteral("admin")},
                                    {QStringLiteral("password"), QStringLiteral("123456")}}}).type,
                 QStringLiteral("admin.login.ok"));
        QVERIFY(fixture.administratorPasswordHash().startsWith(QStringLiteral("PBKDF2-SHA256$")));
        const auto dashboard = exchange(socket, {QStringLiteral("dashboard"), QStringLiteral("admin.dashboard"), {}});
        QCOMPARE(dashboard.type, QStringLiteral("admin.dashboard.ok"));
        QVERIFY(dashboard.payload.value(QStringLiteral("metrics")).toObject()
                    .contains(QStringLiteral("online_rate")));
        QVERIFY(dashboard.payload.contains(QStringLiteral("station_energy")));
        QCOMPARE(exchange(socket, {QStringLiteral("orders"), QStringLiteral("admin.order.list"), {}}).type,
                 QStringLiteral("admin.order.list.ok"));
        const int initialPileCount = exchange(
            socket, {QStringLiteral("initial-piles"), QStringLiteral("admin.pile.list"), {}})
                                         .payload.value(QStringLiteral("piles")).toArray().size();
        const auto created = exchange(socket, {QStringLiteral("station-create"), QStringLiteral("admin.station.create"),
            {{QStringLiteral("name"), QStringLiteral("测试新站")}, {QStringLiteral("address"), QStringLiteral("测试路1号")},
             {QStringLiteral("latitude"), 41.8}, {QStringLiteral("longitude"), 123.4},
             {QStringLiteral("price_per_kwh"), 1.5}, {QStringLiteral("pile_count"), 2}}});
        QCOMPARE(created.type, QStringLiteral("admin.station.create.ok"));
        const qint64 stationId = created.payload.value(QStringLiteral("station")).toObject()
                                     .value(QStringLiteral("id")).toInteger();
        QCOMPARE(exchange(socket, {QStringLiteral("station-update"), QStringLiteral("admin.station.update"),
            {{QStringLiteral("id"), stationId}, {QStringLiteral("name"), QStringLiteral("测试新站（已编辑）")},
             {QStringLiteral("address"), QStringLiteral("测试路2号")}, {QStringLiteral("latitude"), 41.81},
             {QStringLiteral("longitude"), 123.41}, {QStringLiteral("price_per_kwh"), 1.6}}}).type,
                 QStringLiteral("admin.station.update.ok"));
        const auto piles = exchange(socket, {QStringLiteral("piles"), QStringLiteral("admin.pile.list"), {}});
        QCOMPARE(piles.type, QStringLiteral("admin.pile.list.ok"));
        QCOMPARE(piles.payload.value(QStringLiteral("piles")).toArray().size(), initialPileCount + 2);
        QCOMPARE(exchange(socket, {QStringLiteral("users"), QStringLiteral("admin.user.list"),
                                   {{QStringLiteral("phone"), QString()}}}).type,
                 QStringLiteral("admin.user.list.ok"));
        socket.disconnectFromHost();
        socket.waitForDisconnected(1000);
    }

    // 头像路径:更新后应回显,且 user.profile 查询能读回
    void profileAvatarPathUpdate()
    {
        Fixture fixture;
        QTcpSocket socket;
        socket.connectToHost(QHostAddress::LocalHost, fixture.port());
        QVERIFY(socket.waitForConnected(3000));
        QVERIFY(fixture.acceptConnection());
        QCOMPARE(exchange(socket, {QStringLiteral("login"), QStringLiteral("auth.phone_login"),
                                   {{QStringLiteral("phone"), QStringLiteral("13600136000")}}}).type,
                 QStringLiteral("auth.phone_login.ok"));
        const auto updated = exchange(socket, {QStringLiteral("avatar"), QStringLiteral("user.profile.update"),
                                               {{QStringLiteral("avatar_path"),
                                                 QStringLiteral("/home/bit/.local/share/charging/profile-13600136000.png")}}});
        QCOMPARE(updated.type, QStringLiteral("user.profile.update.ok"));
        QCOMPARE(updated.payload.value(QStringLiteral("user")).toObject()
                     .value(QStringLiteral("avatar_path")).toString(),
                 QStringLiteral("/home/bit/.local/share/charging/profile-13600136000.png"));
        const auto profile = exchange(socket, {QStringLiteral("profile"), QStringLiteral("user.profile"), {}});
        QCOMPARE(profile.type, QStringLiteral("user.profile.ok"));
        QCOMPARE(profile.payload.value(QStringLiteral("user")).toObject()
                     .value(QStringLiteral("avatar_path")).toString(),
                 QStringLiteral("/home/bit/.local/share/charging/profile-13600136000.png"));
        socket.disconnectFromHost();
        socket.waitForDisconnected(1000);
    }

    // 电桩管理:单独新增、编辑、手工切换状态(充电中拒绝)
    void pileManagementOperations()
    {
        Fixture fixture;
        QTcpSocket admin;
        admin.connectToHost(QHostAddress::LocalHost, fixture.port());
        QVERIFY(admin.waitForConnected(3000));
        QVERIFY(fixture.acceptConnection());
        QCOMPARE(exchange(admin, {QStringLiteral("login"), QStringLiteral("admin.login"),
                                  {{QStringLiteral("username"), QStringLiteral("admin")},
                                   {QStringLiteral("password"), QStringLiteral("123456")}}}).type,
                 QStringLiteral("admin.login.ok"));

        // 单独新增电桩
        const auto created = exchange(admin, {QStringLiteral("pile-create"), QStringLiteral("admin.pile.create"),
            {{QStringLiteral("station_id"), 1}, {QStringLiteral("code"), QStringLiteral("TST-01")},
             {QStringLiteral("type"), QStringLiteral("slow")}, {QStringLiteral("power_kw"), 7.0}}});
        QCOMPARE(created.type, QStringLiteral("admin.pile.create.ok"));
        QCOMPARE(created.payload.value(QStringLiteral("pile")).toObject()
                     .value(QStringLiteral("status")).toString(), QStringLiteral("idle"));

        // 重复编号 → 拒绝(数据库唯一约束)
        QCOMPARE(exchange(admin, {QStringLiteral("pile-dup"), QStringLiteral("admin.pile.create"),
            {{QStringLiteral("station_id"), 1}, {QStringLiteral("code"), QStringLiteral("TST-01")},
             {QStringLiteral("type"), QStringLiteral("slow")}, {QStringLiteral("power_kw"), 7.0}}}).type,
                 QStringLiteral("admin.pile.create.error"));
        // 非法状态值 → 拒绝
        QCOMPARE(exchange(admin, {QStringLiteral("pile-bad-status"), QStringLiteral("admin.pile.status"),
                                  {{QStringLiteral("pile_id"), 1},
                                   {QStringLiteral("status"), QStringLiteral("broken")}}}).type,
                 QStringLiteral("admin.pile.status.error"));
        // 手工置为故障
        const auto fault = exchange(admin, {QStringLiteral("pile-fault"), QStringLiteral("admin.pile.status"),
                                            {{QStringLiteral("pile_id"), 1},
                                             {QStringLiteral("status"), QStringLiteral("fault")}}});
        QCOMPARE(fault.type, QStringLiteral("admin.pile.status.ok"));
        QCOMPARE(fault.payload.value(QStringLiteral("pile")).toObject()
                     .value(QStringLiteral("status")).toString(), QStringLiteral("fault"));
        // 恢复空闲
        QCOMPARE(exchange(admin, {QStringLiteral("pile-idle"), QStringLiteral("admin.pile.status"),
                                  {{QStringLiteral("pile_id"), 1},
                                   {QStringLiteral("status"), QStringLiteral("idle")}}}).type,
                 QStringLiteral("admin.pile.status.ok"));
        // 编辑功率
        const auto updated = exchange(admin, {QStringLiteral("pile-update"), QStringLiteral("admin.pile.update"),
            {{QStringLiteral("pile_id"), 1}, {QStringLiteral("type"), QStringLiteral("fast")},
             {QStringLiteral("power_kw"), 60.0}}});
        QCOMPARE(updated.type, QStringLiteral("admin.pile.update.ok"));
        QCOMPARE(updated.payload.value(QStringLiteral("pile")).toObject()
                     .value(QStringLiteral("power_kw")).toDouble(), 60.0);

        // 充电中的电桩:状态切换与编辑均拒绝
        QTcpSocket user;
        user.connectToHost(QHostAddress::LocalHost, fixture.port());
        QVERIFY(user.waitForConnected(3000));
        QVERIFY(fixture.acceptConnection());
        QCOMPARE(exchange(user, {QStringLiteral("login"), QStringLiteral("auth.phone_login"),
                                 {{QStringLiteral("phone"), QStringLiteral("13600136000")}}}).type,
                 QStringLiteral("auth.phone_login.ok"));
        QCOMPARE(exchange(user, {QStringLiteral("reserve"), QStringLiteral("order.reserve"),
                                 {{QStringLiteral("pile_id"), 2}}}).type,
                 QStringLiteral("order.reserve.ok"));
        QCOMPARE(exchange(user, {QStringLiteral("start"), QStringLiteral("order.start"),
                                 {{QStringLiteral("order_id"),
                                   exchange(user, {QStringLiteral("active"), QStringLiteral("order.active"), {}})
                                       .payload.value(QStringLiteral("order")).toObject()
                                       .value(QStringLiteral("id")).toInteger()}}}).type,
                 QStringLiteral("order.start.ok"));
        QCOMPARE(exchange(admin, {QStringLiteral("status-charging"), QStringLiteral("admin.pile.status"),
                                  {{QStringLiteral("pile_id"), 2},
                                   {QStringLiteral("status"), QStringLiteral("fault")}}}).type,
                 QStringLiteral("admin.pile.status.error"));
        QCOMPARE(exchange(admin, {QStringLiteral("update-charging"), QStringLiteral("admin.pile.update"),
                                  {{QStringLiteral("pile_id"), 2}, {QStringLiteral("type"), QStringLiteral("fast")},
                                   {QStringLiteral("power_kw"), 60.0}}}).type,
                 QStringLiteral("admin.pile.update.error"));
        admin.disconnectFromHost();
        admin.waitForDisconnected(1000);
        user.disconnectFromHost();
        user.waitForDisconnected(1000);
    }

    // 默认账号 admin/123456 带首登改密标志:校验改密接口全链路
    void forcedPasswordChangeFlow()
    {
        Fixture fixture;
        QTcpSocket socket;
        socket.connectToHost(QHostAddress::LocalHost, fixture.port());
        QVERIFY(socket.waitForConnected(3000));
        QVERIFY(fixture.acceptConnection());

        // 首登:返回 must_change_password=true
        const auto login = exchange(socket, {QStringLiteral("login"), QStringLiteral("admin.login"),
                                             {{QStringLiteral("username"), QStringLiteral("admin")},
                                              {QStringLiteral("password"), QStringLiteral("123456")}}});
        QCOMPARE(login.type, QStringLiteral("admin.login.ok"));
        QVERIFY(login.payload.value(QStringLiteral("administrator")).toObject()
                    .value(QStringLiteral("must_change_password")).toBool());

        // 当前密码错误 → 拒绝
        QCOMPARE(exchange(socket, {QStringLiteral("chg-bad-old"), QStringLiteral("admin.password.change"),
                                   {{QStringLiteral("old_password"), QStringLiteral("000000")},
                                    {QStringLiteral("new_password"), QStringLiteral("strong-pass-1")}}}).type,
                 QStringLiteral("admin.password.change.error"));
        // 新密码过短 → 拒绝
        QCOMPARE(exchange(socket, {QStringLiteral("chg-short"), QStringLiteral("admin.password.change"),
                                   {{QStringLiteral("old_password"), QStringLiteral("123456")},
                                    {QStringLiteral("new_password"), QStringLiteral("short")}}}).type,
                 QStringLiteral("admin.password.change.error"));
        // 新密码与旧密码相同 → 拒绝
        QCOMPARE(exchange(socket, {QStringLiteral("chg-same"), QStringLiteral("admin.password.change"),
                                   {{QStringLiteral("old_password"), QStringLiteral("123456")},
                                    {QStringLiteral("new_password"), QStringLiteral("123456")}}}).type,
                 QStringLiteral("admin.password.change.error"));
        // 正确改密 → 成功
        QCOMPARE(exchange(socket, {QStringLiteral("chg-ok"), QStringLiteral("admin.password.change"),
                                   {{QStringLiteral("old_password"), QStringLiteral("123456")},
                                    {QStringLiteral("new_password"), QStringLiteral("strong-pass-1")}}}).type,
                 QStringLiteral("admin.password.change.ok"));
        QVERIFY(fixture.administratorPasswordHash().startsWith(QStringLiteral("PBKDF2-SHA256$")));

        // 旧密码不能再登录
        QCOMPARE(exchange(socket, {QStringLiteral("relogin-old"), QStringLiteral("admin.login"),
                                   {{QStringLiteral("username"), QStringLiteral("admin")},
                                    {QStringLiteral("password"), QStringLiteral("123456")}}}).type,
                 QStringLiteral("admin.login.error"));
        // 新密码登录成功,且不再要求改密
        const auto relogin = exchange(socket, {QStringLiteral("relogin-new"), QStringLiteral("admin.login"),
                                               {{QStringLiteral("username"), QStringLiteral("admin")},
                                                {QStringLiteral("password"), QStringLiteral("strong-pass-1")}}});
        QCOMPARE(relogin.type, QStringLiteral("admin.login.ok"));
        QVERIFY(!relogin.payload.value(QStringLiteral("administrator")).toObject()
                    .value(QStringLiteral("must_change_password")).toBool());
        socket.disconnectFromHost();
        socket.waitForDisconnected(1000);
    }

private:
    class Fixture {
    public:
        Fixture()
            : manager_(QStringLiteral("fixture-") + QUuid::createUuid().toString()),
              server_(directory_.filePath(QStringLiteral("test.db")))
        {
            QString error;
            QVERIFY(directory_.isValid());
            QVERIFY2(manager_.open(directory_.filePath(QStringLiteral("test.db")), &error), qPrintable(error));
            QVERIFY2(manager_.initialize(&error), qPrintable(error));
            QVERIFY2(server_.start(QHostAddress::LocalHost, 0, &error), qPrintable(error));
        }

        quint16 port() const { return server_.serverPort(); }
        bool acceptConnection() { return server_.waitForNewConnection(1000); }
        QSqlDatabase database() const { return manager_.database(); }
        QString administratorPasswordHash() const
        {
            QSqlQuery query(manager_.database());
            if (!query.exec(QStringLiteral(
                    "SELECT password_hash FROM administrators WHERE username='admin'"))
                || !query.next())
                return {};
            return query.value(0).toString();
        }

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

QTEST_APPLESS_MAIN(TcpIntegrationTest)
#include "test_tcp_integration.moc"
