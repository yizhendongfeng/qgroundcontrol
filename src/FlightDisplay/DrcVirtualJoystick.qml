/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

import QtQuick
import QtQuick.Layouts

import QGroundControl
import QGroundControl.ScreenTools
import QGroundControl.Controls
import QGroundControl.Palette

/// 云端遥控中的状态条（DRC）
///
/// 后台（网页端遥控器）拿到飞行控制权时出现在飞行视图底部，控制权收回 / DRC 退出时消失。
///
/// **这里曾经自绘过两块摇杆，现在没有了。** 原因是那些杆量已经有地方显示：QGC 自带的
/// 屏幕虚拟摇杆（`VirtualJoystick.qml`）在云端持权期间本身就是云端的显示器 ——
/// 把手随后台下发的杆量走、同时不接受触摸。再画一套只会屏占地方并给出第二个真相。
///
/// 所以本组件现在只负责**说清楚状态**：谁在控、以及"你拖不动摇杆"这件事是设计如此，
/// 免得操作员以为界面卡了。
Rectangle {
    id:                 root

    /// DjiDrcClient（由 DjiBridgeServer 暴露成 context property）
    readonly property var drc: djiBridgeServer.djiDrc

    /// 出现与消失只由这一个绑定驱动
    visible:            drc && drc.cloudFlightAuthority

    readonly property real _margin: ScreenTools.defaultFontPixelWidth / 2

    implicitWidth:  _column.implicitWidth + _margin * 2
    implicitHeight: _column.implicitHeight + _margin * 2
    color:          qgcPal.windowShade
    opacity:        0.92
    radius:         ScreenTools.defaultFontPixelWidth / 2
    border.width:   1
    border.color:   qgcPal.colorOrange

    QGCPalette { id: qgcPal }

    /// 吃掉本块范围内的点击与滚轮。没有它，点击会穿透到下面的地图上。
    DeadMouseArea {
        anchors.fill: parent
    }

    ColumnLayout {
        id:                 _column
        anchors.fill:       parent
        anchors.margins:    root._margin
        spacing:            root._margin

        RowLayout {
            Layout.fillWidth: true
            spacing:          root._margin

            Rectangle {
                Layout.alignment: Qt.AlignVCenter
                width:            ScreenTools.defaultFontPixelHeight * 0.6
                height:           width
                radius:           width / 2
                color:            qgcPal.colorOrange
            }

            QGCLabel {
                Layout.fillWidth: true
                color:            qgcPal.buttonText
                text:             qsTr("云端遥控中") + " · " + (root.drc ? root.drc.authUserCallsign : "")
            }
        }

        QGCLabel {
            Layout.fillWidth:   true
            wrapMode:           Text.WordWrap
            font.pointSize:     ScreenTools.smallFontPointSize
            color:              qgcPal.colorOrange
            text:               qsTr("摇杆由云端驱动，本机输入已锁定（拖不动是正常的）。")
        }
    }
}
