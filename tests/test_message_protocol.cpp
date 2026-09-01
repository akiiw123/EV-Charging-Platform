#include "charging/core/message_protocol.h"

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
};

QTEST_APPLESS_MAIN(MessageProtocolTest)
#include "test_message_protocol.moc"
