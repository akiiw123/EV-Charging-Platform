#include "charging/core/database_manager.h"
#include "charging/core/message_protocol.h"
#include "charging/core/tcp_server.h"

#include <QHostAddress>
#include <QJsonArray>
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
