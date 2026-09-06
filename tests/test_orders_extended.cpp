#include "charging/core/database_manager.h"
#include "charging/core/message_protocol.h"
#include "charging/core/tcp_server.h"

#include <QHostAddress>
#include <QJsonObject>
#include <QSqlQuery>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>

// 订单扩展测试：重复预约、非空闲桩预约、越权订单、取消后重复取消、
// 预约超时、重复停止、余额不足、重复结算
class OrdersExtendedTest final : public QObject {
    Q_OBJECT

private slots:
    // 重复预约：同一用户有活动订单时再次预约返回 ORDER_ACTIVE_EXISTS
    void duplicateReservationRejected()
    {
        Fixture fixture;
        QTcpSocket socket;
        connectAndLogin(fixture, socket, QStringLiteral("18800000001"));

        const auto first = exchange(socket, {
            QStringLiteral("reserve-1"),
            QStringLiteral("order.reserve"),
            {{QStringLiteral("pile_id"), 1}}});
        QCOMPARE(first.type, QStringLiteral("order.reserve.ok"));

        // 第二次预约（不同桩）仍应失败：用户已有活动订单
        const auto second = exchange(socket, {
            QStringLiteral("reserve-2"),
            QStringLiteral("order.reserve"),
            {{QStringLiteral("pile_id"), 2}}});
        QCOMPARE(second.type, QStringLiteral("order.reserve.error"));
        QCOMPARE(second.payload.value(QStringLiteral("code")).toString(),
                 QStringLiteral("ORDER_ACTIVE_EXISTS"));
        QVERIFY(second.payload.value(QStringLiteral("message")).toString()
                    .contains(QStringLiteral("未完成")));

        socket.disconnectFromHost();
        socket.waitForDisconnected(1000);
    }

    // 非空闲桩预约：seed 中 900002 为 fault 状态，不可预约
    void faultPileReservationRejected()
    {
        Fixture fixture;
        QTcpSocket socket;
        connectAndLogin(fixture, socket, QStringLiteral("18800000001"));

        const auto response = exchange(socket, {
            QStringLiteral("reserve-fault"),
            QStringLiteral("order.reserve"),
            {{QStringLiteral("pile_id"), 900002}}});
        QCOMPARE(response.type, QStringLiteral("order.reserve.error"));
        QCOMPARE(response.payload.value(QStringLiteral("code")).toString(),
                 QStringLiteral("RESERVATION_FAILED"));
        QVERIFY(response.payload.value(QStringLiteral("message")).toString()
                    .contains(QStringLiteral("不可预约")));

        socket.disconnectFromHost();
        socket.waitForDisconnected(1000);
    }

    // 非空闲桩预约：charging 状态的桩不可被其他用户预约
    void chargingPileReservationRejected()
    {
        Fixture fixture;

        // 用户 A 预约桩 1 并开始充电，使桩 1 变为 charging
        QTcpSocket socketA;
        connectAndLogin(fixture, socketA, QStringLiteral("18800000001"));
        const auto reserve = exchange(socketA, {
            QStringLiteral("reserve"),
            QStringLiteral("order.reserve"),
            {{QStringLiteral("pile_id"), 1}}});
        QCOMPARE(reserve.type, QStringLiteral("order.reserve.ok"));
        const qint64 orderId = reserve.payload.value(QStringLiteral("order")).toObject()
                                   .value(QStringLiteral("id")).toInteger();
        QCOMPARE(exchange(socketA, {
            QStringLiteral("start"),
            QStringLiteral("order.start"),
            {{QStringLiteral("order_id"), orderId}}}).type,
                 QStringLiteral("order.start.ok"));

        // 用户 C（新注册，无活动订单）尝试预约同一桩 → 失败
        QTcpSocket socketC;
        connectAndLogin(fixture, socketC, QStringLiteral("13900001111"));
        const auto reserveC = exchange(socketC, {
            QStringLiteral("reserve-charging"),
            QStringLiteral("order.reserve"),
            {{QStringLiteral("pile_id"), 1}}});
        QCOMPARE(reserveC.type, QStringLiteral("order.reserve.error"));
        QCOMPARE(reserveC.payload.value(QStringLiteral("code")).toString(),
                 QStringLiteral("RESERVATION_FAILED"));
        QVERIFY(reserveC.payload.value(QStringLiteral("message")).toString()
                    .contains(QStringLiteral("不可预约")));

        socketA.disconnectFromHost(); socketA.waitForDisconnected(1000);
        socketC.disconnectFromHost(); socketC.waitForDisconnected(1000);
    }

