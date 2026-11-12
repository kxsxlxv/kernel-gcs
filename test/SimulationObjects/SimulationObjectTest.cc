#include "SimulationObjectTest.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QScopeGuard>
#include <QtNetwork/QHostAddress>

#include "Fact.h"
#include "QmlObjectListModel.h"
#include "SettingsManager.h"
#include "SimulationObjectManager.h"
#include "SimulationObjectProtocol.h"
#include "SimulationObjectSettings.h"

namespace {

const QString kSession1 = QStringLiteral("4a9c14ec-9c72-4f87-8df3-9b69f50ddbb9");
const QString kSession2 = QStringLiteral("c78eddfd-2458-43f4-a706-91332ddb09cb");
const QString kInstance = QStringLiteral("e7f1a32a-5759-4e3c-a1a2-a89f1643319e");

QByteArray packet(const QJsonObject& object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

QJsonObject envelope(const QString& session, const QString& message, const QString& epoch = QStringLiteral("1"))
{
    return {
        {QStringLiteral("schema"), QStringLiteral("kernel.qgc.simobjects")},
        {QStringLiteral("major"), 1},
        {QStringLiteral("minor"), 0},
        {QStringLiteral("message"), message},
        {QStringLiteral("session_id"), session},
        {QStringLiteral("epoch"), epoch},
        {QStringLiteral("source"), QJsonObject{
             {QStringLiteral("producer_id"), QStringLiteral("qgc-fixture")},
             {QStringLiteral("instance_id"), kInstance}}},
        {QStringLiteral("tick"), QStringLiteral("100")},
        {QStringLiteral("sim_time_ns"), QStringLiteral("250000000")},
    };
}

QJsonObject announce(const QString& session, const QString& epoch = QStringLiteral("1"))
{
    QJsonObject object = envelope(session, QStringLiteral("session_announce"), epoch);
    object.insert(QStringLiteral("session_seq"), 0);
    object.insert(QStringLiteral("paused"), false);
    object.insert(QStringLiteral("simulation_speed"), 1.0);
    return object;
}

QJsonObject state(const QString& session, int seq, const QString& id = QStringLiteral("missile-1"),
                  const QString& generation = QStringLiteral("1"), const QString& epoch = QStringLiteral("1"))
{
    QJsonObject object = envelope(session, QStringLiteral("state"), epoch);
    object.insert(QStringLiteral("seq"), seq);
    object.insert(QStringLiteral("object"), QJsonObject{
        {QStringLiteral("id"), id},
        {QStringLiteral("generation"), generation},
        {QStringLiteral("kind"), QStringLiteral("missile")},
        {QStringLiteral("lifecycle"), QStringLiteral("active")},
        {QStringLiteral("motion_state"), QStringLiteral("moving")},
    });
    object.insert(QStringLiteral("position"), QJsonObject{
        {QStringLiteral("frame"), QStringLiteral("WGS84_GEODETIC")},
        {QStringLiteral("lat_deg"), 56.95},
        {QStringLiteral("lon_deg"), 24.11},
        {QStringLiteral("altitude_m"), 360.0},
        {QStringLiteral("altitude_datum"), QStringLiteral("AMSL_EGM96")},
    });
    object.insert(QStringLiteral("velocity"), QJsonObject{
        {QStringLiteral("frame"), QStringLiteral("NED")},
        {QStringLiteral("north_mps"), 95.0},
        {QStringLiteral("east_mps"), 0.0},
        {QStringLiteral("down_mps"), 0.0},
    });
    object.insert(QStringLiteral("target"), QJsonObject{
        {QStringLiteral("npc_id"), QStringLiteral("npc-11")},
        {QStringLiteral("generation"), QStringLiteral("1")},
        {QStringLiteral("valid"), true},
    });
    return object;
}

QJsonObject ended(const QString& session, int seq)
{
    QJsonObject object = envelope(session, QStringLiteral("object_end"));
    object.insert(QStringLiteral("seq"), seq);
    object.insert(QStringLiteral("object"), QJsonObject{
        {QStringLiteral("id"), QStringLiteral("missile-1")},
        {QStringLiteral("generation"), QStringLiteral("1")},
        {QStringLiteral("kind"), QStringLiteral("missile")},
    });
    object.insert(QStringLiteral("reason"), QStringLiteral("test_end"));
    return object;
}

}  // namespace

void SimulationObjectTest::_validatesProtocol()
{
    SimulationObjectProtocol::Packet decoded;
    QJsonObject valid = state(kSession1, 1);
    QVERIFY(SimulationObjectProtocol::decode(packet(valid), decoded));
    QCOMPARE(decoded.kind, QStringLiteral("missile"));
    QCOMPARE(decoded.targetId, QStringLiteral("npc-11"));
    QVERIFY(decoded.positionValid);
    QCOMPARE(decoded.altitudeDatum, QStringLiteral("AMSL_EGM96"));

    QJsonObject invalid = valid;
    invalid.insert(QStringLiteral("major"), 2);
    QVERIFY(!SimulationObjectProtocol::decode(packet(invalid), decoded));

    invalid = valid;
    QJsonObject position = invalid.value(QStringLiteral("position")).toObject();
    position.insert(QStringLiteral("lat_deg"), 91);
    invalid.insert(QStringLiteral("position"), position);
    QVERIFY(!SimulationObjectProtocol::decode(packet(invalid), decoded));

    invalid = valid;
    invalid.insert(QStringLiteral("sim_time_ns"), 250000000.0); // Must be an exact decimal string.
    QVERIFY(!SimulationObjectProtocol::decode(packet(invalid), decoded));

    invalid = valid;
    invalid.insert(QStringLiteral("position"), QJsonValue::Null);
    QVERIFY(SimulationObjectProtocol::decode(packet(invalid), decoded));
    QVERIFY(!decoded.positionValid);

    QVERIFY(!SimulationObjectProtocol::decode(QByteArray(1201, 'x'), decoded));
}

void SimulationObjectTest::_identityAndLifecycle()
{
    auto* settings = SettingsManager::instance()->simulationObjectSettings();
    Fact* const enabled = settings->enabled();
    const QVariant original = enabled->rawValue();
    const auto restore = qScopeGuard([enabled, original]() { enabled->setRawValue(original); });
    enabled->setRawValue(true);
    SimulationObjectManager manager(settings);
    const QHostAddress peer(QHostAddress::LocalHost);

    QVERIFY(manager.acceptDatagram(packet(announce(kSession1)), peer));
    QVERIFY(manager.acceptDatagram(packet(state(kSession1, 1)), peer));
    QCOMPARE(manager.objects()->count(), 1);
    SimulationObject* const first = manager.objectFor(kSession1, 1, QStringLiteral("missile-1"), 1);
    QVERIFY(first);
    QVERIFY(first->positionValid());
    QVERIFY(first->targetValid());
    QVERIFY(!manager.acceptDatagram(packet(state(kSession1, 1)), peer));
    QVERIFY(!manager.acceptDatagram(packet(state(kSession1, 0)), peer));

    // Object generations distinguish simultaneous engagements and restarts.
    QVERIFY(manager.acceptDatagram(packet(state(kSession1, 1, QStringLiteral("missile-1"), QStringLiteral("2"))), peer));
    QCOMPARE(manager.objects()->count(), 2);

    QVERIFY(manager.acceptDatagram(packet(ended(kSession1, 2)), peer));
    QCOMPARE(first->lifecycle(), QStringLiteral("completed"));
    QVERIFY(!manager.acceptDatagram(packet(state(kSession1, 3)), peer));
    QVERIFY(manager.objectFor(kSession1, 1, QStringLiteral("missile-1"), 2));

    // A new announced session reuses logical IDs without aliasing the old object.
    QVERIFY(manager.acceptDatagram(packet(announce(kSession2)), peer));
    QCOMPARE(manager.objects()->count(), 0);
    QVERIFY(!manager.acceptDatagram(packet(state(kSession1, 4)), peer));
    QVERIFY(!manager.acceptDatagram(packet(announce(kSession1)), peer));
    QVERIFY(manager.acceptDatagram(packet(state(kSession2, 1)), peer));
    QCOMPARE(manager.objects()->count(), 1);
    QVERIFY(manager.objectFor(kSession2, 1, QStringLiteral("missile-1"), 1));
}

void SimulationObjectTest::_mapProjectionMetadata()
{
    auto* settings = SettingsManager::instance()->simulationObjectSettings();
    Fact* const enabled = settings->enabled();
    const QVariant original = enabled->rawValue();
    const auto restore = qScopeGuard([enabled, original]() { enabled->setRawValue(original); });
    enabled->setRawValue(true);
    SimulationObjectManager manager(settings);
    const QHostAddress peer(QHostAddress::LocalHost);

    QVERIFY(manager.acceptDatagram(packet(announce(kSession1)), peer));
    QVERIFY(manager.acceptDatagram(packet(state(kSession1, 1)), peer));
    SimulationObject* const missile = manager.objectFor(kSession1, 1, QStringLiteral("missile-1"), 1);
    QVERIFY(missile);
    QCOMPARE(missile->kind(), QStringLiteral("missile"));
    QVERIFY(missile->positionValid());
    QCOMPARE(missile->altitudeDatum(), QStringLiteral("AMSL_EGM96"));

    QJsonObject ellipsoidState = state(kSession1, 2);
    QJsonObject position = ellipsoidState.value(QStringLiteral("position")).toObject();
    position.insert(QStringLiteral("altitude_datum"), QStringLiteral("WGS84_ELLIPSOID"));
    ellipsoidState.insert(QStringLiteral("position"), position);
    QVERIFY(manager.acceptDatagram(packet(ellipsoidState), peer));
    QCOMPARE(missile->altitudeDatum(), QStringLiteral("WGS84_ELLIPSOID"));

    QJsonObject missingPosition = state(kSession1, 3);
    missingPosition.insert(QStringLiteral("position"), QJsonValue::Null);
    QVERIFY(manager.acceptDatagram(packet(missingPosition), peer));
    QVERIFY(!missile->positionValid());
    QVERIFY(missile->altitudeDatum().isEmpty());
    // Coordinates are retained internally as last-known, never presented as a fresh position.
    QVERIFY(missile->coordinate().isValid());

    QJsonObject npcState = state(kSession1, 1, QStringLiteral("npc-1"));
    QJsonObject npc = npcState.value(QStringLiteral("object")).toObject();
    npc.insert(QStringLiteral("kind"), QStringLiteral("npc"));
    npcState.insert(QStringLiteral("object"), npc);
    QVERIFY(manager.acceptDatagram(packet(npcState), peer));
    SimulationObject* const npcObject = manager.objectFor(kSession1, 1, QStringLiteral("npc-1"), 1);
    QVERIFY(npcObject);
    QCOMPARE(npcObject->kind(), QStringLiteral("npc"));
    QVERIFY(npcObject->positionValid());
}

void SimulationObjectTest::_staleAndDisconnect()
{
    auto* settings = SettingsManager::instance()->simulationObjectSettings();
    Fact* const enabled = settings->enabled();
    const QVariant original = enabled->rawValue();
    const auto restore = qScopeGuard([enabled, original]() { enabled->setRawValue(original); });
    enabled->setRawValue(true);
    SimulationObjectManager manager(settings);
    const QHostAddress peer(QHostAddress::LocalHost);

    QVERIFY(manager.acceptDatagram(packet(announce(kSession1)), peer));
    QVERIFY(manager.acceptDatagram(packet(state(kSession1, 1)), peer));
    SimulationObject* object = manager.objectFor(kSession1, 1, QStringLiteral("missile-1"), 1);
    QVERIFY(object);

    manager.expireAt(2500);
    QCOMPARE(object->lifecycle(), QStringLiteral("disconnected"));
    manager.expireAt(5200);
    QCOMPARE(manager.objects()->count(), 0);
    // A delayed packet does not revive stale state; a newer update may revive it.
    QVERIFY(!manager.acceptDatagram(packet(state(kSession1, 1)), peer));
    QVERIFY(manager.acceptDatagram(packet(state(kSession1, 2)), peer));
    QCOMPARE(manager.objects()->count(), 1);
    object = manager.objectFor(kSession1, 1, QStringLiteral("missile-1"), 1);
    QVERIFY(object);
    QCOMPARE(object->lifecycle(), QStringLiteral("active"));

    QVERIFY(manager.acceptDatagram(packet(ended(kSession1, 3)), peer));
    manager.expireAt(1600);
    QCOMPARE(manager.objects()->count(), 0);
    QVERIFY(!manager.acceptDatagram(packet(state(kSession1, 4)), peer));
}

UT_REGISTER_TEST(SimulationObjectTest, TestLabel::Unit)
