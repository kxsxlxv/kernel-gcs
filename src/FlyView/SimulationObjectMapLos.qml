import QtQuick
import QtLocation

import QGroundControl
import QGroundControl.FlyView

/// Read-only link between an active simulator marker and an explicitly bound live aircraft.
MapPolyline {
    id: root
    property var simulationObject

    SimulationObjectLosEndpoints {
        id: endpoints
        simulationObject: root.simulationObject
    }

    path: endpoints.drawable
          ? [root.simulationObject.coordinate, endpoints.targetVehicle.coordinate] : []
    visible: endpoints.drawable
    line.color: "#80cfff"
    line.width: 2
    opacity: 0.6
    autoFadeIn: false
    // Always behind Vehicle and missile symbols, regardless of model order.
    z: QGroundControl.zOrderTrajectoryLines
}
