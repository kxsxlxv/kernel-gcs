#include "MapTrailHistoryTest.h"

#include <QtCore/qnumeric.h>
#include <QtPositioning/QGeoCoordinate>
#include <QtTest/QSignalSpy>
#include <QtQml/qqml.h>

#include "../../src/FlightMap/MapTrailHistory.h"
#include "../../src/FlightMap/MapTrailSceneGraph.h"

namespace {
const QGeoCoordinate kOrigin(43.41, 39.94, 100.0);
}

void MapTrailHistoryTest::_renderAnchor()
{
    MapTrailHistory history;
    QSignalSpy anchorChanges(&history, &MapTrailHistory::renderAnchorChanged);
    QVERIFY(!history.renderAnchor().isValid());

    const QGeoCoordinate next = kOrigin.atDistanceAndAzimuth(25, 90);
    const QGeoCoordinate farther = kOrigin.atDistanceAndAzimuth(50, 90);
    history.setLifetimeMs(1000);
    history.appendAt(kOrigin, 0);
    QCOMPARE(history.renderAnchor(), kOrigin);
    QCOMPARE(anchorChanges.size(), 1);

    // Neither telemetry nor rolling-window expiration should change the
    // native geographic anchor, which otherwise makes the trail jump for a
    // frame while QML waits for a scheduled mesh update.
    history.appendAt(next, 200);
    history.appendAt(farther, 400);
    QCOMPARE(history.renderAnchor(), kOrigin);
    QCOMPARE(anchorChanges.size(), 1);
    history.expireAt(1100);
    QCOMPARE(history.count(), 2);
    QCOMPARE(history.renderAnchor(), kOrigin);
    QCOMPARE(anchorChanges.size(), 1);

    // The renderer can explicitly rebase when the path reaches the edge.
    history.rebaseToLatest();
    QCOMPARE(history.renderAnchor(), farther);
    QCOMPARE(anchorChanges.size(), 2);
    history.rebaseToLatest();
    QCOMPARE(anchorChanges.size(), 2);

    const QGeoCoordinate resumed = kOrigin.atDistanceAndAzimuth(75, 90);
    history.appendAt(resumed, 1200);
    QCOMPARE(history.renderAnchor(), farther);
    QCOMPARE(anchorChanges.size(), 2);

    history.expireAt(2300);
    QCOMPARE(history.count(), 0);
    QVERIFY(!history.renderAnchor().isValid());
    QCOMPARE(anchorChanges.size(), 3);

    history.appendAt(kOrigin, 2400);
    QCOMPARE(history.renderAnchor(), kOrigin);
    QCOMPARE(anchorChanges.size(), 4);
    history.clear();
    QVERIFY(!history.renderAnchor().isValid());
    QCOMPARE(anchorChanges.size(), 5);
    history.rebaseToLatest();
    QCOMPARE(anchorChanges.size(), 5);
}

void MapTrailHistoryTest::_sceneGraphStyles()
{
    const QVector<QPointF> points{QPointF(0, 0), QPointF(100, 0), QPointF(200, 30)};
    const QVector<qreal> fades{0.1, 0.5, 1.0};
    const auto solid = MapTrailSceneGraph::buildPrimitives(points, fades, 3.0, QStringLiteral("solid"));
    QCOMPARE(solid.size(), 2);
    QVERIFY(!solid[0].dot);
    const auto fading = MapTrailSceneGraph::buildPrimitives(points, fades, 3.0, QStringLiteral("fading"));
    QCOMPARE(fading.size(), 2);
    QCOMPARE(fading.first().alphaStart, 0.1);
    QCOMPARE(fading.first().alphaEnd, 0.5);

    const auto dashes = MapTrailSceneGraph::buildPrimitives(points, fades, 3.0, QStringLiteral("dashed"));
    QVERIFY(dashes.size() > solid.size());
    QVERIFY(dashes.size() < MapTrailSceneGraph::kMaxPrimitives);
    QVERIFY(!dashes.first().dot);

    const auto dots = MapTrailSceneGraph::buildPrimitives(points, fades, 3.0, QStringLiteral("dotted"));
    QVERIFY(dots.size() > solid.size());
    QVERIFY(dots.first().dot);

    QVERIFY(MapTrailSceneGraph::buildPrimitives(points, fades, 3.0, QStringLiteral("none")).isEmpty());
    QVERIFY(MapTrailSceneGraph::buildPrimitives({points.first()}, fades, 3.0, QStringLiteral("solid")).isEmpty());
}

void MapTrailHistoryTest::_sceneGraphFading()
{
    const QVector<QPointF> points{QPointF(0, 0), QPointF(100, 0), QPointF(200, 0)};
    const auto factors = QVector<qreal>{0.0, 0.4, 1.0};
    const auto fading = MapTrailSceneGraph::buildPrimitives(points, factors, 3.0, QStringLiteral("fading"));
    QCOMPARE(fading.size(), 2);
    QCOMPARE(fading[0].alphaStart, 0.0);
    QCOMPARE(fading[0].alphaEnd, 0.4);
    QCOMPARE(fading[1].alphaStart, 0.4);
    QCOMPARE(fading[1].alphaEnd, 1.0);

    // Invalid projected positions break the path, never connect across a
    // coordinate projection failure or around a longitude wrap.
    const QVector<QPointF> broken{QPointF(0, 0), QPointF(100, 0),
                                  QPointF(qQNaN(), qQNaN()), QPointF(200, 0)};
    QCOMPARE(MapTrailSceneGraph::buildPrimitives(broken, factors, 3.0, QStringLiteral("solid")).size(), 1);

    const QVector<QPointF> far{QPointF(0, 0), QPointF(1e7, 0)};
    const auto many = MapTrailSceneGraph::buildPrimitives(far, {}, 3.0, QStringLiteral("dotted"));
    QVERIFY(many.size() <= MapTrailSceneGraph::kMaxPrimitives);
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
    QVERIFY(qmlTypeId("QGroundControl.FlightMap", 1, 0, "MapTrailSceneGraph") >= 0);
}

UT_REGISTER_TEST(MapTrailHistoryTest, TestLabel::Unit)
