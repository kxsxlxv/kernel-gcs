#include "SimulationObjectManager.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include <QtCore/QApplicationStatic>
#include <QtNetwork/QUdpSocket>
#include <QtNetwork/QNetworkDatagram>

#include "Fact.h"
#include "QmlObjectListModel.h"
#include "QGCLoggingCategory.h"
#include "SettingsManager.h"
#include "SimulationObjectSettings.h"

QGC_LOGGING_CATEGORY(SimulationObjectLog, "SimulationObjects.Manager")

Q_APPLICATION_STATIC(SimulationObjectManager, _simulationObjectManagerInstance,
                     SettingsManager::instance()->simulationObjectSettings());

SimulationObject::SimulationObject(const SimulationObjectProtocol::Packet& packet, QObject* parent)
    : QObject(parent)
    , _id(packet.id)
    , _kind(packet.kind)
    , _sessionId(packet.sessionId)
    , _epoch(packet.epoch)
    , _generation(packet.generation)
{
}

void SimulationObject::_apply(const SimulationObjectProtocol::Packet& packet, qint64 receiveMs)
{
    _lastReceiveMs = receiveMs;
    _motionState = packet.motionState.isEmpty() ? QStringLiteral("unknown") : packet.motionState;
    _completionReason.clear();
    _targetId = packet.targetId;
    _targetValid = packet.targetValid;
    // Prefer NED horizontal velocity. At low speed keep the last reliable heading.
    // Only infer from positions when the producer omitted velocity entirely.
    static constexpr double kMinHeadingSpeedMps = 0.5;
    if (packet.velocityValid) {
        if (std::hypot(packet.northMps, packet.eastMps) >= kMinHeadingSpeedMps) {
            const double angle = std::atan2(packet.eastMps, packet.northMps) * 180.0 / std::numbers::pi;
            _heading = angle < 0 ? angle + 360.0 : angle;
            _headingValid = true;
        }
    } else if (packet.positionValid && _positionValid && packet.simTimeNs > _lastPositionSimTimeNs) {
        const double elapsedSeconds = (packet.simTimeNs - _lastPositionSimTimeNs) / 1e9;
        const double distanceMeters = _coordinate.distanceTo(packet.position);
        if (distanceMeters >= 1.0 && distanceMeters / elapsedSeconds >= kMinHeadingSpeedMps) {
            _heading = _coordinate.azimuthTo(packet.position);
            _headingValid = true;
        }
    }

    // The old coordinate may remain for last-known display, but is never marked current.
    _positionValid = packet.positionValid;
    _altitudeDatum = packet.positionValid ? packet.altitudeDatum : QString();
    if (packet.positionValid) {
        _coordinate = packet.position;
        _lastPositionSimTimeNs = packet.simTimeNs;
    }
    _setLifecycle(QStringLiteral("active"));
    emit changed();
}

void SimulationObject::_setLifecycle(const QString& lifecycle)
{
    if (_lifecycle != lifecycle) {
        _lifecycle = lifecycle;
        emit changed();
    }
}

SimulationObjectManager::SimulationObjectManager(SimulationObjectSettings* settings, QObject* parent)
    : QObject(parent)
    , _settings(settings)
    , _objects(new QmlObjectListModel(this))
{
    _clock.start();
    _cleanupTimer.setInterval(100);
    connect(&_cleanupTimer, &QTimer::timeout, this, [this]() { expireAt(_clock.elapsed()); });
}

SimulationObjectManager::~SimulationObjectManager()
{
    stop();
}

SimulationObjectManager* SimulationObjectManager::instance()
{
    return _simulationObjectManagerInstance();
}

void SimulationObjectManager::initialize()
{
    if (!_settings) {
        return;
    }
    const auto restart = [this](const QVariant&) { _restart(); };
    connect(_settings->enabled(), &Fact::rawValueChanged, this, restart);
    connect(_settings->bindAddress(), &Fact::rawValueChanged, this, restart);
    connect(_settings->listenPort(), &Fact::rawValueChanged, this, restart);
    connect(_settings->allowedSender(), &Fact::rawValueChanged, this, restart);
    connect(_settings->producerId(), &Fact::rawValueChanged, this, restart);
    _restart();
}

