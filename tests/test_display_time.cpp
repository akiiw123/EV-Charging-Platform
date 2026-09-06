#include "charging/core/display_time.h"
#include <QtTest>
class DisplayTimeTest : public QObject {
    Q_OBJECT
private slots:
    void timestamps() {
        using charging::core::beijingTime;
        QCOMPARE(beijingTime(QStringLiteral("2026-09-05 11:39:34")), QStringLiteral("2026-09-05 19:39:34"));
        QCOMPARE(beijingTime(QStringLiteral("2026-09-05T11:39:34Z")), QStringLiteral("2026-09-05 19:39:34"));
        QCOMPARE(beijingTime(QStringLiteral("2026-09-05T19:39:34+08:00")), QStringLiteral("2026-09-05 19:39:34"));
        QCOMPARE(beijingTime(QStringLiteral("2026-09-05T18:00:00Z")), QStringLiteral("2026-09-06 02:00:00"));
        QCOMPARE(beijingTime(QString()), QStringLiteral("—"));
        QCOMPARE(beijingTime(QStringLiteral("invalid")), QStringLiteral("—"));
    }
};
QTEST_GUILESS_MAIN(DisplayTimeTest)
#include "test_display_time.moc"
