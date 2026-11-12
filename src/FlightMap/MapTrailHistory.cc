#include "MapTrailHistory.h"

#include <algorithm>
#include <cmath>

MapTrailHistory::MapTrailHistory(QObject* parent)
    : QObject(parent)
{
    _clock.start();
    _timer.setInterval(kCleanupIntervalMs);
    connect(&_timer, &QTimer::timeout, this, [this]() {
        expireAt(_clock.elapsed());
        if (!_samples.isEmpty()) {
            emit fadingChanged();
        }
    });
}

void MapTrailHistory::setEnabled(bool enabled)
{
    if (_enabled == enabled) {
        return;
    }
    _enabled = enabled;
    if (!_enabled) {
        clear();
    }
    emit enabledChanged();
}

void MapTrailHistory::setLifetimeMs(int ms)
{
    if (ms < 0 || ms > 60000 || _lifetimeMs == ms) {
        return;
    }
    _lifetimeMs = ms;
    expireAt(_clock.elapsed());
    if (_lifetimeMs == 0) {
        _timer.stop();
    } else if (!_samples.isEmpty() && !_timer.isActive()) {
        _timer.start();
    }
    emit lifetimeMsChanged();
    emit fadingChanged();
}

void MapTrailHistory::append(const QGeoCoordinate& coordinate)
{
    appendAt(coordinate, _clock.elapsed());
}

void MapTrailHistory::appendAt(const QGeoCoordinate& coordinate, qint64 nowMs)
{
    if (!_enabled || !coordinate.isValid()) {
        return;
    }
    if (!_samples.isEmpty()) {
        const Sample& last = _samples.last();
        if (nowMs < last.timeMs || nowMs - last.timeMs < 100) {
            return;
        }
        const double horizontal = last.coordinate.distanceTo(coordinate);
        const double dz = coordinate.altitude() - last.coordinate.altitude();
        const double distance = std::hypot(horizontal, std::isfinite(dz) ? dz : 0.0);
        if (distance < 0.75) {
            return;
        }
    }
    _samples.append({coordinate, nowMs});
    bool truncated = false;
    while (_samples.size() > kMaxSamples) {
        _samples.removeFirst();
        truncated = true;
    }
    if (truncated) {
        emit pathRebuilt();
    } else {
        emit pointAppended(coordinate);
    }
    emit changed();
    if (_lifetimeMs > 0 && !_timer.isActive()) {
        _timer.start();
    }
    expireAt(nowMs);
}

void MapTrailHistory::expireAt(qint64 nowMs)
{
    if (!_enabled || _samples.isEmpty() || _lifetimeMs == 0) {
        return;
    }
    const qint64 oldestAllowed = nowMs - _lifetimeMs;
    bool removed = false;
    while (!_samples.isEmpty() && _samples.first().timeMs < oldestAllowed) {
        _samples.removeFirst();
        removed = true;
    }
    if (removed) {
        emit pathRebuilt();
        emit changed();
    }
    if (_samples.isEmpty()) {
        _timer.stop();
    }
}

void MapTrailHistory::clear()
{
    _timer.stop();
    if (_samples.isEmpty()) {
        return;
    }
    _samples.clear();
    emit pathRebuilt();
    emit changed();
}

QVariantList MapTrailHistory::coordinates() const
{
    QVariantList result;
    result.reserve(_samples.size());
    for (const Sample& sample : _samples) {
        result.append(QVariant::fromValue(sample.coordinate));
    }
    return result;
}

QVariantList MapTrailHistory::fadeFactors() const
{
    QVariantList result;
    result.reserve(_samples.size());
    const qint64 nowMs = _clock.elapsed();
    for (qsizetype i = 0; i < _samples.size(); ++i) {
        const double alpha = _lifetimeMs > 0
            ? std::clamp(1.0 - (nowMs - _samples[i].timeMs) / static_cast<double>(_lifetimeMs), 0.0, 1.0)
            : (_samples.size() > 1 ? static_cast<double>(i + 1) / _samples.size() : 1.0);
        result.append(alpha);
    }
    return result;
}
