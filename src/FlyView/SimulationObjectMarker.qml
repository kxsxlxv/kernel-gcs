import QtQuick
import QGroundControl.Controls

/// Read-only simulation missile marker artwork shared by both Fly View map engines.
Item {
    id: root

    property string objectId: ""
    property string lifecycle: "active"
    property real size: ScreenTools.defaultFontPixelHeight * 2
    property bool showLabel: true

    readonly property color _markerColor: lifecycle === "active" ? qgcPal.colorOrange : qgcPal.colorGrey

    width: size
    height: size

    QGCPalette { id: qgcPal; colorGroupEnabled: root.enabled }

    Rectangle {
        anchors.centerIn: parent
        width: root.size * 0.70
        height: width
        rotation: 45
        radius: root.size * 0.04
        color: root._markerColor
        border.color: qgcPal.window
        border.width: Math.max(1, root.size * 0.045)
    }

    QGCLabel {
        anchors.centerIn: parent
        text: qsTr("M", "simulation missile marker abbreviation")
        color: qgcPal.window
        font.bold: true
        font.pixelSize: root.size * 0.43
    }

    Rectangle {
        anchors.left: parent.right
        anchors.leftMargin: ScreenTools.defaultFontPixelWidth / 2
        anchors.verticalCenter: parent.verticalCenter
        width: Math.min(ScreenTools.defaultFontPixelWidth * 22, statusLabel.implicitWidth + ScreenTools.defaultFontPixelWidth)
        height: statusLabel.implicitHeight + ScreenTools.defaultFontPixelWidth / 2
        radius: ScreenTools.defaultFontPixelWidth / 3
        color: qgcPal.window
        border.color: root._markerColor
        border.width: 1
        visible: root.showLabel && root.objectId !== ""

        QGCLabel {
            id: statusLabel
            anchors.fill: parent
            anchors.leftMargin: ScreenTools.defaultFontPixelWidth / 2
            anchors.rightMargin: ScreenTools.defaultFontPixelWidth / 2
            color: qgcPal.text
            font.pointSize: ScreenTools.smallFontPointSize
            elide: Text.ElideRight
            text: root.lifecycle === "active" ? root.objectId
                  : root.objectId + " (" + (root.lifecycle === "completed" ? qsTr("complete")
                                                : root.lifecycle === "disconnected" ? qsTr("disconnected")
                                                : qsTr("stale")) + ")"
        }
    }
}
