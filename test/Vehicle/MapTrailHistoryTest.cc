#include "MapTrailHistoryTest.h"

#include <QtPositioning/QGeoCoordinate>
#include <QtTest/QSignalSpy>
#include <QtQml/qqml.h>

#include "../../src/FlightMap/MapTrailHistory.h"

namespace {
const QGeoCoordinate kOrigin(43.41, 39.94, 100.0);
}

void MapTrailHistoryTest::_renderAnchor()
{
    MapTrailHistory history;
    QVERIFY(!history.renderAnchor().isValid());

    const QGeoCoordinate next = kOrigin.atDistanceAndAzimuth(25, 90);
    history.setLifetimeMs(1000);
    history.appendAt(kOrigin, 0);
    QCOMPARE(history.renderAnchor(), kOrigin);
    history.appendAt(next, 200);
    QCOMPARE(history.renderAnchor(), next);

    // Removing the first sample cannot move the native map anchor.
    history.expireAt(1100);
    QCOMPARE(history.count(), 1);
    QCOMPARE(history.renderAnchor(), next);

    history.expireAt(1300);
    QCOMPARE(history.count(), 0);
    QVERIFY(!history.renderAnchor().isValid());
    history.appendAt(kOrigin, 1400);
    QCOMPARE(history.renderAnchor(), kOrigin);
    history.clear();
    QVERIFY(!history.renderAnchor().isValid());
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

void MapTrailHistoryTest::_liveLifetimeChange()
{
    MapTrailHistory history;
    history.setLifetimeMs(0);
    history.appendAt(kOrigin, 0);
    history.appendAt(kOrigin.atDistanceAndAzimuth(10, 90), 200);
    history.appendAt(kOrigin.atDistanceAndAzimuth(20, 90), 400);
    QCOMPARE(history.count(), 3);

    history.setLifetimeMs(1000);
    history.expireAt(1500);
    QCOMPARE(history.count(), 0);
    history.appendAt(kOrigin.atDistanceAndAzimuth(30, 90), 1700);
    history.appendAt(kOrigin.atDistanceAndAzimuth(40, 90), 1900);
    QCOMPARE(history.count(), 2);

    history.clear();
    QCOMPARE(history.count(), 0);
    history.appendAt(kOrigin, 2000);
    QCOMPARE(history.count(), 1);
}

void MapTrailHistoryTest::_independentHistories()
{
    MapTrailHistory vehicle;
    MapTrailHistory simulation;
    vehicle.setLifetimeMs(1000);
    simulation.setLifetimeMs(5000);

    vehicle.appendAt(kOrigin, 0);
    vehicle.appendAt(kOrigin.atDistanceAndAzimuth(20, 90), 200);
    simulation.appendAt(kOrigin, 0);
    simulation.appendAt(kOrigin.atDistanceAndAzimuth(30, 0), 200);

    vehicle.expireAt(1500);
    simulation.expireAt(1500);
    QCOMPARE(vehicle.count(), 0);
    QCOMPARE(simulation.count(), 2);

    vehicle.appendAt(kOrigin, 1600);
    QCOMPARE(vehicle.count(), 1);
    QCOMPARE(simulation.count(), 2);

    simulation.clear();
    QCOMPARE(simulation.count(), 0);
    QCOMPARE(vehicle.count(), 1);
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
void MapTrailHistoryTest::_qmlTypeRegistered()
{
    // Pure C++ history tests can pass even when its QML element was discarded
    // by static-library linking. Keep a direct regression guard for registration.
    QVERIFY(qmlTypeId("QGroundControl.FlightMap", 1, 0, "MapTrailHistory") >= 0);
}

UT_REGISTER_TEST(MapTrailHistoryTest, TestLabel::Unit)
