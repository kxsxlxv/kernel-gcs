import QtQuick
import QtLocation
import QtPositioning

import QGroundControl
import QGroundControl.FlightMap

/// Native scene-graph geometry follows the same geographic transform as map
/// markers. No QImage/Canvas allocation is involved in panning or zooming.
MapQuickItem {
    id: root

    property var mapView
    property var trackedObject
    property bool simulated: false

    coordinate: trailHistory.renderAnchor
    anchorPoint: Qt.point(trailMesh.width / 2, trailMesh.height / 2)
    autoFadeIn: false

    readonly property var settings: simulated
        ? QGroundControl.settingsManager.simulationObjectSettings
        : QGroundControl.settingsManager.flightMapSettings
    readonly property bool trailEnabled: !!settings && (simulated
        ? settings.trailEnabled.rawValue : settings.vehicleTrailEnabled.rawValue)
    readonly property int trailLifetimeMs: Number(simulated
        ? settings.trailLifetimeSec.rawValue : settings.vehicleTrailLifetimeSec.rawValue) * 1000
    readonly property string trailStyle: String(simulated
        ? settings.trailStyle.rawValue : settings.vehicleTrailStyle.rawValue)
    readonly property color trailColor: simulated
        ? settings.trailColor.rawValue : settings.vehicleTrailColor.rawValue
    readonly property real trailWidth: Number(simulated
        ? settings.trailWidth.rawValue : settings.vehicleTrailWidth.rawValue)
    readonly property real trailOpacity: Number(simulated
        ? settings.trailOpacity.rawValue : settings.vehicleTrailOpacity.rawValue)
    readonly property bool clearWhenStopped: simulated
        ? settings.trailClearWhenStopped.rawValue : settings.vehicleTrailClearWhenStopped.rawValue

    function stopped() {
        if (!trackedObject) {
            return false
        }
        return simulated
            ? (trackedObject.motionState === "stationary" || trackedObject.motionState === "paused")
            : (trackedObject.groundSpeed && Number(trackedObject.groundSpeed.rawValue) < 0.5)
    }

    function checkStopPolicy() {
        if (clearWhenStopped && stopped()) {
            trailHistory.clear()
        }
    }

    function record() {
        if (!trackedObject || (simulated && !trackedObject.positionValid)) {
            return
        }
        checkStopPolicy()
        if (!(clearWhenStopped && stopped())) {
            trailHistory.append(trackedObject.coordinate)
        }
    }

    function scheduleReproject() {
        // Coalesce zoom/bearing changes to ~30 updates/s. A pure map pan
        // already moves the native MapQuickItem, so it never rebuilds vertices.
        if (!reprojectTimer.running) {
            reprojectTimer.start()
        }
    }

    function reproject() {
        const points = trailHistory.coordinates()
        if (!mapView || points.length < 2 || !root.coordinate.isValid) {
            trailMesh.setProjectedPath([])
            return
        }
        let anchor = mapView.fromCoordinate(root.coordinate, false)
        if (!isFinite(anchor.x) || !isFinite(anchor.y)) {
            trailMesh.setProjectedPath([])
            return
        }

        // Unlike the previous last-sample anchor, this anchor stays fixed as
        // new telemetry arrives. Rebase only when the trajectory has travelled
        // far enough to approach the GPU item's bounds. Anchor and geometry
        // update together in this synchronous function.
        const newest = mapView.fromCoordinate(points[points.length - 1], false)
        if (isFinite(newest.x) && isFinite(newest.y)
                && (Math.abs(newest.x - anchor.x) > mapView.width * 0.75
                    || Math.abs(newest.y - anchor.y) > mapView.height * 0.75)) {
            trailHistory.rebaseToLatest()
            // Read the updated value directly; the QML map-item binding is
            // notified in this same event loop before the next frame.
            anchor = mapView.fromCoordinate(trailHistory.renderAnchor, false)
            if (!isFinite(anchor.x) || !isFinite(anchor.y)) {
                trailMesh.setProjectedPath([])
                return
            }
        }
        const halfWidth = trailMesh.width / 2
        const halfHeight = trailMesh.height / 2
        const projected = []
        let previousPixel = null
        for (let i = 0; i < points.length; ++i) {
            const p = mapView.fromCoordinate(points[i], false)
            if (!isFinite(p.x) || !isFinite(p.y)
                    || (previousPixel && isFinite(previousPixel.x)
                        && Math.abs(p.x - previousPixel.x) > mapView.width * 0.8)) {
                projected.push(Qt.point(NaN, NaN))
            } else {
                projected.push(Qt.point(p.x - anchor.x + halfWidth, p.y - anchor.y + halfHeight))
            }
            previousPixel = p
        }
        trailMesh.setProjectedPath(projected)
        if (trailStyle === "fading") {
            trailMesh.setFadeFactors(trailHistory.fadeFactors())
        }
    }

    onTrackedObjectChanged: {
        trailHistory.clear()
        record()
        scheduleReproject()
    }
    onClearWhenStoppedChanged: checkStopPolicy()
    onMapViewChanged: scheduleReproject()
    onTrailStyleChanged: scheduleReproject()
    Component.onCompleted: {
        record()
        scheduleReproject()
    }

    MapTrailHistory {
        id: trailHistory
        enabled: root.trailEnabled && root.trailStyle !== "none"
        lifetimeMs: root.trailLifetimeMs

        onPointAppended: root.scheduleReproject()
        onPathRebuilt: root.scheduleReproject()
        onFadingChanged: {
            if (root.trailStyle === "fading") {
                trailMesh.setFadeFactors(trailHistory.fadeFactors())
            }
        }
    }

    Timer {
        id: reprojectTimer
        interval: 33
        repeat: false
        onTriggered: root.reproject()
    }

    Connections {
        target: root.trackedObject
        ignoreUnknownSignals: true
        function onCoordinateChanged() { root.record() }
        function onChanged() { root.record() }
    }

    Connections {
        target: root.simulated || !root.trackedObject ? null : root.trackedObject.groundSpeed
        ignoreUnknownSignals: true
        function onRawValueChanged() { root.checkStopPolicy() }
    }

    Connections {
        target: root.mapView
        ignoreUnknownSignals: true
        function onZoomLevelChanged() { root.scheduleReproject() }
        function onBearingChanged() { root.scheduleReproject() }
        function onTiltChanged() { root.scheduleReproject() }
        function onWidthChanged() { root.scheduleReproject() }
        function onHeightChanged() { root.scheduleReproject() }
    }

    sourceItem: MapTrailSceneGraph {
        id: trailMesh
        // This is scene-graph geometry, not an image buffer: a larger logical
        // item has negligible memory cost and avoids clipping during rebasing.
        width: root.mapView ? Math.max(1, root.mapView.width * 4) : 1
        height: root.mapView ? Math.max(1, root.mapView.height * 4) : 1
        strokeColor: root.trailColor
        lineWidth: root.trailWidth
        lineOpacity: root.trailOpacity
        lineStyle: root.trailStyle
    }
}
