import QtQuick          2.3
import QtQuick.Controls 1.2

import QGroundControl                   1.0
import QGroundControl.Controls          1.0
import QGroundControl.ScreenTools       1.0
import QGroundControl.FlightMap         1.0

Rectangle {
    property var mainRootWindow
    property var borderColor: qgcPal.colorGrey
    property var rightPanelWidth
    id: planViewRightPannel
    anchors.right:  parent.right
    anchors.top: parent.top
    anchors.bottom: parent.bottom
    height: parent.height
    width: rightPanelWidth
    color: qgcPal.window
    DeadMouseArea {
        anchors.fill: parent
    }

    Rectangle {
        id: leftBorder
        width: 1
        color: borderColor
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
    }
    Rectangle {
        id: topBorder
        height: 1
        color: borderColor
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
    }
}