void SimulationObjectManager::_restart()
{
    stop();
    if (!_settings) {
        return;
    }
    if (!_settings->enabled()->rawValue().toBool()) {
        qCDebug(SimulationObjectLog) << "Simulation receiver disabled";
        return;
    }
    QHostAddress bindAddress;
    if (!bindAddress.setAddress(_settings->bindAddress()->rawValue().toString()) ||
        !_allowedPeer.setAddress(_settings->allowedSender()->rawValue().toString()) ||
        _settings->producerId()->rawValue().toString().isEmpty()) {
        qCWarning(SimulationObjectLog) << "Invalid simulation receiver endpoint or source configuration";
        return;
    }
    const quint32 configuredPort = _settings->listenPort()->rawValue().toUInt();
    if (configuredPort < 1 || configuredPort > 65535) {
        qCWarning(SimulationObjectLog) << "Invalid simulation UDP port" << configuredPort;
        return;
    }
    _socket = new QUdpSocket(this);
    if (!_socket->bind(bindAddress, static_cast<quint16>(configuredPort))) {
        qCWarning(SimulationObjectLog) << "Unable to bind simulation UDP socket" << _socket->errorString();
        delete _socket;
        _socket = nullptr;
        return;
    }
    connect(_socket, &QUdpSocket::readyRead, this, &SimulationObjectManager::_receivePending);
    _cleanupTimer.start();
    qCDebug(SimulationObjectLog) << "Simulation receiver listening" << bindAddress << configuredPort;
}

void SimulationObjectManager::stop()
{
    _cleanupTimer.stop();
    delete _socket;
    _socket = nullptr;
    _clearObjects();
    _activeSession.clear();
    _activeEpoch = 0;
    _hasAnnouncement = false;
    _lastAnnouncementSeq = 0;
    _lastSourceReceiveMs = -1;
    _watermarks.clear();
    _retiredSessions.clear();
}

void SimulationObjectManager::_clearObjects()
{
    _entries.clear();
    _objects->clearAndDeleteContents();
}

void SimulationObjectManager::_receivePending()
{
    if (!_socket) {
        return;
    }
    // Bounded per event-loop turn so incoming datagrams cannot monopolize the UI thread.
    int remaining = 256;
    while (remaining-- > 0 && _socket->hasPendingDatagrams()) {
        const QNetworkDatagram datagram = _socket->receiveDatagram(1201);
        (void) acceptDatagram(datagram.data(), datagram.senderAddress());
    }
    if (_socket->hasPendingDatagrams()) {
        QTimer::singleShot(0, this, &SimulationObjectManager::_receivePending);
    }
}

QString SimulationObjectManager::_sessionKey(const QString& sessionId, quint64 epoch)
{
    return sessionId + QLatin1Char('|') + QString::number(epoch);
}

QString SimulationObjectManager::_objectKey(const QString& sessionId, quint64 epoch,
                                           const QString& id, quint64 generation)
{
    // Identifier validation forbids control characters; delimit records with a non-printable separator.
    return _sessionKey(sessionId, epoch) + QChar(0x1f) + id + QChar(0x1f) + QString::number(generation);
}

SimulationObject* SimulationObjectManager::objectFor(const QString& sessionId, quint64 epoch,
                                                    const QString& id, quint64 generation) const
{
    return _entries.value(_objectKey(sessionId, epoch, id, generation));
}

