/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

import QtQuick
import QtQuick.Layouts

import QGroundControl.Controls
import QGroundControl.ScreenTools

QGCPopupDialog {
    property alias  text:           label.text
    property alias  inputText:      textFieldInput.text
    property var    acceptFunction: null        // Mainly used by MainRootWindow.showMessage to specify accept function in call
    property bool   enableInput:    false

    onAccepted: {
        if (acceptFunction) {
            acceptFunction()
        }
    }

    ColumnLayout {
        spacing: 10
        QGCLabel {
            id:                     label
            Layout.preferredWidth:  Math.max(mainWindow.width / (ScreenTools.isMobile ? 2 : 3), headerMinWidth)
            wrapMode:               Text.WordWrap
        }
        QGCTextField {
            id:                     textFieldInput
            visible:                enableInput
            Layout.fillWidth:       true
            text:                   qsTr("新建航线任务")
        }
    }
}