    // 越权订单：用户 B 不能操作用户 A 的订单，返回 ORDER_NOT_FOUND（不泄露他单信息）
    void crossUserOrderAccessDenied()
    {
        Fixture fixture;
        QTcpSocket userA, userB;
        connectAndLogin(fixture, userA, QStringLiteral("18800000001"));
        connectAndLogin(fixture, userB, QStringLiteral("13900002222"));

        const auto reserve = exchange(userA, {
            QStringLiteral("reserve"),
            QStringLiteral("order.reserve"),
            {{QStringLiteral("pile_id"), 1}}});
        QCOMPARE(reserve.type, QStringLiteral("order.reserve.ok"));
        const qint64 orderId = reserve.payload.value(QStringLiteral("order")).toObject()
                                   .value(QStringLiteral("id")).toInteger();

        // 用户 B 尝试取消用户 A 的订单
        const auto cancel = exchange(userB, {
            QStringLiteral("cancel-other"),
            QStringLiteral("order.cancel"),
            {{QStringLiteral("order_id"), orderId}}});
        QCOMPARE(cancel.type, QStringLiteral("order.cancel.error"));
        QCOMPARE(cancel.payload.value(QStringLiteral("code")).toString(),
                 QStringLiteral("ORDER_NOT_FOUND"));

        // 用户 B 尝试启动用户 A 的订单
        const auto start = exchange(userB, {
            QStringLiteral("start-other"),
            QStringLiteral("order.start"),
            {{QStringLiteral("order_id"), orderId}}});
        QCOMPARE(start.type, QStringLiteral("order.start.error"));
        QCOMPARE(start.payload.value(QStringLiteral("code")).toString(),
                 QStringLiteral("ORDER_NOT_FOUND"));

        // 用户 B 尝试停止用户 A 的订单
        const auto stop = exchange(userB, {
            QStringLiteral("stop-other"),
            QStringLiteral("order.stop"),
            {{QStringLiteral("order_id"), orderId}}});
        QCOMPARE(stop.type, QStringLiteral("order.stop.error"));
        QCOMPARE(stop.payload.value(QStringLiteral("code")).toString(),
                 QStringLiteral("ORDER_NOT_FOUND"));

        userA.disconnectFromHost(); userA.waitForDisconnected(1000);
        userB.disconnectFromHost(); userB.waitForDisconnected(1000);
    }

    // 取消后重复取消：已取消的订单再次取消返回 ORDER_CANCEL_FAILED
    void cancelAfterCancelFails()
    {
        Fixture fixture;
        QTcpSocket socket;
        connectAndLogin(fixture, socket, QStringLiteral("18800000001"));

        const auto reserve = exchange(socket, {
            QStringLiteral("reserve"),
            QStringLiteral("order.reserve"),
            {{QStringLiteral("pile_id"), 1}}});
        QCOMPARE(reserve.type, QStringLiteral("order.reserve.ok"));
        const qint64 orderId = reserve.payload.value(QStringLiteral("order")).toObject()
                                   .value(QStringLiteral("id")).toInteger();

        // 第一次取消成功
        QCOMPARE(exchange(socket, {
            QStringLiteral("cancel-1"),
            QStringLiteral("order.cancel"),
            {{QStringLiteral("order_id"), orderId}}}).type,
                 QStringLiteral("order.cancel.ok"));

        // 第二次取消失败
        const auto second = exchange(socket, {
            QStringLiteral("cancel-2"),
            QStringLiteral("order.cancel"),
            {{QStringLiteral("order_id"), orderId}}});
        QCOMPARE(second.type, QStringLiteral("order.cancel.error"));
        QCOMPARE(second.payload.value(QStringLiteral("code")).toString(),
                 QStringLiteral("ORDER_CANCEL_FAILED"));

        socket.disconnectFromHost();
        socket.waitForDisconnected(1000);
    }

