#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtCore/QtGlobal>

#include <limits>
#include <QtPositioning/QGeoCoordinate>

namespace SimulationObjectProtocol {

enum class Message { Announcement, State, End, SessionEnd };

struct Packet {
    Message message = Message::State;
    QString sessionId;
    quint64 epoch = 0;
    QString producerId;
    QString instanceId;
    quint64 simTimeNs = 0;
    quint32 sessionSeq = 0;
    quint32 seq = 0;
    QString id;
    quint64 generation = 0;
    QString kind;
    QString motionState;
    QString completionReason;
    QString targetId;
    quint64 targetGeneration = 0;
    bool targetValid = false;
    bool rangeValid = false;
    double slantRangeM = std::numeric_limits<double>::quiet_NaN();
    QGeoCoordinate position;
    bool positionValid = false;
    QString altitudeDatum;
    bool velocityValid = false;
    double northMps = 0;
    double eastMps = 0;
};

/// Decode the versioned, simulation-only JSON wire profile. No MAVLink frames are accepted.
[[nodiscard]] bool decode(const QByteArray& bytes, Packet& packet);

}  // namespace SimulationObjectProtocol
