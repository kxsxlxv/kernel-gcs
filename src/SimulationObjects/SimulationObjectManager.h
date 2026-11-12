#pragma once

#include <QtCore/QElapsedTimer>
#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtQmlIntegration/QtQmlIntegration>
#include <QtCore/QSet>
#include <QtCore/QTimer>
#include <QtNetwork/QHostAddress>
#include <QtPositioning/QGeoCoordinate>

#include "SimulationObjectProtocol.h"

class QmlObjectListModel;
class QUdpSocket;
class SimulationObjectSettings;

/// A simulation object is never a QGC Vehicle and never sends autopilot commands.
class SimulationObject : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString objectId READ objectId CONSTANT)
    Q_PROPERTY(QString kind READ kind CONSTANT)
    Q_PROPERTY(QString sessionId READ sessionId CONSTANT)
    Q_PROPERTY(quint64 epoch READ epoch CONSTANT)
    Q_PROPERTY(quint64 generation READ generation CONSTANT)
    Q_PROPERTY(QString lifecycle READ lifecycle NOTIFY changed)
    Q_PROPERTY(QString targetId READ targetId NOTIFY changed)
    Q_PROPERTY(bool targetValid READ targetValid NOTIFY changed)
    Q_PROPERTY(QGeoCoordinate coordinate READ coordinate NOTIFY changed)
    Q_PROPERTY(bool positionValid READ positionValid NOTIFY changed)
    Q_PROPERTY(QString altitudeDatum READ altitudeDatum NOTIFY changed)
    Q_PROPERTY(double heading READ heading NOTIFY changed)
    Q_PROPERTY(bool headingValid READ headingValid NOTIFY changed)
    Q_PROPERTY(QString motionState READ motionState NOTIFY changed)
    Q_PROPERTY(QString completionReason READ completionReason NOTIFY changed)

public:
    explicit SimulationObject(const SimulationObjectProtocol::Packet& packet, QObject* parent = nullptr);

    QString objectId() const { return _id; }
    QString kind() const { return _kind; }
    QString sessionId() const { return _sessionId; }
    quint64 epoch() const { return _epoch; }
    quint64 generation() const { return _generation; }
    QString lifecycle() const { return _lifecycle; }
    QString targetId() const { return _targetId; }
    bool targetValid() const { return _targetValid; }
    QGeoCoordinate coordinate() const { return _coordinate; }
    bool positionValid() const { return _positionValid; }
    QString altitudeDatum() const { return _altitudeDatum; }
    double heading() const { return _heading; }
    bool headingValid() const { return _headingValid; }
    QString motionState() const { return _motionState; }
    QString completionReason() const { return _completionReason; }

signals:
    void changed();

private:
    friend class SimulationObjectManager;

    void _apply(const SimulationObjectProtocol::Packet& packet, qint64 receiveMs);
    void _setLifecycle(const QString& lifecycle);

    QString _id;
    QString _kind;
    QString _sessionId;
    QString _identityKey;
    quint64 _epoch = 0;
    quint64 _generation = 0;
    QString _lifecycle = QStringLiteral("active");
    QString _targetId;
    bool _targetValid = false;
    QGeoCoordinate _coordinate;
    bool _positionValid = false;
    QString _altitudeDatum;
    double _heading = 0;
    bool _headingValid = false;
    quint64 _lastPositionSimTimeNs = 0;
    QString _motionState = QStringLiteral("unknown");
    QString _completionReason;
    qint64 _lastReceiveMs = 0;
    qint64 _completedMs = -1;
};

/// Owns session-scoped simulation objects; networking is enabled only by an explicit setting.
class SimulationObjectManager : public QObject
{
    Q_OBJECT
    Q_MOC_INCLUDE("QmlObjectListModel.h")
    Q_PROPERTY(QmlObjectListModel* objects READ objects CONSTANT)
    Q_PROPERTY(QString receiverStatus READ receiverStatus NOTIFY receiverStatusChanged)

public:
    explicit SimulationObjectManager(SimulationObjectSettings* settings, QObject* parent = nullptr);
    ~SimulationObjectManager() override;

    static SimulationObjectManager* instance();
    QmlObjectListModel* objects() const { return _objects; }
    QString receiverStatus() const { return _receiverStatus; }

    void initialize();
    void stop();

    // Public for deterministic protocol/lifecycle tests; identical validation to socket ingress.
    [[nodiscard]] bool acceptDatagram(const QByteArray& bytes, const QHostAddress& sender);
    void expireAt(qint64 monotonicMs);
    SimulationObject* objectFor(const QString& sessionId, quint64 epoch,
                                const QString& id, quint64 generation) const;

signals:
    void receiverStatusChanged();

private:
    struct Watermark {
        quint32 seq = 0;
        quint64 simTimeNs = 0;
        QString kind;
        bool ended = false;
    };

    static QString _sessionKey(const QString& sessionId, quint64 epoch);
    static QString _objectKey(const QString& sessionId, quint64 epoch,
                              const QString& id, quint64 generation);
    void _restart();
    void _clearObjects();
    void _receivePending();
    void _setReceiverStatus(const QString& status);

    SimulationObjectSettings* _settings = nullptr;
    QmlObjectListModel* _objects = nullptr;
    QUdpSocket* _socket = nullptr;
    QTimer _cleanupTimer;
    QElapsedTimer _clock;
    QHostAddress _allowedPeer;
    QString _receiverStatus = QStringLiteral("disabled");
    QHash<QString, SimulationObject*> _entries;
    QHash<QString, Watermark> _watermarks;
    QSet<QString> _retiredSessions;
    QString _activeSession;
    quint64 _activeEpoch = 0;
    quint32 _lastAnnouncementSeq = 0;
    qint64 _lastSourceReceiveMs = -1;
    bool _hasAnnouncement = false;

    static constexpr int kMaxTrackedObjects = 256;
    static constexpr int kMaxWatermarks = 4096;
    static constexpr int kMaxRetiredSessions = 256;
};
