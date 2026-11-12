import QtQuick
import QtQuick3D
import QtPositioning

import QGroundControl
import QGroundControl.Controls
import QGroundControl.GeoMap

/// GeoMap 2D/3D simulation missile marker, independent of MAVLink Vehicle.
GeoMapItem {
    id: root
    objectName: "simulationMissileGeoMapItem"

    property var simulationObject
    property real markerSize: ScreenTools.defaultFontPixelHeight * 2.5
    property bool showLabel: true
    property real homeTerrainBias: 0

    // Only the known AMSL datum can share the vehicle's DEM/home altitude correction.
    // Ellipsoidal or unspecified heights are shown at ground level until datum conversion exists.
    readonly property bool _hasAmslAltitude: simulationObject && simulationObject.altitudeDatum === "AMSL_EGM96"
    readonly property var _camera: scene ? scene.camera : null
    readonly property real _quadScale: _camera
        ? markerSize * _camera.distance * _camera.unitsPerPixelAtUnitDistance / 100 : 1
    readonly property real _statusOpacity: simulationObject && simulationObject.lifecycle === "active" ? 1
        : simulationObject && simulationObject.lifecycle === "completed" ? 0.35 : 0.55

    coordinate: {
        if (!simulationObject || !simulationObject.positionValid) {
            return QtPositioning.coordinate()
        }
        const pos = simulationObject.coordinate
        return _hasAmslAltitude
            ? QtPositioning.coordinate(pos.latitude, pos.longitude, pos.altitude + homeTerrainBias)
            : QtPositioning.coordinate(pos.latitude, pos.longitude)
    }
    visible: simulationObject && simulationObject.kind === "missile" && simulationObject.positionValid
             && coordinate.isValid
    altitudeMode: _hasAmslAltitude ? GeoMapItem.Absolute : GeoMapItem.ClampToGround
    width: markerSize
    height: markerSize
    anchorPoint: Qt.point(width / 2, height / 2)
    node3DOpacity: _statusOpacity
    z: QGroundControl.zOrderVehicles + 0.5

    SimulationObjectMarker {
        anchors.fill: parent
        size: root.markerSize
        objectId: root.simulationObject ? root.simulationObject.objectId : ""
        lifecycle: root.simulationObject ? root.simulationObject.lifecycle : ""
        showLabel: root.showLabel
        heading: root.simulationObject && root.simulationObject.headingValid
                 ? root.simulationObject.heading + (root._camera ? root._camera.heading : 0) : 0
        headingValid: root.simulationObject ? root.simulationObject.headingValid : false
        opacity: root.contentOpacity2D * root._statusOpacity
    }

    delegate3D: Component {
        Node {
            // Scene z is up; positive geographic heading is clockwise.
            eulerRotation.z: root.simulationObject && root.simulationObject.headingValid
                             ? -root.simulationObject.heading : 0
            scale: Qt.vector3d(root._quadScale, root._quadScale, 1)

            Model {
                source: "#Rectangle"

                materials: PrincipledMaterial {
                    lighting: PrincipledMaterial.NoLighting
                    alphaMode: PrincipledMaterial.Blend
                    cullMode: Material.NoCulling
                    baseColorMap: Texture {
                        sourceItem: SimulationObjectMarker {
                            size: 96
                            showLabel: false
                            headingValid: true  // The containing 3D Node supplies heading rotation.
                            lifecycle: root.simulationObject ? root.simulationObject.lifecycle : ""
                        }
                    }
                }
            }
        }
    }
}
