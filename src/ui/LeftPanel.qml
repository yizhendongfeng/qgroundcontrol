import QtQuick          2.3
import QtQuick.Controls 1.2

import QGroundControl                   1.0
import QGroundControl.Controls          1.0
import QGroundControl.ScreenTools       1.0
import QGroundControl.FlightMap         1.0

Rectangle {
    property var mainRootWindow
    id: toolStripLeftPannel
    anchors.left:  parent.left
    anchors.top: parent.top
    color: qgcPal.windowShade
    property color _splitLineColor: "#707070"
    DeadMouseArea {
        anchors.fill: parent
    }

    Rectangle {
        id: leftBorder
        width: 1
        color: _splitLineColor
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
    }
    Rectangle {
        id: topBorder
        height: 1
        color: _splitLineColor
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
    }
}
