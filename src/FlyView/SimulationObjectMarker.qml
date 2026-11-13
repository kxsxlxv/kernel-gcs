import QtQuick
import QtQuick.Shapes

import QGroundControl
import QGroundControl.Controls

/// Read-only simulation missile marker artwork shared by both Fly View map engines.
Item {
    id: root

    property string objectId: ""
    property string lifecycle: "active"
    property real size: ScreenTools.defaultFontPixelHeight * 2.5
    property bool showLabel: true
    property real heading: 0        // Clockwise degrees from north; icon nose is up at 0
    property bool headingValid: false
    property bool rangeValid: false
    property real slantRangeM: NaN

    // Show the numeric suffix for canonical missile IDs, preserving the full
    // objectId in the model for identity, logs, and session tracking.
    readonly property string displayNumber: {
        const match = /^missile[-_](\d+)$/i.exec(objectId)
        return match ? match[1] : objectId
    }

    readonly property string reportedRangeText: {
        if (!rangeValid || !isFinite(slantRangeM) || slantRangeM < 0) {
            return ""
        }
        // Use spaces as grouping separators, independent of the user's locale.
        return Math.round(slantRangeM).toString().replace(/\B(?=(\d{3})+(?!\d))/g, " ") + qsTr(" m")
    }

    readonly property color _markerColor: lifecycle === "active" ? qgcPal.colorOrange : qgcPal.colorGrey

    width: size
    height: size

    QGCPalette { id: qgcPal; colorGroupEnabled: root.enabled }

    // North-up missile silhouette. Rotate the artwork, never the object ID label.
    Item {
        id: missileIcon
        anchors.centerIn: parent
        width: root.size
        height: root.size
        rotation: root.headingValid ? root.heading : 0

        Shape {
            anchors.centerIn: parent
            width: 64
            height: 96
            scale: root.size / 96
            preferredRendererType: Shape.CurveRenderer

            ShapePath {
                strokeColor: qgcPal.window
                strokeWidth: 3.5
                fillColor: root._markerColor

                PathSvg {
                    // Ogive nose, narrow fuselage, mid-body wings, and rear stabilizers.
                    path: "M 32 3 C 36 10 39 17 39 25 L 39 40 L 51 56 L 51 66 "
                          + "L 39 61 L 39 77 L 49 87 L 49 94 L 32 87 "
                          + "L 15 94 L 15 87 L 25 77 L 25 61 L 13 66 "
                          + "L 13 56 L 25 40 L 25 25 C 25 17 28 10 32 3 Z"
                }
            }

            ShapePath {
                strokeWidth: -1
                fillColor: qgcPal.window

                PathSvg {
                    path: "M 30 21 L 34 21 L 34 74 L 30 74 Z "
                          + "M 26 39 L 38 39 L 38 42 L 26 42 Z "
                          + "M 26 52 L 38 52 L 38 55 L 26 55 Z"
                }
            }
        }
    }

    // Number only; lifecycle remains encoded by the missile artwork color.
    Column {
        anchors.left: parent.right
        anchors.leftMargin: ScreenTools.defaultFontPixelWidth / 2
        anchors.verticalCenter: parent.verticalCenter
        spacing: ScreenTools.defaultFontPixelWidth / 4
        visible: root.showLabel && root.objectId !== ""

        QGCLabel {
            id: statusLabel
            width: Math.min(implicitWidth, ScreenTools.defaultFontPixelWidth * 22)
            color: "white"
            opacity: 0.9
            font.pointSize: ScreenTools.smallFontPointSize
            elide: Text.ElideRight
            text: root.displayNumber
        }

        Rectangle {
            id: rangeBadge
            visible: root.reportedRangeText !== ""
            width: rangeLabel.implicitWidth + ScreenTools.defaultFontPixelWidth
            height: rangeLabel.implicitHeight + ScreenTools.defaultFontPixelWidth / 2
            radius: ScreenTools.defaultFontPixelWidth / 3
            color: Qt.rgba(0, 0, 0, 0.84)

            QGCLabel {
                id: rangeLabel
                anchors.centerIn: parent
                color: "white"
                font.pointSize: ScreenTools.smallFontPointSize
                font.bold: true
                text: root.reportedRangeText
            }
        }
    }

}
