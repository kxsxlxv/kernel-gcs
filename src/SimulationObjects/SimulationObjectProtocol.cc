#include "SimulationObjectProtocol.h"

#include <cmath>
#include <limits>

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonParseError>
#include <QtCore/QUuid>

namespace {

bool decimal(const QJsonValue& value, quint64& result)
{
    if (!value.isString()) {
        return false;
    }
    const QString string = value.toString();
    if (string.isEmpty() || string.size() > 20) {
        return false;
    }
    for (const QChar character : string) {
        if (character.unicode() < '0' || character.unicode() > '9') {
            return false;
        }
    }
    bool ok = false;
    const quint64 number = string.toULongLong(&ok, 10);
    if (ok) {
        result = number;
    }
    return ok;
}

bool integer32(const QJsonValue& value, quint32& result)
{
    if (!value.isDouble()) {
        return false;
    }
    const double number = value.toDouble();
    if (!std::isfinite(number) || number < 0 || number > std::numeric_limits<quint32>::max() ||
        std::floor(number) != number) {
        return false;
    }
    result = static_cast<quint32>(number);
    return true;
}

bool real(const QJsonValue& value, double& result, double low, double high)
{
    if (!value.isDouble()) {
        return false;
    }
    const double number = value.toDouble();
    if (!std::isfinite(number) || number < low || number > high) {
        return false;
    }
    result = number;
    return true;
}

bool optionalReal(const QJsonObject& object, const char* name, double low, double high)
{
    const QJsonValue value = object.value(QLatin1String(name));
    if (value.isUndefined() || value.isNull()) {
        return true;
    }
    double ignored = 0;
    return real(value, ignored, low, high);
}

bool optionalBool(const QJsonObject& object, const char* name)
{
    const QJsonValue value = object.value(QLatin1String(name));
    return value.isUndefined() || value.isNull() || value.isBool();
}

bool optionalString(const QJsonObject& object, const char* name)
{
    const QJsonValue value = object.value(QLatin1String(name));
    return value.isUndefined() || value.isNull() || value.isString();
}

bool optionalDecimal(const QJsonObject& object, const char* name)
{
    const QJsonValue value = object.value(QLatin1String(name));
    quint64 ignored = 0;
    return value.isUndefined() || value.isNull() || decimal(value, ignored);
}

bool identifier(const QJsonValue& value, QString& id)
{
    if (!value.isString()) {
        return false;
    }
    const QString result = value.toString();
    if (result.isEmpty() || result.toUtf8().size() > 128) {
        return false;
    }
    for (const QChar character : result) {
        if (character.unicode() < 0x20 || character.unicode() == 0x7f) {
            return false;
        }
    }
    id = result;
    return true;
}

bool uuid(const QJsonValue& value, QString& result)
{
    if (!value.isString()) {
        return false;
    }
    const QString original = value.toString();
    const QUuid parsed(original);
    if (parsed.isNull() || parsed.toString(QUuid::WithoutBraces) != original.toLower()) {
        return false;
    }
    result = parsed.toString(QUuid::WithoutBraces);
    return true;
}

bool binding(const QJsonValue& value)
{
    if (value.isUndefined() || value.isNull()) {
        return true;
    }
    if (!value.isObject()) {
        return false;
    }
    const QJsonObject object = value.toObject();
    QString name;
    quint32 systemId = 0;
    quint32 componentId = 0;
    return identifier(object.value(QStringLiteral("source_key")), name) &&
           integer32(object.value(QStringLiteral("sysid")), systemId) &&
           integer32(object.value(QStringLiteral("compid")), componentId) &&
           systemId >= 1 && systemId <= 255 && componentId <= 255;
}

bool position(const QJsonValue& value, SimulationObjectProtocol::Packet& packet)
{
    if (value.isNull()) {
        return true;
    }
    if (!value.isObject()) {
        return false;
    }
    const QJsonObject object = value.toObject();
    const QString datum = object.value(QStringLiteral("altitude_datum")).toString();
    if (object.value(QStringLiteral("frame")).toString() != QLatin1String("WGS84_GEODETIC") ||
        (datum != QLatin1String("AMSL_EGM96") && datum != QLatin1String("WGS84_ELLIPSOID") &&
         datum != QLatin1String("AMSL_UNSPECIFIED"))) {
        return false;
    }
    double latitude = 0;
    double longitude = 0;
    double altitude = 0;
    if (!real(object.value(QStringLiteral("lat_deg")), latitude, -90, 90) ||
        !real(object.value(QStringLiteral("lon_deg")), longitude, -180, 180) ||
        !real(object.value(QStringLiteral("altitude_m")), altitude,
              -std::numeric_limits<double>::max(), std::numeric_limits<double>::max())) {
        return false;
    }
    packet.position = QGeoCoordinate(latitude, longitude, altitude);
    packet.positionValid = packet.position.isValid();
    packet.altitudeDatum = datum;
    return packet.positionValid;
}

bool velocity(const QJsonValue& value, SimulationObjectProtocol::Packet& packet)
{
    if (value.isUndefined() || value.isNull()) {
        return true;
    }
    if (!value.isObject()) {
        return false;
    }
    const QJsonObject object = value.toObject();
    if (object.value(QStringLiteral("frame")).toString() != QLatin1String("NED")) {
        return false;
    }
    const double bound = std::numeric_limits<double>::max();
    double downMps = 0;
    if (!real(object.value(QStringLiteral("north_mps")), packet.northMps, -bound, bound) ||
        !real(object.value(QStringLiteral("east_mps")), packet.eastMps, -bound, bound) ||
        !real(object.value(QStringLiteral("down_mps")), downMps, -bound, bound)) {
        return false;
    }
    packet.velocityValid = true;
    return true;
}

bool target(const QJsonValue& value, SimulationObjectProtocol::Packet& packet)
{
    if (value.isUndefined() || value.isNull()) {
        return true;
    }
    if (!value.isObject()) {
        return false;
    }
    const QJsonObject object = value.toObject();
    const QJsonValue valid = object.value(QStringLiteral("valid"));
    if (!(valid.isBool() || valid.isNull()) ||
        !identifier(object.value(QStringLiteral("npc_id")), packet.targetId) ||
        !decimal(object.value(QStringLiteral("generation")), packet.targetGeneration) ||
        !binding(object.value(QStringLiteral("mavlink")))) {
        return false;
    }
    packet.targetValid = valid.isBool() && valid.toBool();
    return true;
}

bool engagement(const QJsonValue& value, SimulationObjectProtocol::Packet& packet)
{
    if (value.isUndefined() || value.isNull()) {
        return true;
    }
    if (!value.isObject()) {
        return false;
    }
    const QJsonObject object = value.toObject();
    const double bound = std::numeric_limits<double>::max();
    if (!optionalReal(object, "slant_range_m", 0, bound) ||
        !optionalReal(object, "closing_speed_mps", -bound, bound) ||
        !optionalReal(object, "estimated_intercept_s", 0, bound) ||
        !optionalBool(object, "seeker_locked") ||
        !optionalBool(object, "terrain_los_clear") ||
        !optionalString(object, "guidance_phase") ||
        !optionalDecimal(object, "range_sim_time_ns")) {
        return false;
    }
    const QJsonValue rangeValid = object.value(QStringLiteral("range_valid"));
    if (!rangeValid.isUndefined() && !rangeValid.isBool()) {
        return false;
    }
    if (rangeValid.toBool()) {
        quint64 timestamp = 0;
        if (!object.value(QStringLiteral("slant_range_m")).isDouble() ||
            !decimal(object.value(QStringLiteral("range_sim_time_ns")), timestamp)) {
            return false;
        }
    }
    packet.rangeValid = rangeValid.toBool();
    const QJsonValue rangeValue = object.value(QStringLiteral("slant_range_m"));
    if (rangeValue.isDouble()) {
        // The existing optionalReal validator above already checked finite/nonnegative.
        packet.slantRangeM = rangeValue.toDouble();
    }
    return true;
}

}  // namespace

