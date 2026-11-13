import QtQuick
import QtLocation
import QtPositioning

import QGroundControl
import QGroundControl.FlightMap

/// Geographic anchor follows native map transforms; the Canvas paints only
/// offsets relative to the anchor so pan cannot leave a stale image on screen.
MapQuickItem {
    id: root

    property var mapView
    property var trackedObject
    property bool simulated: false

    coordinate: trailHistory.renderAnchor
    anchorPoint: Qt.point(trailOverlay.width / 2, trailOverlay.height / 2)
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

    onTrackedObjectChanged: {
        trailHistory.clear()
        record()
    }
    onClearWhenStoppedChanged: checkStopPolicy()
    Component.onCompleted: record()

    MapTrailHistory {
        id: trailHistory
        enabled: root.trailEnabled && root.trailStyle !== "none"
        lifetimeMs: root.trailLifetimeMs

        onPointAppended: trailCanvas.requestPaint()
        onPathRebuilt: trailCanvas.requestPaint()
        onFadingChanged: {
            if (root.trailStyle === "fading") {
                trailCanvas.requestPaint()
            }
        }
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
        function onCenterChanged() { trailCanvas.requestPaint() }
        function onZoomLevelChanged() { trailCanvas.requestPaint() }
        function onBearingChanged() { trailCanvas.requestPaint() }
        function onTiltChanged() { trailCanvas.requestPaint() }
    }

    onTrailStyleChanged: trailCanvas.requestPaint()
    onTrailColorChanged: trailCanvas.requestPaint()
    onTrailWidthChanged: trailCanvas.requestPaint()
    onTrailOpacityChanged: trailCanvas.requestPaint()
    onVisibleChanged: trailCanvas.requestPaint()

    sourceItem: Item {
        id: trailOverlay

        // An object anchored near a viewport edge still needs to render trail
        // segments on the opposite side. The fixed bound is two viewports.
        width: root.mapView ? Math.max(1, root.mapView.width * 2) : 1
        height: root.mapView ? Math.max(1, root.mapView.height * 2) : 1

        Canvas {
            id: trailCanvas
            anchors.fill: parent
            renderTarget: Canvas.Image
            renderStrategy: Canvas.Immediate
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
    
            onPaint: {
                const ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                if (!root.visible || !root.mapView || !root.trailEnabled
                        || root.trailStyle === "none" || root.trailOpacity <= 0) {
                    return
                }
                const points = trailHistory.coordinates()
                if (points.length < 2) {
                    return
                }
                const anchor = root.mapView.fromCoordinate(root.coordinate, false)
                if (!isFinite(anchor.x) || !isFinite(anchor.y)) {
                    return
                }
                // MapQuickItem geo-anchors this image to the last retained trail sample.
                // Projection is relative to that point, not to the changing map viewport.
                const originX = width * 0.5 - anchor.x
                const originY = height * 0.5 - anchor.y
                const projected = []
                for (let i = 0; i < points.length; ++i) {
                    const point = root.mapView.fromCoordinate(points[i], false)
                    projected.push(Qt.point(point.x + originX, point.y + originY))
                }
                ctx.lineWidth = root.trailWidth
                ctx.lineCap = "round"
                ctx.lineJoin = "round"
                const c = root.trailColor
                const rgb = Math.round(c.r * 255) + "," + Math.round(c.g * 255) + ","
                          + Math.round(c.b * 255)
                const stroke = (alpha) => "rgba(" + rgb + "," + Math.max(0, Math.min(1, alpha)) + ")"
                if (root.trailStyle === "dashed") {
                    ctx.setLineDash([9, 6])
                } else if (root.trailStyle === "dotted") {
                    ctx.setLineDash([1, 5])
                } else {
                    ctx.setLineDash([])
                }
    
                if (root.trailStyle === "fading") {
                    const factors = trailHistory.fadeFactors()
                    for (let i = 1; i < projected.length; ++i) {
                        const a = projected[i - 1]
                        const b = projected[i]
                        if (!isFinite(a.x) || !isFinite(a.y) || !isFinite(b.x) || !isFinite(b.y)
                                || Math.abs(a.x - b.x) > root.mapView.width * 0.8) {
                            continue
                        }
                        if (a.x === b.x && a.y === b.y) {
                            continue
                        }
                        const gradient = ctx.createLinearGradient(a.x, a.y, b.x, b.y)
                        gradient.addColorStop(0, stroke(factors[i - 1] * root.trailOpacity))
                        gradient.addColorStop(1, stroke(factors[i] * root.trailOpacity))
                        ctx.strokeStyle = gradient
                        ctx.beginPath()
                        ctx.moveTo(a.x, a.y)
                        ctx.lineTo(b.x, b.y)
                        ctx.stroke()
                    }
                } else {
                    ctx.strokeStyle = stroke(root.trailOpacity)
                    ctx.beginPath()
                    let started = false
                    for (let i = 0; i < projected.length; ++i) {
                        const p = projected[i]
                        if (!isFinite(p.x) || !isFinite(p.y)) {
                            started = false
                            continue
                        }
                        if (!started || (i > 0 && Math.abs(p.x - projected[i - 1].x) > root.mapView.width * 0.8)) {
                            ctx.moveTo(p.x, p.y)
                        } else {
                            ctx.lineTo(p.x, p.y)
                        }
                        started = true
                    }
                    ctx.stroke()
                }
            }
        }
    }
}