    // 预约超时：创建时间超过 15 分钟后，start 返回明确的超时提示
    void reservationTimeoutGivesClearMessage()
    {
        Fixture fixture;
        QTcpSocket socket;
        connectAndLogin(fixture, socket, QStringLiteral("18800000001"));

        const auto reserve = exchange(socket, {
            QStringLiteral("reserve"),
            QStringLiteral("order.reserve"),
            {{QStringLiteral("pile_id"), 1}}});
        QCOMPARE(reserve.type, QStringLiteral("order.reserve.ok"));
        const qint64 orderId = reserve.payload.value(QStringLiteral("order")).toObject()
                                   .value(QStringLiteral("id")).toInteger();

        // 将创建时间改到 20 分钟前，模拟超时
        QSqlQuery backdate(fixture.database());
        QVERIFY(backdate.exec(QStringLiteral(
            "UPDATE charging_orders SET created_at=datetime('now','-20 minutes') "
            "WHERE status='reserved'")));

        // 任意请求触发超时清理后，start 返回明确的超时提示
        const auto start = exchange(socket, {
            QStringLiteral("start"),
            QStringLiteral("order.start"),
            {{QStringLiteral("order_id"), orderId}}});
        QCOMPARE(start.type, QStringLiteral("order.start.error"));
        QVERIFY(start.payload.value(QStringLiteral("message")).toString()
                    .contains(QStringLiteral("超时")));
        QVERIFY(start.payload.value(QStringLiteral("message")).toString()
                    .contains(QStringLiteral("15")));

        // 超时释放后可重新预约同一桩
        QCOMPARE(exchange(socket, {
            QStringLiteral("reserve-2"),
            QStringLiteral("order.reserve"),
            {{QStringLiteral("pile_id"), 1}}}).type,
                 QStringLiteral("order.reserve.ok"));

        socket.disconnectFromHost();
        socket.waitForDisconnected(1000);
    }

    // 重复停止：已停止（awaiting_payment）的订单再次 stop 返回 ORDER_STOP_FAILED
    void stopAfterStopFails()
    {
        Fixture fixture;
        QTcpSocket socket;
        connectAndLogin(fixture, socket, QStringLiteral("18800000001"));

        QCOMPARE(exchange(socket, {
            QStringLiteral("recharge"),
            QStringLiteral("wallet.recharge"),
            {{QStringLiteral("amount"), 100}}}).type,
                 QStringLiteral("wallet.recharge.ok"));
        const auto reserve = exchange(socket, {
            QStringLiteral("reserve"),
            QStringLiteral("order.reserve"),
            {{QStringLiteral("pile_id"), 1}}});
        QCOMPARE(reserve.type, QStringLiteral("order.reserve.ok"));
        const qint64 orderId = reserve.payload.value(QStringLiteral("order")).toObject()
                                   .value(QStringLiteral("id")).toInteger();
        QCOMPARE(exchange(socket, {
            QStringLiteral("start"),
            QStringLiteral("order.start"),
            {{QStringLiteral("order_id"), orderId}}}).type,
                 QStringLiteral("order.start.ok"));
        QTest::qWait(20);
        QCOMPARE(exchange(socket, {
            QStringLiteral("stop-1"),
            QStringLiteral("order.stop"),
            {{QStringLiteral("order_id"), orderId}}}).type,
                 QStringLiteral("order.stop.ok"));

        // 重复停止失败
        const auto second = exchange(socket, {
            QStringLiteral("stop-2"),
            QStringLiteral("order.stop"),
            {{QStringLiteral("order_id"), orderId}}});
        QCOMPARE(second.type, QStringLiteral("order.stop.error"));
        QCOMPARE(second.payload.value(QStringLiteral("code")).toString(),
                 QStringLiteral("ORDER_STOP_FAILED"));
        QVERIFY(second.payload.value(QStringLiteral("message")).toString()
                    .contains(QStringLiteral("充电状态")));

        socket.disconnectFromHost();
        socket.waitForDisconnected(1000);
    }

    // 余额不足：低余额用户（seed 中 18800000003，余额 0.50）结算失败
    void insufficientBalanceSettleFails()
    {
        Fixture fixture;
        QTcpSocket socket;
        connectAndLogin(fixture, socket, QStringLiteral("18800000003"));

        const auto reserve = exchange(socket, {
            QStringLiteral("reserve"),
            QStringLiteral("order.reserve"),
            {{QStringLiteral("pile_id"), 1}}});
        QCOMPARE(reserve.type, QStringLiteral("order.reserve.ok"));
        const qint64 orderId = reserve.payload.value(QStringLiteral("order")).toObject()
                                   .value(QStringLiteral("id")).toInteger();
        QCOMPARE(exchange(socket, {
            QStringLiteral("start"),
            QStringLiteral("order.start"),
            {{QStringLiteral("order_id"), orderId}}}).type,
                 QStringLiteral("order.start.ok"));
        QTest::qWait(20);
        QCOMPARE(exchange(socket, {
            QStringLiteral("stop"),
            QStringLiteral("order.stop"),
            {{QStringLiteral("order_id"), orderId}}}).type,
                 QStringLiteral("order.stop.ok"));

        // 立即停止使本单电量/金额为零（金额 0 ≤ 余额 0.50，结算合法成功），
        // 触不到"余额不足"分支；这里在库中改写待结算订单的计费数据，
        // 使金额超过余额，专门验证结算的余额守卫（不降低业务要求）。
        QSqlQuery bump(fixture.database());
        bump.prepare(QStringLiteral(
            "UPDATE charging_orders SET energy_kwh=40.0, amount=50.0 WHERE id=:id"));
        bump.bindValue(QStringLiteral(":id"), orderId);
        QVERIFY(bump.exec());

        // 结算失败：余额不足
        const auto settle = exchange(socket, {
            QStringLiteral("settle"),
            QStringLiteral("order.settle"),
            {{QStringLiteral("order_id"), orderId}}});
        QCOMPARE(settle.type, QStringLiteral("order.settle.error"));
        QCOMPARE(settle.payload.value(QStringLiteral("code")).toString(),
                 QStringLiteral("ORDER_SETTLE_FAILED"));
        QVERIFY(settle.payload.value(QStringLiteral("message")).toString()
                    .contains(QStringLiteral("余额不足")));

        socket.disconnectFromHost();
        socket.waitForDisconnected(1000);
    }

