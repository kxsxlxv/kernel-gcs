#pragma once

#include <QtCore/QElapsedTimer>
#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QTimer>
#include <QtCore/QVariantList>
#include <QtPositioning/QGeoCoordinate>
#include <QtQmlIntegration/QtQmlIntegration>

/// Bounded, monotonic-timestamped visual trail, independent of vehicle distance accounting.
class MapTrailHistory : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(int lifetimeMs READ lifetimeMs WRITE setLifetimeMs NOTIFY lifetimeMsChanged)
    Q_PROPERTY(int count READ count NOTIFY changed)
    Q_PROPERTY(QGeoCoordinate renderAnchor READ renderAnchor NOTIFY renderAnchorChanged)

public:
    explicit MapTrailHistory(QObject* parent = nullptr);

    static constexpr int kMaxSamples = 2048;
    static constexpr int kCleanupIntervalMs = 200;

    bool enabled() const { return _enabled; }
    void setEnabled(bool enabled);

    int lifetimeMs() const { return _lifetimeMs; }
    void setLifetimeMs(int ms);

    int count() const { return static_cast<int>(_samples.size()); }
    QGeoCoordinate renderAnchor() const { return _renderAnchor; }

    Q_INVOKABLE void append(const QGeoCoordinate& coordinate);
    Q_INVOKABLE void clear();
    /// Re-anchor only when the projected trail approaches the viewport boundary.
    /// Ordinary telemetry updates must not move the native MapQuickItem.
    Q_INVOKABLE void rebaseToLatest();
    Q_INVOKABLE QVariantList coordinates() const;
    Q_INVOKABLE QVariantList fadeFactors() const;

    /// Deterministic monotonic-clock seam for tests.
    void appendAt(const QGeoCoordinate& coordinate, qint64 nowMs);
    void expireAt(qint64 nowMs);

signals:
    void enabledChanged();
    void lifetimeMsChanged();
    void changed();
    void pointAppended(QGeoCoordinate coordinate);
    void pathRebuilt();
    void fadingChanged();
    void renderAnchorChanged();

private:
    struct Sample {
        QGeoCoordinate coordinate;
        qint64 timeMs = 0;
    };

    QList<Sample> _samples;
    QGeoCoordinate _renderAnchor;
    QElapsedTimer _clock;
    QTimer _timer;
    bool _enabled = true;
    int _lifetimeMs = 10000;
};
