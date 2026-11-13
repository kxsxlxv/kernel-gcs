import QtQuick
import QtQuick3D
import QtPositioning

import QGroundControl
import QGroundControl.FlyView
import QGroundControl.GeoMap

/// Terrain-aware LOS visualization with no target prediction or range computation.
GeoMapItem {
    id: root

    property var simulationObject
    property real missileHomeTerrainBias: 0
    property var _geometry: null

    readonly property var _camera: scene ? scene.camera : null
    readonly property real _screenFactor: _camera ? _camera.unitsPerPixelAtUnitDistance : 0
    readonly property var losCoordinates: {
        if (!endpoints.drawable) {
            return []
        }
        const missile = simulationObject.coordinate
        const target = endpoints.targetVehicle.coordinate
        let missileHeight = missile.altitude + root.missileHomeTerrainBias
        if (simulationObject.altitudeDatum !== "AMSL_EGM96") {
            missileHeight = surfaceModel ? surfaceModel.terrainHeightAt(missile) : NaN
        }
        let targetBias = 0
        const home = endpoints.targetVehicle.homePosition
        if (surfaceModel && home.isValid && isFinite(home.altitude)) {
            const terrainHome = surfaceModel.terrainDataHeightAt(home)
            if (isFinite(terrainHome)) {
                targetBias = terrainHome - home.altitude
            }
        }
        const targetHeight = target.altitude + targetBias
        if (!isFinite(missileHeight) || !isFinite(targetHeight)) {
            return []
        }
        return [
            QtPositioning.coordinate(missile.latitude, missile.longitude, missileHeight),
            QtPositioning.coordinate(target.latitude, target.longitude, targetHeight)
        ]
    }

    SimulationObjectLosEndpoints {
        id: endpoints
        simulationObject: root.simulationObject
    }

    altitudeMode: GeoMapItem.Absolute
    crossfade3D: false
    coordinate: _geometry && _geometry.anchorCoordinate.isValid
                ? _geometry.anchorCoordinate : QtPositioning.coordinate()
    visible: endpoints.drawable && losCoordinates.length === 2 && !!_geometry
             && _geometry.pointCount === 2
    node3DOpacity: 1

    function _reload() {
        if (!root._geometry) {
            return
        }
        if (root.losCoordinates.length === 2) {
            root._geometry.setPath(root.losCoordinates)
        } else {
            root._geometry.clearPath()
        }
    }

    onLosCoordinatesChanged: _reload()
    onSurfaceModelChanged: _reload()

    Connections {
        target: root.surfaceModel
        ignoreUnknownSignals: true
        function onTerrainDataChanged() { root._reload() }
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
                    // These shader resources belong to the GeoMap QML module,
                    // not this component's FlyView resource directory.
                    vertexShader: "qrc:/qml/QGroundControl/GeoMap/shaders/flightpath.vert"
                    fragmentShader: "qrc:/qml/QGroundControl/GeoMap/shaders/flightpath.frag"

                    property real lineWidth: 2
                    property real screenFactor: root._screenFactor
                    property color pathColor: "#80cfff"
                    property color highlightColor: "#80cfff"
                    property real pathOpacity: 0.6
                    property real trailStyle: 1.0 // Dashed
                    property real depthPull: root.scene ? 0.5 * (1.0 - root.scene.terrainScale) : 0.0
                }
                Component.onCompleted: {
                    root._geometry = pathGeometry
                    root._reload()
                }
                Component.onDestruction: root._geometry = null
            }
        }
    }
}
