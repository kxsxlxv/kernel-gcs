import QtQuick
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls

/// Read-only simulator health display. A configured receiver does not imply packet reception.
Rectangle {
    id: root
    objectName: "simulationTelemetryStatus"

    property var objectsModel
    readonly property int objectCount: objectsModel ? objectsModel.count : 0

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
            color: root.objectCount > 0 ? qgcPal.colorGreen : qgcPal.colorYellow
        }

        QGCLabel {
            text: qsTr("SIM")
            color: qgcPal.text
            font.bold: true
            font.pointSize: ScreenTools.smallFontPointSize
        }

        QGCLabel {
            text: qsTr("%1 objects").arg(root.objectCount)
            color: qgcPal.text
            font.pointSize: ScreenTools.smallFontPointSize
        }
    }
}
