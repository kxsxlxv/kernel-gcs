import QtQuick
import QtQuick3D
import QtPositioning

import QGroundControl
import QGroundControl.Controls
import QGroundControl.FlightMap
import QGroundControl.GeoMap

/// Shared time-window history, rendered as a terrain-aware GeoMap 2D/3D ribbon.
GeoMapItem {
    id: root

    property var vehicle
    property var simulationObject
    property real simulationHomeTerrainBias: 0

    readonly property bool _simulated: !!simulationObject
    readonly property var _sourceObject: simulationObject ? simulationObject : vehicle
    readonly property var _settings: _simulated
        ? QGroundControl.settingsManager.simulationObjectSettings
        : QGroundControl.settingsManager.flightMapSettings
    readonly property bool _enabled: _simulated
        ? _settings.trailEnabled.rawValue : _settings.vehicleTrailEnabled.rawValue
    readonly property int _lifetimeMs: Number(_simulated
        ? _settings.trailLifetimeSec.rawValue : _settings.vehicleTrailLifetimeSec.rawValue) * 1000
    readonly property string _style: String(_simulated
        ? _settings.trailStyle.rawValue : _settings.vehicleTrailStyle.rawValue)
    readonly property color lineColor: _simulated
        ? _settings.trailColor.rawValue : _settings.vehicleTrailColor.rawValue
    readonly property real lineWidth: Number(_simulated
        ? _settings.trailWidth.rawValue : _settings.vehicleTrailWidth.rawValue)
    readonly property real lineOpacity: Number(_simulated
        ? _settings.trailOpacity.rawValue : _settings.vehicleTrailOpacity.rawValue)
    readonly property bool _clearWhenStopped: _simulated
        ? _settings.trailClearWhenStopped.rawValue : _settings.vehicleTrailClearWhenStopped.rawValue
    readonly property bool _hasAmslAltitude: !_simulated || (simulationObject.altitudeDatum === "AMSL_EGM96")
    readonly property real _bias: _simulated
        ? (_hasAmslAltitude ? simulationHomeTerrainBias : 0) : _homeTerrainBias

    altitudeMode: GeoMapItem.Absolute
    crossfade3D: false
    coordinate: {
        if (!root._geometry || !root._geometry.anchorCoordinate.isValid) {
            return QtPositioning.coordinate()
        }
        const anchor = root._geometry.anchorCoordinate
        return QtPositioning.coordinate(anchor.latitude, anchor.longitude, anchor.altitude + root._bias)
    }
    visible: !!_sourceObject && _enabled && _style !== "none" && lineOpacity > 0
             && trailHistory.count >= 2
    node3DOpacity: 1

    property var _geometry: null
    readonly property var _camera: scene ? scene.camera : null
    readonly property real _screenFactor: _camera ? _camera.unitsPerPixelAtUnitDistance : 0
    property real _homeTerrainBias: 0

    function _updateHomeTerrainBias() {
        if (surfaceModel && vehicle && vehicle.homePosition.isValid && !isNaN(vehicle.homePosition.altitude)) {
            _homeTerrainBias = surfaceModel.terrainDataHeightAt(vehicle.homePosition) - vehicle.homePosition.altitude
        } else {
            _homeTerrainBias = 0
        }
    }

    function _stopped() {
        if (!_sourceObject) {
            return false
        }
        return _simulated
            ? (_sourceObject.motionState === "stationary" || _sourceObject.motionState === "paused")
            : (_sourceObject.groundSpeed && Number(_sourceObject.groundSpeed.rawValue) < 0.5)
    }

    function _applyStopPolicy() {
        if (_clearWhenStopped && _stopped()) {
            trailHistory.clear()
        }
    }

    function _record() {
        if (!_sourceObject || (_simulated && !_sourceObject.positionValid)) {
            return
        }
        _applyStopPolicy()
        if (!(_clearWhenStopped && _stopped())) {
            trailHistory.append(_sourceObject.coordinate)
        }
    }

    function _displayCoordinates() {
        const coordinates = trailHistory.coordinates()
        if (_hasAmslAltitude) {
            return coordinates
        }
        // No ellipsoid-to-AMSL conversion exists: keep this non-AMSL path ground-clamped.
        const clamped = []
        for (let i = 0; i < coordinates.length; ++i) {
            const point = coordinates[i]
            const height = surfaceModel ? surfaceModel.terrainHeightAt(point) : 0
            clamped.push(QtPositioning.coordinate(point.latitude, point.longitude,
                         isFinite(height) ? height : 0))
        }
        return clamped
    }

    function _reloadPath() {
        if (_geometry) {
            _geometry.setPath(_displayCoordinates(), [],
                              _style === "fading" ? trailHistory.fadeFactors() : [])
        }
    }

    onVehicleChanged: {
        trailHistory.clear()
        _updateHomeTerrainBias()
        _record()
    }
    onSimulationObjectChanged: {
        trailHistory.clear()
        _record()
    }
    onSurfaceModelChanged: {
        _updateHomeTerrainBias()
        if (!_hasAmslAltitude) _reloadPath()
    }
    on_ClearWhenStoppedChanged: _applyStopPolicy()
    on_HasAmslAltitudeChanged: _reloadPath()
    Component.onCompleted: _record()

    MapTrailHistory {
        id: trailHistory
        enabled: root._enabled
        lifetimeMs: root._lifetimeMs
        onPointAppended: (coordinate) => {
            if (root._geometry) {
                if (root._hasAmslAltitude) {
                    root._geometry.appendPoint(coordinate)
                } else {
                    root._reloadPath()
                }
            }
        }
        onPathRebuilt: root._reloadPath()
        onFadingChanged: {
            if (root._style === "fading") {
                root._reloadPath()
            }
        }
    }

    Connections {
        target: root._sourceObject
        ignoreUnknownSignals: true
        function onCoordinateChanged() { root._record() }
        function onChanged() { root._record() }
    }

    Connections {
        target: root._simulated || !root.vehicle ? null : root.vehicle.groundSpeed
        ignoreUnknownSignals: true
        function onRawValueChanged() { root._applyStopPolicy() }
    }

    delegate3D: Component {
        Node {
            scale: Qt.vector3d(1, 1, root.scene ? root.scene.terrainScale : 1)

            Model {
                geometry: FlightPathGeometry {
                    id: pathGeometry
                    scene: root.scene
                }

                materials: CustomMaterial {
                    shadingMode: CustomMaterial.Unshaded
                    cullMode: Material.NoCulling
                    sourceBlend: CustomMaterial.SrcAlpha
                    destinationBlend: CustomMaterial.OneMinusSrcAlpha
                    vertexShader: "shaders/flightpath.vert"
                    fragmentShader: "shaders/flightpath.frag"

                    property real lineWidth: root.lineWidth
                    property real screenFactor: root._screenFactor
                    property color pathColor: root.lineColor
                    property color highlightColor: root.lineColor
                    property real pathOpacity: root.lineOpacity
                    property real trailStyle: root._style === "dashed" ? 1.0
                        : root._style === "dotted" ? 2.0 : root._style === "fading" ? 3.0 : 0.0
                    property real depthPull: root.scene ? 0.5 * (1.0 - root.scene.terrainScale) : 0.0
                }

                Component.onCompleted: {
                    root._geometry = pathGeometry
                    root._reloadPath()
                }
                Component.onDestruction: root._geometry = null
            }
        }
    }

    Connections {
        target: root.surfaceModel
        function onTerrainDataChanged() {
            root._updateHomeTerrainBias()
            if (!root._hasAmslAltitude) root._reloadPath()
        }
    }

    Connections {
        target: root.vehicle
        function onHomePositionChanged() { root._updateHomeTerrainBias() }
    }
}
