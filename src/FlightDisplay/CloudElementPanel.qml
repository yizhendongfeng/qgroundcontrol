/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGroundControl
import QGroundControl.ScreenTools
import QGroundControl.Controls
import QGroundControl.Palette

/// 飞行视图上的「云元素」浮动面板
///
/// 只包一层标题栏 + 可折叠，编辑器本体复用规划视图那一份 CloudElementEditor。
/// 必须放在地图外面（不在 indicatorDrawer 里）：标绘要点地图，而那个抽屉
/// 是 modal + CloseOnPressOutside，点一下地图自己就关了。
Rectangle {
    id:                 root

    /// 用于编辑器里的「定位」按钮
    property var    flightMap

    property bool   expanded: true

    readonly property real _margin:     ScreenTools.defaultFontPixelWidth / 2
    readonly property real _listHeight: ScreenTools.defaultFontPixelHeight * 20

    // QGCFlickable 没有从内容推出来的 implicitHeight，编辑器的高度得显式算，
    // 不然 Layout 里塌成 0（面板只剩标题栏）。
    readonly property real _editorHeight: expanded ? Math.min(editor.contentHeight, _listHeight) : 0

    implicitWidth:  expanded ? ScreenTools.defaultFontPixelWidth * 26 : headerRow.implicitWidth + (_margin * 2)
    implicitHeight: headerRow.implicitHeight + _editorHeight + (_margin * 3)
    color:          qgcPal.windowShade
    radius:         ScreenTools.defaultFontPixelWidth / 2
    border.width:   1
    border.color:   qgcPal.text

    QGCPalette { id: qgcPal }

    ColumnLayout {
        anchors.fill:       parent
        anchors.margins:    root._margin
        spacing:            root._margin

        RowLayout {
            id:             headerRow
            Layout.fillWidth: true
            spacing:        root._margin

            QGCColoredImage {
                Layout.preferredWidth:  ScreenTools.defaultFontPixelHeight
                Layout.preferredHeight: ScreenTools.defaultFontPixelHeight
                source:                 "/InstrumentValueIcons/cloud.svg"
                fillMode:               Image.PreserveAspectFit
                sourceSize.height:      height
                color:                  djiBridgeServer.cloudWsConnected ? qgcPal.buttonText : qgcPal.colorGrey
            }

            QGCLabel {
                Layout.fillWidth:   true
                color:              qgcPal.buttonText
                text:               qsTr("云元素") + " (" + djiBridgeServer.cloudMapCount + ")"
            }

            QGCButton {
                _horizontalPadding: 0
                text:               root.expanded ? qsTr("收起") : qsTr("展开")
                onClicked:          root.expanded = !root.expanded
            }
        }

        CloudElementEditor {
            id:                 editor
            Layout.fillWidth:   true
            Layout.preferredHeight: root._editorHeight
            visible:            root.expanded
            compactMode:        true
            flightMap:          root.flightMap
        }
    }
}
