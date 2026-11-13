import QtQml

import QGroundControl

/// Resolves an explicitly provided simulator binding to a live MAVLink Vehicle.
/// The Vehicle model has no source_key, so only sysid/compid are cross-checked.
QtObject {
    id: root
    property var simulationObject

    readonly property var _vehicles: QGroundControl.multiVehicleManager.vehicles
    readonly property int _vehicleCount: _vehicles ? _vehicles.count : 0
    readonly property var targetVehicle: {
        const count = root._vehicleCount
        const object = root.simulationObject
        if (!object || !object.targetBindingValid || !object.targetValid || count === 0) {
            return null
        }
        const vehicle = QGroundControl.multiVehicleManager.getVehicleById(object.targetSysId)
        return vehicle && object.matchesTargetVehicle(vehicle) ? vehicle : null
    }
    readonly property bool drawable: {
        const object = root.simulationObject
        const target = root.targetVehicle
        return !!object && object.kind === "missile" && object.lifecycle === "active"
               && object.positionValid && object.coordinate.isValid && !!target
               && target.coordinate.isValid
    }
}