bool SimulationObjectProtocol::decode(const QByteArray& bytes, Packet& packet)
{
    if (bytes.isEmpty() || bytes.size() > 1200) {
        return false;
    }
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return false;
    }
    const QJsonObject root = document.object();
    quint32 major = 0;
    quint32 minor = 0;
    Packet parsed;
    if (root.value(QStringLiteral("schema")).toString() != QLatin1String("kernel.qgc.simobjects") ||
        !integer32(root.value(QStringLiteral("major")), major) || major != 1 ||
        !integer32(root.value(QStringLiteral("minor")), minor) || minor > 65535 ||
        !uuid(root.value(QStringLiteral("session_id")), parsed.sessionId) ||
        !decimal(root.value(QStringLiteral("epoch")), parsed.epoch) ||
        !root.value(QStringLiteral("source")).isObject() ||
        !optionalDecimal(root, "tick")) {
        return false;
    }
    const QJsonObject source = root.value(QStringLiteral("source")).toObject();
    if (!identifier(source.value(QStringLiteral("producer_id")), parsed.producerId) ||
        !uuid(source.value(QStringLiteral("instance_id")), parsed.instanceId)) {
        return false;
    }
    const QString message = root.value(QStringLiteral("message")).toString();
    if (message == QLatin1String("session_announce")) {
        parsed.message = Message::Announcement;
        if (!integer32(root.value(QStringLiteral("session_seq")), parsed.sessionSeq) ||
            !root.value(QStringLiteral("paused")).isBool() ||
            !optionalDecimal(root, "fixed_step_ns") ||
            !optionalString(root, "world_id") ||
            !optionalString(root, "world_revision")) {
            return false;
        }
        const QJsonValue speed = root.value(QStringLiteral("simulation_speed"));
        double number = 0;
        if (!speed.isNull() && !real(speed, number, std::numeric_limits<double>::min(),
                                     std::numeric_limits<double>::max())) {
            return false;
        }
    } else if (message == QLatin1String("session_end")) {
        parsed.message = Message::SessionEnd;
        if (!optionalString(root, "reason")) {
            return false;
        }
    } else if (message == QLatin1String("state") || message == QLatin1String("object_end")) {
        parsed.message = message == QLatin1String("state") ? Message::State : Message::End;
        if (!integer32(root.value(QStringLiteral("seq")), parsed.seq) ||
            !root.value(QStringLiteral("object")).isObject()) {
            return false;
        }
        const QJsonObject object = root.value(QStringLiteral("object")).toObject();
        parsed.kind = object.value(QStringLiteral("kind")).toString();
        if (!identifier(object.value(QStringLiteral("id")), parsed.id) ||
            !decimal(object.value(QStringLiteral("generation")), parsed.generation) ||
            (parsed.kind != QLatin1String("missile") && parsed.kind != QLatin1String("npc"))) {
            return false;
        }
        if (parsed.message == Message::End) {
            if (!identifier(root.value(QStringLiteral("reason")), parsed.completionReason)) {
                return false;
            }
        } else {
            parsed.motionState = object.value(QStringLiteral("motion_state")).toString();
            if (object.value(QStringLiteral("lifecycle")).toString() != QLatin1String("active") ||
                (!parsed.motionState.isEmpty() &&
                 parsed.motionState != QLatin1String("moving") &&
                 parsed.motionState != QLatin1String("stationary") &&
                 parsed.motionState != QLatin1String("paused") &&
                 parsed.motionState != QLatin1String("unknown")) ||
                !root.contains(QStringLiteral("position")) ||
                !position(root.value(QStringLiteral("position")), parsed) ||
                !velocity(root.value(QStringLiteral("velocity")), parsed) ||
                (parsed.kind == QLatin1String("missile") &&
                 (!target(root.value(QStringLiteral("target")), parsed) ||
                  !engagement(root.value(QStringLiteral("engagement")), parsed))) ||
                (parsed.kind == QLatin1String("npc") &&
                 !binding(root.value(QStringLiteral("mavlink_binding"))))) {
                return false;
            }
        }
    } else {
        return false;
    }
    const QJsonValue simTime = root.value(QStringLiteral("sim_time_ns"));
    if ((parsed.message == Message::State || parsed.message == Message::End) &&
        !decimal(simTime, parsed.simTimeNs)) {
        return false;
    }
    if (!simTime.isUndefined() && !simTime.isNull() && !decimal(simTime, parsed.simTimeNs)) {
        return false;
    }
    packet = parsed;
    return true;
}
