#include "charging/core/message_protocol.h"
#include "charging/core/password_security.h"

#include <QJsonObject>
#include <QtTest>

class MessageProtocolTest final : public QObject {
    Q_OBJECT

private slots:
    void roundTrip()
    {
        const charging::core::Message input {
            QStringLiteral("req-1"),
            QStringLiteral("station.list"),
            QJsonObject {{QStringLiteral("city"), QStringLiteral("沈阳")}},
        };
        charging::core::Message output;
        QString error;
        QVERIFY2(charging::core::MessageProtocol::decodeLine(
                     charging::core::MessageProtocol::encode(input), &output, &error),
                 qPrintable(error));
        QCOMPARE(output.id, input.id);
        QCOMPARE(output.type, input.type);
        QCOMPARE(output.payload, input.payload);
    }

    void passwordHashing()
    {
        const QString encoded = charging::core::password::hash(QStringLiteral("123456"));
        QVERIFY(encoded.startsWith(QStringLiteral("PBKDF2-SHA256$")));
        QVERIFY(charging::core::password::verify(QStringLiteral("123456"), encoded));
        QVERIFY(!charging::core::password::verify(QStringLiteral("wrong"), encoded));
        QVERIFY(!charging::core::password::verify(QStringLiteral("123456"),
                                                   QStringLiteral("PBKDF2-SHA256$broken")));
        QVERIFY(charging::core::password::verify(QStringLiteral("legacy"),
                                                  QStringLiteral("DEV_ONLY:legacy")));
        QVERIFY(charging::core::password::needsUpgrade(QStringLiteral("DEV_ONLY:legacy")));
        QVERIFY(!charging::core::password::needsUpgrade(encoded));
    }
};

QTEST_APPLESS_MAIN(MessageProtocolTest)
#include "test_message_protocol.moc"
