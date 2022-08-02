import QtQuick                  2.3
import QtQuick.Controls         2.12
import QtQuick.Controls.Styles  1.4

import QGroundControl.Palette 1.0
import QGroundControl.ScreenTools 1.0

Button {
    id:             control
    hoverEnabled:   true
    focusPolicy:    Qt.ClickFocus
    width:          ScreenTools.implicitButtonHeight
    height: width
    property string iconSource
    property alias sourceSize: icon.sourceSize
    property bool  showNormal: pressed | hovered | checked // _showNormal, true: 灰色（不可交互或状态改变），false: 高亮（表示可交互）
    QGCPalette { id: qgcPal; colorGroupEnabled: enabled }

    contentItem: Item {
        anchors.fill: parent
        QGCColoredImage {
            id:                     icon
            anchors.fill:           parent
            source:                 control.iconSource
            color:                  showNormal ? qgcPal.colorGrey : qgcPal.text
            fillMode:               Image.PreserveAspectFit
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            antialiasing:       true
        }
    }
    background: Rectangle {
        anchors.fill: parent
        color: "transparent"
    }
}
