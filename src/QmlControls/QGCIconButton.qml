import QtQuick                  2.3
import QtQuick.Controls         2.12
import QtQuick.Controls.Styles  1.4

import QGroundControl.Palette 1.0
import QGroundControl.ScreenTools 1.0

Button {
    id:             control
    hoverEnabled:   true
    focusPolicy:    Qt.ClickFocus
    width: ScreenTools.defaultFontPixelWidth * 2.5
    height: width
    property string iconSource
    property bool   _showHighlight:     pressed | hovered | checked
    QGCPalette { id: qgcPal; colorGroupEnabled: enabled }

    contentItem: Item {
        anchors.fill: parent
        QGCColoredImage {
            id:                     icon
            anchors.fill:           parent
            source:                 control.iconSource
//            width:             ScreenTools.defaultFontPixelWidth * 2
            color:                  _showHighlight ? qgcPal.text : qgcPal.colorGrey
            fillMode:               Image.PreserveAspectFit
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
        }
    }
    background: Rectangle {
        anchors.fill: parent
        color: "transparent"
    }
}
