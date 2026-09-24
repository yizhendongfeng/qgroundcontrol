/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

import QtQuick          2.11
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3
import QGroundControl               1.0
import QGroundControl.ScreenTools   1.0
import QGroundControl.Palette       1.0
import QGroundControl.Controls      1.0

Rectangle {
    id:         _root
    color:      qgcPal.toolbarBackground

    width:      parent.width / 15 > _maxWidth ? _maxWidth : parent.width / 15
    property var    _maxWidth: 50
    property var    _minWidth: 10

//    width:      _idealWidth < repeater.contentWidth ? repeater.contentWidth : _idealWidth
    height:     Math.min(maxHeight, toolStripColumn.height + (flickable.anchors.margins * 2))
    radius:     ScreenTools.defaultFontPixelWidth / 2
    property alias  model:              repeater.model
    property real   maxHeight           ///< Maximum height for control, determines whether text is hidden to make control shorter
    property alias  title:              titleLabel.text
    property bool showActionText: true
    property var _dropPanel: dropPanel

    function simulateClick(buttonIndex) {
        buttonIndex = buttonIndex + 1 // skip over title label
        var button = toolStripColumn.children[buttonIndex]
        if (button.checkable) {
            button.checked = !button.checked
        }
        button.clicked()
    }

    // Ensure we don't get narrower than content
    property real _idealWidth: (ScreenTools.isMobile ? ScreenTools.minTouchPixels : ScreenTools.defaultFontPixelWidth * 8) + toolStripColumn.anchors.margins * 2

    signal dropped(int index)
    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        color: "grey"
    }
    DeadMouseArea {
        anchors.fill: parent
    }

    QGCFlickable {
        id:                 flickable
        anchors.margins:    ScreenTools.defaultFontPixelWidth * 0.4
        anchors.top:        parent.top
        anchors.left:       parent.left
        anchors.right:      parent.right
        height:             parent.height
        contentHeight:      toolStripColumn.height
        flickableDirection: Flickable.VerticalFlick
        clip:               true

        Column {
            id:             toolStripColumn
            anchors.left:   parent.left
            anchors.right:  parent.right
            spacing:        ScreenTools.defaultFontPixelWidth * 0.5

            QGCLabel {
                id:                     titleLabel
                anchors.left:           parent.left
                anchors.right:          parent.right
                horizontalAlignment:    Text.AlignHCenter
                font.pointSize:         ScreenTools.smallFontPointSize
                visible:                title != ""
                Layout.fillWidth: true
            }

            Repeater {
                id: repeater
                ToolStripHoverButton {
                    // id:                 buttonTemplate
                    anchors.left:       parent.left
                    anchors.right:      parent.right
                    height:             width
                    radius:             ScreenTools.defaultFontPixelWidth / 2
                    fontPointSize:      ScreenTools.smallFontPointSize
                    toolStripAction:    modelData
                    // dropPanel:          _dropPanel
                    // showText:           showActionText
                    onDropped:          _root.dropped(index)
                    // 互斥是手写的（autoExclusive 在这个控件上会出各种怪事），规则两条：
                    //   1. 选中某一项 → 把别的都取消
                    //   2. 点已经选中的那一项 → Qt 会先把 checked 翻成 false，互斥逻辑
                    //      只管取消别人、管不了自己，于是连点两次就变成「一项都没选中」，
                    //      页面还在，工具条上却没有任何高亮。工具条是导航，永远要有一项
                    //      选中，所以谁也没选中时把自己补回来
                    onCheckedChanged: {
                        if (checked) {
                            for (var i=0; i<repeater.count; i++) {
                                if (i !== index) {
                                    var button = repeater.itemAt(i)
                                    if (button && button.checked) {
                                        button.checked = false
                                    }
                                }
                            }
                        } else {
                            var anyChecked = false
                            for (var j=0; j<repeater.count; j++) {
                                var other = repeater.itemAt(j)
                                if (other && other.checked) {
                                    anyChecked = true
                                    break
                                }
                            }
                            if (!anyChecked) {
                                checked = true
                            }
                        }
                    }
                }
            }
        }
    }

    DropPanel {
        id:         dropPanel
        // toolStrip:  _root
    }

    Behavior on anchors.leftMargin {
        NumberAnimation { duration: 300 }
    }

    function showWidget(slipIn) {
        if (slipIn)
            anchors.leftMargin = 1
        else
            anchors.leftMargin = -width
    }

    function uncheckAllActionButton() {
        // We deal with exclusive check state manually since usinug autoExclusive caused all sorts of crazt problems
        for (var i=0; i<repeater.count; i++) {
            var button = repeater.itemAt(i)
            if (button.checked) {
                button.checked = false
            }
        }
    }

}