    // 重复结算：已完成的订单再次结算返回 ORDER_SETTLE_FAILED 且余额不重复扣减
    void duplicateSettlementRejected()
    {
        Fixture fixture;
        QTcpSocket socket;
        connectAndLogin(fixture, socket, QStringLiteral("18800000001"));

        QCOMPARE(exchange(socket, {
            QStringLiteral("recharge"),
            QStringLiteral("wallet.recharge"),
            {{QStringLiteral("amount"), 200}}}).type,
                 QStringLiteral("wallet.recharge.ok"));
        const auto reserve = exchange(socket, {
            QStringLiteral("reserve"),
            QStringLiteral("order.reserve"),
            {{QStringLiteral("pile_id"), 1}}});
        QCOMPARE(reserve.type, QStringLiteral("order.reserve.ok"));
        const qint64 orderId = reserve.payload.value(QStringLiteral("order")).toObject()
                                   .value(QStringLiteral("id")).toInteger();
        QCOMPARE(exchange(socket, {
            QStringLiteral("start"),
            QStringLiteral("order.start"),
            {{QStringLiteral("order_id"), orderId}}}).type,
                 QStringLiteral("order.start.ok"));
        QTest::qWait(20);
        QCOMPARE(exchange(socket, {
            QStringLiteral("stop"),
            QStringLiteral("order.stop"),
            {{QStringLiteral("order_id"), orderId}}}).type,
                 QStringLiteral("order.stop.ok"));
        const auto settle1 = exchange(socket, {
            QStringLiteral("settle-1"),
            QStringLiteral("order.settle"),
            {{QStringLiteral("order_id"), orderId}}});
        QCOMPARE(settle1.type, QStringLiteral("order.settle.ok"));
        const double balanceAfterFirst = settle1.payload.value(QStringLiteral("user")).toObject()
                                             .value(QStringLiteral("wallet_balance")).toDouble();

        // 重复结算失败，提示"请勿重复操作"
        const auto settle2 = exchange(socket, {
            QStringLiteral("settle-2"),
            QStringLiteral("order.settle"),
            {{QStringLiteral("order_id"), orderId}}});
        QCOMPARE(settle2.type, QStringLiteral("order.settle.error"));
        QCOMPARE(settle2.payload.value(QStringLiteral("code")).toString(),
                 QStringLiteral("ORDER_SETTLE_FAILED"));
        QVERIFY(settle2.payload.value(QStringLiteral("message")).toString()
                    .contains(QStringLiteral("请勿重复操作")));

        // 余额不变（未重复扣款）
        const auto profile = exchange(socket, {
            QStringLiteral("profile"),
            QStringLiteral("user.profile"),
            {}});
        QCOMPARE(profile.payload.value(QStringLiteral("user")).toObject()
                     .value(QStringLiteral("wallet_balance")).toDouble(),
                 balanceAfterFirst);

        socket.disconnectFromHost();
        socket.waitForDisconnected(1000);
    }

private:
    class Fixture {
    public:
        Fixture()
            : manager_(QStringLiteral("orders-fixture-") + QUuid::createUuid().toString()),
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

    static void connectAndLogin(Fixture& fixture, QTcpSocket& socket, const QString& phone)
    {
        socket.connectToHost(QHostAddress::LocalHost, fixture.port());
        QVERIFY(socket.waitForConnected(3000));
        QVERIFY(fixture.acceptConnection());
        const auto login = exchange(socket, {
            QStringLiteral("login-%1").arg(phone),
            QStringLiteral("auth.phone_login"),
            {{QStringLiteral("phone"), phone}}});
        QCOMPARE(login.type, QStringLiteral("auth.phone_login.ok"));
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

QTEST_GUILESS_MAIN(OrdersExtendedTest)
#include "test_orders_extended.moc"
