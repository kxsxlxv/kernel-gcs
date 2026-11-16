#include "SimulationObjectPresentation.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace {

double wrapHeading(double degrees)
{
    const double wrapped = std::fmod(degrees, 360.0);
    return wrapped < 0.0 ? wrapped + 360.0 : wrapped;
}

} // namespace

void SimulationObjectPresentation::clear()
{
    _frames.clear();
}

void SimulationObjectPresentation::append(const QGeoCoordinate& coordinate, const QString& altitudeDatum,
                                          quint64 simTimeNs, qint64 receiveMs, double heading, bool headingValid,
                                          bool velocityValid, double northMps, double eastMps, double downMps,
                                          bool moving)
{
    if (!coordinate.isValid() || !std::isfinite(coordinate.altitude())) {
        clear();
        return;
    }

    const Frame frame{coordinate, altitudeDatum, simTimeNs, receiveMs, heading, headingValid,
                      velocityValid, northMps, eastMps, downMps, moving};

    if (!_frames.isEmpty()) {
        const Frame& last = _frames.last();
        // Never blend across invalid time, paused timeline resets, datum changes,
        // delayed packet bursts or large unobserved gaps.
        if (simTimeNs <= last.simTimeNs || receiveMs < last.receiveMs ||
            receiveMs - last.receiveMs > kDiscontinuityMs ||
            simTimeNs - last.simTimeNs > 2000000000ULL ||
            altitudeDatum != last.altitudeDatum) {
            clear();
        } else if (receiveMs == last.receiveMs) {
            _frames.last() = frame;
            return;
        }
    }

    _frames.append(frame);
    if (_frames.size() > kMaxFrames) {
        _frames.removeFirst();
    }
}

SimulationObjectPresentation::Pose SimulationObjectPresentation::_pose(const Frame& frame)
{
    return {frame.coordinate, frame.heading, frame.headingValid, frame.simTimeNs};
}

SimulationObjectPresentation::Pose SimulationObjectPresentation::_interpolate(const Frame& from,
                                                                               const Frame& to,
                                                                               double fraction)
{
    fraction = std::clamp(fraction, 0.0, 1.0);
    const double distance = from.coordinate.distanceTo(to.coordinate);
    QGeoCoordinate coordinate = from.coordinate;
    if (std::isfinite(distance) && distance > 0.0) {
        coordinate = from.coordinate.atDistanceAndAzimuth(distance * fraction,
                                                         from.coordinate.azimuthTo(to.coordinate));
    }
    coordinate.setAltitude(from.coordinate.altitude() +
                           (to.coordinate.altitude() - from.coordinate.altitude()) * fraction);

    Pose pose{coordinate, from.heading, from.headingValid,
              from.simTimeNs + static_cast<quint64>((to.simTimeNs - from.simTimeNs) * fraction)};
    if (from.headingValid && to.headingValid) {
        const double shortest = std::remainder(to.heading - from.heading, 360.0);
        pose.heading = wrapHeading(from.heading + shortest * fraction);
    } else if (to.headingValid) {
        pose.heading = to.heading;
        pose.headingValid = true;
    }
    return pose;
}

SimulationObjectPresentation::Pose SimulationObjectPresentation::sampleAt(qint64 nowMs) const
{
    if (_frames.isEmpty()) {
        return {};
    }
    const qint64 targetMs = nowMs - kDisplayDelayMs;
    if (targetMs <= _frames.first().receiveMs) {
        return _pose(_frames.first());
    }

    for (qsizetype i = 1; i < _frames.size(); ++i) {
        const Frame& current = _frames[i];
        if (targetMs <= current.receiveMs) {
            const Frame& previous = _frames[i - 1];
            const qint64 interval = current.receiveMs - previous.receiveMs;
            if (interval <= 0) {
                return _pose(current);
            }
            const double fraction = static_cast<double>(targetMs - previous.receiveMs) / interval;
            return _interpolate(previous, current, fraction);
        }
    }

    const Frame& last = _frames.last();
    Pose result = _pose(last);
    if (!last.moving || !last.velocityValid || targetMs <= last.receiveMs) {
        return result;
    }
    const qint64 extrapMs = std::min(targetMs - last.receiveMs, kExtrapolationLimitMs);
    // Map simulation time to wall time using the last two accepted samples.
    // The bounded rate also prevents a corrupted/time-warped timeline from
    // creating an unbounded visual jump. With only one frame, assume 1x.
    double simSecondsPerWallSecond = 1.0;
    if (_frames.size() > 1) {
        const Frame& previous = _frames[_frames.size() - 2];
        const qint64 wallMs = last.receiveMs - previous.receiveMs;
        if (wallMs > 0 && last.simTimeNs > previous.simTimeNs) {
            const double simMs = (last.simTimeNs - previous.simTimeNs) / 1000000.0;
            simSecondsPerWallSecond = std::clamp(simMs / wallMs, 0.0, 4.0);
        }
    }
    const double dt = (extrapMs / 1000.0) * simSecondsPerWallSecond;
    const double horizontal = std::hypot(last.northMps, last.eastMps) * dt;
    if (!std::isfinite(horizontal) || !std::isfinite(last.downMps * dt)) {
        return result;
    }
    if (horizontal > 0.0) {
        const double bearing = wrapHeading(std::atan2(last.eastMps, last.northMps) * 180.0 /
                                           std::numbers::pi);
        result.coordinate = last.coordinate.atDistanceAndAzimuth(horizontal, bearing);
    }
    result.coordinate.setAltitude(last.coordinate.altitude() - last.downMps * dt);
    const quint64 advanceNs = static_cast<quint64>(std::llround(dt * 1e9));
    if (advanceNs <= std::numeric_limits<quint64>::max() - last.simTimeNs) {
        result.simTimeNs = last.simTimeNs + advanceNs;
    }
    return result;
}
