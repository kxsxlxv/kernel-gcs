#include "MapTrailHistoryTest.h"

#include <QtPositioning/QGeoCoordinate>
#include <QtTest/QSignalSpy>

#include "../../src/FlightMap/MapTrailHistory.h"

namespace {
const QGeoCoordinate kOrigin(43.41, 39.94, 100.0);
}

void MapTrailHistoryTest::_rollingWindow()
{
    MapTrailHistory history;
    QSignalSpy rebuilt(&history, &MapTrailHistory::pathRebuilt);
    history.setLifetimeMs(1000);

    history.appendAt(kOrigin, 0);
    history.appendAt(kOrigin.atDistanceAndAzimuth(10, 90), 200);
    history.appendAt(kOrigin.atDistanceAndAzimuth(20, 90), 600);
    QCOMPARE(history.count(), 3);
    history.expireAt(1100);
    QCOMPARE(history.count(), 2);
    QCOMPARE(rebuilt.count(), 1);
    history.expireAt(1700);
    QCOMPARE(history.count(), 0);
    QCOMPARE(rebuilt.count(), 2);
}

void MapTrailHistoryTest::_stationaryAndResume()
{
    MapTrailHistory history;
    history.setLifetimeMs(1000);
    history.appendAt(kOrigin, 0);
    for (qint64 t = 200; t < 1000; t += 200) {
        history.appendAt(kOrigin, t);
    }
    QCOMPARE(history.count(), 1);
    history.expireAt(1100);
    QCOMPARE(history.count(), 0);

    history.appendAt(kOrigin.atDistanceAndAzimuth(20, 0), 1200);
    history.appendAt(kOrigin.atDistanceAndAzimuth(30, 0), 1400);
    QCOMPARE(history.count(), 2);
    QCOMPARE(history.coordinates().size(), 2);
}

void MapTrailHistoryTest::_limitsAndDisable()
{
    MapTrailHistory history;
    history.setLifetimeMs(0);
    for (int i = 0; i < MapTrailHistory::kMaxSamples + 100; ++i) {
        history.appendAt(kOrigin.atDistanceAndAzimuth(i * 2.0, 90), i * 100);
    }
    QCOMPARE(history.count(), MapTrailHistory::kMaxSamples);
    history.expireAt(1000000);
    QCOMPARE(history.count(), MapTrailHistory::kMaxSamples);

    history.setEnabled(false);
    QCOMPARE(history.count(), 0);
    history.appendAt(kOrigin, 1000100);
    QCOMPARE(history.count(), 0);
    history.setEnabled(true);
    history.appendAt(kOrigin, 1000200);
    QCOMPARE(history.count(), 1);
}
UT_REGISTER_TEST(MapTrailHistoryTest, TestLabel::Unit)
