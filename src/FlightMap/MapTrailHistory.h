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
    QML_ELEMENT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(int lifetimeMs READ lifetimeMs WRITE setLifetimeMs NOTIFY lifetimeMsChanged)
    Q_PROPERTY(int count READ count NOTIFY changed)

public:
    explicit MapTrailHistory(QObject* parent = nullptr);

    static constexpr int kMaxSamples = 2048;
    static constexpr int kCleanupIntervalMs = 200;

    bool enabled() const { return _enabled; }
    void setEnabled(bool enabled);

    int lifetimeMs() const { return _lifetimeMs; }
    void setLifetimeMs(int ms);

    int count() const { return static_cast<int>(_samples.size()); }

    Q_INVOKABLE void append(const QGeoCoordinate& coordinate);
    Q_INVOKABLE void clear();
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

private:
    struct Sample {
        QGeoCoordinate coordinate;
        qint64 timeMs = 0;
    };

    QList<Sample> _samples;
    QElapsedTimer _clock;
    QTimer _timer;
    bool _enabled = true;
    int _lifetimeMs = 10000;
};