bool SimulationObjectManager::acceptDatagram(const QByteArray& bytes, const QHostAddress& sender)
{
    if (!_settings || !_settings->enabled()->rawValue().toBool()) {
        return false;
    }
    QHostAddress trusted;
    if (!trusted.setAddress(_settings->allowedSender()->rawValue().toString()) || sender != trusted) {
        return false;
    }

    SimulationObjectProtocol::Packet packet;
    if (!SimulationObjectProtocol::decode(bytes, packet) ||
        packet.producerId != _settings->producerId()->rawValue().toString()) {
        return false;
    }
    const qint64 now = _clock.elapsed();
    const QString incomingSession = _sessionKey(packet.sessionId, packet.epoch);

    if (packet.message == SimulationObjectProtocol::Message::Announcement) {
        if (_retiredSessions.contains(incomingSession)) {
            return false;
        }
        if (!_hasAnnouncement || incomingSession != _activeSession) {
            if (_hasAnnouncement) {
                if (_retiredSessions.size() >= kMaxRetiredSessions ||
                    (packet.sessionId == _activeSession.section(QLatin1Char('|'), 0, 0) &&
                     packet.epoch <= _activeEpoch)) {
                    return false;
                }
                _retiredSessions.insert(_activeSession);
            }
            _clearObjects();
            _watermarks.clear();
            _activeSession = incomingSession;
            qCDebug(SimulationObjectLog) << "Simulation session admitted" << _activeSession;
            _activeEpoch = packet.epoch;
            _hasAnnouncement = true;
            _lastAnnouncementSeq = packet.sessionSeq;
        } else {
            if (packet.sessionSeq <= _lastAnnouncementSeq) {
                return false;
            }
            _lastAnnouncementSeq = packet.sessionSeq;
        }
        _lastSourceReceiveMs = now;
        return true;
    }

    if (!_hasAnnouncement || incomingSession != _activeSession) {
        return false;
    }
    if (packet.message == SimulationObjectProtocol::Message::SessionEnd) {
        _retiredSessions.insert(_activeSession);
        _clearObjects();
        _watermarks.clear();
        _hasAnnouncement = false;
        _activeSession.clear();
        _lastSourceReceiveMs = now;
        return true;
    }

    const QString key = _objectKey(packet.sessionId, packet.epoch, packet.id, packet.generation);
    auto watermark = _watermarks.find(key);
    if (watermark != _watermarks.end()) {
        if (watermark->ended || packet.kind != watermark->kind || packet.seq <= watermark->seq ||
            packet.simTimeNs < watermark->simTimeNs) {
            return false;
        }
    } else {
        if (_watermarks.size() >= kMaxWatermarks) {
            return false; // Fail closed rather than forgetting a completed object's tombstone.
        }
        watermark = _watermarks.insert(key, Watermark{});
        watermark->kind = packet.kind;
    }

    watermark->seq = packet.seq;
    watermark->simTimeNs = packet.simTimeNs;
    _lastSourceReceiveMs = now;

    SimulationObject* object = _entries.value(key);
    if (packet.message == SimulationObjectProtocol::Message::End) {
        watermark->ended = true;
        if (object) {
            object->_completedMs = now;
            object->_completionReason = packet.completionReason;
            qCDebug(SimulationObjectLog) << "Simulation object ended" << packet.id << packet.completionReason;
            object->_setLifecycle(QStringLiteral("completed"));
        }
        return true;
    }

    if (!object) {
        if (_entries.size() >= kMaxTrackedObjects) {
            return false;
        }
        object = new SimulationObject(packet, this);
        object->_identityKey = key;
        _entries.insert(key, object);
        _objects->append(object);
        qCDebug(SimulationObjectLog) << "Simulation object created" << packet.id << packet.kind;
    }
    object->_apply(packet, now);
    return true;
}

void SimulationObjectManager::expireAt(qint64 nowMs)
{
    if (!_settings) {
        return;
    }
    const qint64 stale = std::max<qint64>(100, _settings->staleTimeoutMs()->rawValue().toLongLong());
    const qint64 removal = std::max<qint64>(stale + 1, _settings->removalTimeoutMs()->rawValue().toLongLong());
    const qint64 retention = std::max<qint64>(0, _settings->completedRetentionMs()->rawValue().toLongLong());
    const bool sourceDisconnected = _lastSourceReceiveMs >= 0 && nowMs - _lastSourceReceiveMs >= stale;
    for (int i = _objects->count() - 1; i >= 0; --i) {
        auto* object = qobject_cast<SimulationObject*>(_objects->get(i));
        if (!object) {
            continue;
        }
        const bool completed = object->_completedMs >= 0;
        const qint64 age = nowMs - (completed ? object->_completedMs : object->_lastReceiveMs);
        if (age >= (completed ? retention : removal)) {
            _entries.remove(object->_identityKey);
            _objects->removeAt(i);
            qCDebug(SimulationObjectLog) << "Simulation object pruned" << object->objectId() << object->lifecycle();
            object->deleteLater();
        } else if (!completed && age >= stale) {
            object->_setLifecycle(sourceDisconnected ? QStringLiteral("disconnected") : QStringLiteral("stale"));
        }
    }
}
