#pragma once

#include <QtCore/QString>
#include <QtCore/QVector>
#include <QtCore/QtGlobal>
#include <QtPositioning/QGeoCoordinate>

/// Bounded, display-only telemetry buffer. Never alters authoritative packets,
/// geographic history, reported range, or MAVLink vehicle state.
class SimulationObjectPresentation
{
public:
    struct Pose {
        QGeoCoordinate coordinate;
        double heading = 0.0;
        bool headingValid = false;
    };

    static constexpr qint64 kDisplayDelayMs = 100;
    static constexpr qint64 kExtrapolationLimitMs = 200;
    static constexpr qint64 kDiscontinuityMs = 500;
    static constexpr int kMaxFrames = 16;

    void clear();
    void append(const QGeoCoordinate& coordinate, const QString& altitudeDatum,
                quint64 simTimeNs, qint64 receiveMs, double heading, bool headingValid,
                bool velocityValid, double northMps, double eastMps, double downMps,
                bool moving);
    [[nodiscard]] Pose sampleAt(qint64 nowMs) const;
    int sampleCount() const { return static_cast<int>(_frames.size()); }

private:
    struct Frame {
        QGeoCoordinate coordinate;
        QString altitudeDatum;
        quint64 simTimeNs = 0;
        qint64 receiveMs = 0;
        double heading = 0.0;
        bool headingValid = false;
        bool velocityValid = false;
        double northMps = 0.0;
        double eastMps = 0.0;
        double downMps = 0.0;
        bool moving = false;
    };

    static Pose _pose(const Frame& frame);
    static Pose _interpolate(const Frame& from, const Frame& to, double fraction);
    QVector<Frame> _frames;
};
