import QtQuick
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls

/// Read-only receiver status. Tracks socket state and accepted session freshness.
Rectangle {
    id: root
    objectName: "simulationTelemetryStatus"

    property var objectsModel
    property var receiver

    readonly property int objectCount: objectsModel ? objectsModel.count : 0
    readonly property string receiverStatus: receiver ? receiver.receiverStatus : "disabled"
    readonly property string statusText: {
        switch (receiverStatus) {
        case "listening": return qsTr("Listening")
        case "receiving": return qsTr("Receiving")
        case "stale": return qsTr("Stale")
        case "error": return qsTr("Socket error")
        default: return qsTr("Disabled")
        }
    }

    width: statusRow.implicitWidth + ScreenTools.defaultFontPixelWidth * 2
    height: statusRow.implicitHeight + ScreenTools.defaultFontPixelWidth
    radius: ScreenTools.defaultFontPixelWidth * 0.4
    color: qgcPal.window
    border.color: qgcPal.mapWidgetBorderDark
    border.width: 1
    opacity: 0.93

    QGCPalette { id: qgcPal; colorGroupEnabled: root.enabled }

    RowLayout {
        id: statusRow
        anchors.centerIn: parent
        spacing: ScreenTools.defaultFontPixelWidth * 0.5

        Rectangle {
            width: ScreenTools.defaultFontPixelWidth * 0.7
            height: width
            radius: width / 2
            color: root.receiverStatus === "receiving" ? qgcPal.colorGreen
                 : root.receiverStatus === "error" ? qgcPal.colorRed
                 : root.receiverStatus === "stale" ? qgcPal.colorYellow
                 : qgcPal.colorGrey
        }

        QGCLabel {
            text: qsTr("SIM")
            color: qgcPal.text
            font.bold: true
            font.pointSize: ScreenTools.smallFontPointSize
        }

        QGCLabel {
            text: root.statusText
            color: qgcPal.text
            font.pointSize: ScreenTools.smallFontPointSize
        }

        QGCLabel {
            text: qsTr("%1 objects").arg(root.objectCount)
            color: qgcPal.text
            font.pointSize: ScreenTools.smallFontPointSize
        }
    }
}
