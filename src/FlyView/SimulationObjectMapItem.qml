import QtQuick
import QtLocation
import QtPositioning

import QGroundControl
import QGroundControl.Controls

/// QtLocation marker; only missile objects with a current valid position appear.
MapQuickItem {
    id: root
    objectName: "simulationMissileMapItem"

    property var simulationObject
    property real markerSize: ScreenTools.defaultFontPixelHeight * 2
    property bool showLabel: true

    coordinate: simulationObject && simulationObject.positionValid
                ? simulationObject.coordinate : QtPositioning.coordinate()
    visible: simulationObject && simulationObject.kind === "missile" && simulationObject.positionValid
             && coordinate.isValid
    anchorPoint.x: marker.width / 2
    anchorPoint.y: marker.height / 2
    z: QGroundControl.zOrderVehicles + 0.5

    sourceItem: SimulationObjectMarker {
        id: marker
        size: root.markerSize
        objectId: root.simulationObject ? root.simulationObject.objectId : ""
        lifecycle: root.simulationObject ? root.simulationObject.lifecycle : ""
        showLabel: root.showLabel
        opacity: lifecycle === "active" ? 1 : lifecycle === "completed" ? 0.35 : 0.55
    }
}
