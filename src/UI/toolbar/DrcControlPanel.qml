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

/// 指令飞行 / 远程控制（DRC）面板
///
/// 显示 DRC 第二路连接的状态、控制权归属、上行统计，并提供三类本地动作：
///   1. 抢/放飞行控制权与负载控制权（和云端对称的本地侧操作）
///   2. 紧急停桨 —— 危险动作，两次点击确认
///   3. 本地虚拟摇杆 —— 台架校方向用，不经过 MQTT
///
/// **本组件不处理任何信号**：云端的接管/停桨弹窗已经搬到 `DrcRequestDialogs`（那样才只有一份）。
/// 所以这里可以随便惰性加载 —— 塞进 `active: false` 的 Loader 也不会漏掉任何请求。
///
/// 面板只做显示与转发，所有判断都在 DjiDrcClient 里 —— 那边拒了才不会动飞机。
Rectangle {
    id:                 root

    /// DjiDrcClient（由 DjiBridgeServer 暴露成 context property）
    readonly property var drc: djiBridgeServer.djiDrc

    property bool   expanded:           true
    property bool   showDebug:          false
    /// 紧急停桨按钮的一次点击只是"上膛"，再点一次才真的发
    property bool   _stopArmed:         false

    readonly property real _margin:      ScreenTools.defaultFontPixelWidth / 2

    /// 云端正在操控 → 本地一律不许发杆量。飞行视图上的只读摇杆是"点不动"的，
    /// 这里这个开关管的是面板里那组调试滑杆 —— 两条本地入口都要堵上。
    readonly property bool _localLocked: !!drc && drc.cloudFlightAuthority

    /// 连接状态文案。0 未连接 / 1 连接中 / 2 已连接（drcState 的取值）
    readonly property string _stateText: !drc ? qsTr("桥未就绪")
                                       : drc.drcState === 2 ? qsTr("已连接")
                                       : drc.drcState === 1 ? qsTr("连接中…")
                                       : qsTr("未连接")
    readonly property color _stateColor: drc && drc.drcState === 2 ? qgcPal.colorGreen
                                       : drc && drc.drcState === 1 ? qgcPal.colorOrange
                                       : qgcPal.colorGrey

    implicitWidth:  ScreenTools.defaultFontPixelWidth * 10
    implicitHeight: _column.implicitHeight + (_margin * 2)
    color:          qgcPal.windowShade
    radius:         ScreenTools.defaultFontPixelWidth / 2
    border.width:   1
    border.color:   qgcPal.text

    QGCPalette { id: qgcPal }

    /// pushStats 里取一个计数；键不存在时给 0 —— 别让面板显示成空白
    function _stat(key) {
        if (!drc) {
            return 0
        }
        var stats = drc.pushStats
        return stats[key] === undefined ? 0 : stats[key]
    }

    Timer {
        id:          disarmTimer
        interval:    4000
        onTriggered: root._stopArmed = false
    }

    ColumnLayout {
        id:                 _column
        anchors.fill:       parent
        anchors.margins:    root._margin
        spacing:            root._margin

        // ---------------- 标题栏 ----------------
        RowLayout {
            Layout.fillWidth: true
            spacing:          root._margin

            Rectangle {
                width:          ScreenTools.defaultFontPixelHeight * 0.7
                height:         width
                radius:         width / 2
                color:          root._stateColor
            }

            QGCLabel {
                Layout.fillWidth:   true
                color:              qgcPal.buttonText
                text:               qsTr("指令飞行") + " · " + root._stateText
            }

            QGCButton {
                _horizontalPadding: 0
                text:               root.expanded ? qsTr("收起") : qsTr("展开")
                onClicked:          root.expanded = !root.expanded
            }
        }

        // ---------------- 状态 ----------------
        GridLayout {
            Layout.fillWidth:   true
            visible:            root.expanded
            columns:            2
            columnSpacing:      root._margin
            rowSpacing:         root._margin / 2

            QGCLabel { text: qsTr("Broker");        color: qgcPal.colorGrey; font.pointSize: ScreenTools.smallFontPointSize }
            QGCLabel {
                Layout.fillWidth:   true
                text:               (drc && drc.brokerAddress.length > 0) ? drc.brokerAddress : "—"
                elide:              Text.ElideMiddle
                font.pointSize:     ScreenTools.smallFontPointSize
            }

            QGCLabel { text: qsTr("下行静默");       color: qgcPal.colorGrey; font.pointSize: ScreenTools.smallFontPointSize }
            QGCLabel {
                text:               drc ? (drc.downSilenceMs + " ms") : "—"
                font.pointSize:     ScreenTools.smallFontPointSize
                // 超过 1 秒就变色：这条数字就是断链的第一眼判据
                color:              (drc && drc.downSilenceMs > 1000) ? qgcPal.colorOrange : qgcPal.text
            }

            QGCLabel { text: qsTr("上行频率");       color: qgcPal.colorGrey; font.pointSize: ScreenTools.smallFontPointSize }
            QGCLabel {
                text:               drc ? (qsTr("osd %1Hz / hsi %2Hz").arg(drc.osdFrequency).arg(drc.hsiFrequency)) : "—"
                font.pointSize:     ScreenTools.smallFontPointSize
            }
        }

        // ---------------- 控制权 ----------------
        ColumnLayout {
            Layout.fillWidth: true
            visible:          root.expanded
            spacing:          root._margin / 2

            QGCLabel {
                Layout.fillWidth:   true
                font.pointSize:     ScreenTools.smallFontPointSize
                color:              qgcPal.text
                text:               qsTr("控制权")
            }

            RowLayout {
                Layout.fillWidth: true
                spacing:          root._margin

                QGCLabel {
                    Layout.fillWidth: true
                    font.pointSize:   ScreenTools.smallFontPointSize
                    text:             qsTr("飞行：%1").arg(drc && drc.cloudFlightAuthority ? qsTr("云端") : qsTr("本机"))
                    color:            (drc && drc.cloudFlightAuthority) ? qgcPal.colorOrange : qgcPal.text
                }

                QGCButton {
                    text:       (drc && drc.cloudFlightAuthority) ? qsTr("收回") : qsTr("交给云端")
                    onClicked:  drc.drcGrabFlightAuthority(!drc.cloudFlightAuthority)
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing:          root._margin

                QGCLabel {
                    Layout.fillWidth: true
                    font.pointSize:   ScreenTools.smallFontPointSize
                    text:             qsTr("负载：%1").arg(drc && drc.cloudPayloadAuthority ? qsTr("云端") : qsTr("本机"))
                    color:            (drc && drc.cloudPayloadAuthority) ? qgcPal.colorOrange : qgcPal.text
                }

                QGCButton {
                    text:       (drc && drc.cloudPayloadAuthority) ? qsTr("收回") : qsTr("交给云端")
                    onClicked:  drc.drcGrabPayloadAuthority(!drc.cloudPayloadAuthority)
                }
            }

            // 持权但飞机不吃手动输入 —— 这是"云端显示在控制、飞机一动不动"的唯一解释。
            // 不显出来就是一个静默故障：界面上所有数字都正常（台架上 1726 条 327002 就这么来的）。
            QGCLabel {
                Layout.fillWidth:   true
                visible:            drc && drc.cloudFlightAuthority && !drc.manualControlReady
                wrapMode:           Text.WordWrap
                font.pointSize:     ScreenTools.smallFontPointSize
                color:              qgcPal.colorRed
                text:               qsTr("云端已持权，但飞机当前模式不吃手动输入 —— 云端指令会被逐条拒掉。请切到 Position / Loiter。")
            }
        }

        // ---------------- 云端杆量镜像 ----------------
        // 与飞行视图底部那块只读摇杆同源（都是 DjiDrcClient::cloudStick）。
        // 这里给出数字，摇杆给出姿态，两边互为印证。
        ColumnLayout {
            Layout.fillWidth: true
            visible:          root.expanded && root._localLocked
            spacing:          root._margin / 2

            QGCLabel {
                Layout.fillWidth:   true
                font.pointSize:     ScreenTools.smallFontPointSize
                color:              qgcPal.colorOrange
                wrapMode:           Text.WordWrap
                text:               qsTr("云端正在操控，本机摇杆输入已锁定。")
            }

            QGCLabel {
                Layout.fillWidth:   true
                font.pointSize:     ScreenTools.smallFontPointSize
                color:              qgcPal.colorGrey
                text:               drc && drc.cloudStick && drc.cloudStick["has_data"]
                                        ? qsTr("云端杆量  x %1   y %2   h %3   w %4")
                                              .arg(Number(drc.cloudStick["x"]).toFixed(1))
                                              .arg(Number(drc.cloudStick["y"]).toFixed(1))
                                              .arg(Number(drc.cloudStick["h"]).toFixed(1))
                                              .arg(Number(drc.cloudStick["w"]).toFixed(1))
                                        : qsTr("尚未收到云端杆量")
            }
        }

        // ---------------- 上行统计 ----------------
        ColumnLayout {
            Layout.fillWidth: true
            visible:          root.expanded
            spacing:          root._margin / 2

            RowLayout {
                Layout.fillWidth: true

                QGCLabel {
                    Layout.fillWidth:   true
                    font.pointSize:     ScreenTools.smallFontPointSize
                    color:              qgcPal.text
                    text:               qsTr("上行 / 下行计数")
                }

                QGCButton {
                    _horizontalPadding: 0
                    text:               qsTr("清零")
                    onClicked:          drc.drcClearStats()
                }
            }

            GridLayout {
                Layout.fillWidth:   true
                columns:            2
                columnSpacing:      root._margin
                rowSpacing:         root._margin / 2

                QGCLabel { text: qsTr("drone_control 收"); color: qgcPal.colorGrey; font.pointSize: ScreenTools.smallFontPointSize }
                QGCLabel { text: root._stat("drone_control_in"); font.pointSize: ScreenTools.smallFontPointSize }

                QGCLabel { text: qsTr("重复丢弃");        color: qgcPal.colorGrey; font.pointSize: ScreenTools.smallFontPointSize }
                QGCLabel {
                    text:           root._stat("drone_control_dup")
                    font.pointSize: ScreenTools.smallFontPointSize
                    // 云端是 publishCount=5 的重复投递，这个数涨得快是正常的
                    color:          qgcPal.colorGrey
                }

                QGCLabel { text: qsTr("指令被拒");        color: qgcPal.colorGrey; font.pointSize: ScreenTools.smallFontPointSize }
                QGCLabel {
                    text:           root._stat("drone_control_rej")
                    font.pointSize: ScreenTools.smallFontPointSize
                    color:          root._stat("drone_control_rej") > 0 ? qgcPal.colorRed : qgcPal.text
                }

                QGCLabel { text: qsTr("心跳上行");        color: qgcPal.colorGrey; font.pointSize: ScreenTools.smallFontPointSize }
                QGCLabel { text: root._stat("heartbeat_up"); font.pointSize: ScreenTools.smallFontPointSize }

                QGCLabel { text: qsTr("osd 上行");        color: qgcPal.colorGrey; font.pointSize: ScreenTools.smallFontPointSize }
                QGCLabel { text: root._stat("osd_up"); font.pointSize: ScreenTools.smallFontPointSize }

                QGCLabel { text: qsTr("hsi / delay 上行"); color: qgcPal.colorGrey; font.pointSize: ScreenTools.smallFontPointSize }
                QGCLabel {
                    text:           root._stat("hsi_up") + " / " + root._stat("delay_up")
                    font.pointSize: ScreenTools.smallFontPointSize
                }
            }
        }

        // ---------------- 低延时 osd（顺带自检上行内容） ----------------
        ColumnLayout {
            Layout.fillWidth: true
            visible:          root.expanded && drc && drc.drcOsd && drc.drcOsd["latitude"] !== undefined
            spacing:          root._margin / 2

            QGCLabel {
                font.pointSize: ScreenTools.smallFontPointSize
                color:          qgcPal.text
                text:           qsTr("最近一条 osd_info_push")
            }

            QGCLabel {
                Layout.fillWidth:   true
                font.pointSize:     ScreenTools.smallFontPointSize
                color:              qgcPal.colorGrey
                wrapMode:           Text.WordWrap
                text:               drc ? qsTr("纬度 %1  经度 %2  高度 %3\n航向 %4  speed_n/e/d %5/%6/%7\n云台 p/r/y %8/%9/%10")
                                            .arg(Number(drc.drcOsd["latitude"]).toFixed(6))
                                            .arg(Number(drc.drcOsd["longitude"]).toFixed(6))
                                            .arg(Number(drc.drcOsd["altitude"]).toFixed(1))
                                            .arg(drc.drcOsd["attitude_head"])
                                            .arg(Number(drc.drcOsd["speed_x"]).toFixed(1))
                                            .arg(Number(drc.drcOsd["speed_y"]).toFixed(1))
                                            .arg(Number(drc.drcOsd["speed_z"]).toFixed(1))
                                            .arg(Number(drc.drcOsd["gimbal_pitch"]).toFixed(0))
                                            .arg(Number(drc.drcOsd["gimbal_roll"]).toFixed(0))
                                            .arg(Number(drc.drcOsd["gimbal_yaw"]).toFixed(0))
                                     : ""
            }
        }

        // ---------------- 最后一条错误 ----------------
        QGCLabel {
            Layout.fillWidth:   true
            visible:            root.expanded && drc && drc.lastErrorText.length > 0
            wrapMode:           Text.WordWrap
            font.pointSize:     ScreenTools.smallFontPointSize
            color:              qgcPal.colorRed
            text:               drc ? drc.lastErrorText : ""
        }

        // ---------------- 危险区：紧急停桨 ----------------
        ColumnLayout {
            Layout.fillWidth: true
            visible:          root.expanded
            spacing:          root._margin / 2

            QGCLabel {
                Layout.fillWidth:   true
                font.pointSize:     ScreenTools.smallFontPointSize
                color:              qgcPal.colorGrey
                wrapMode:           Text.WordWrap
                text:               qsTr("紧急停桨 = 飞行终止（空中停桨），不可逆。云端的同名请求默认被拒，要先在上云设置里打开开关。")
            }

            QGCButton {
                Layout.fillWidth:   true
                text:               root._stopArmed ? qsTr("再点一次确认停桨") : qsTr("紧急停桨")
                primary:            root._stopArmed
                onClicked: {
                    if (!root._stopArmed) {
                        root._stopArmed = true
                        disarmTimer.restart()
                        return
                    }
                    disarmTimer.stop()
                    root._stopArmed = false
                    drc.drcEmergencyStop()
                }
            }
        }

        // ---------------- 调试区：本地虚拟摇杆 ----------------
        // 台架上校方向用：直接喂给本地映射层，不经过 MQTT、不需要云端配合。
        // 三个轴的取反开关在「应用设置 → 上云 → DRC」里。
        ColumnLayout {
            Layout.fillWidth: true
            spacing:          root._margin / 2

            QGCButton {
                Layout.fillWidth:   true
                text:               root.showDebug ? qsTr("收起调试") : qsTr("调试：本地摇杆")
                onClicked:          root.showDebug = !root.showDebug
            }

            ColumnLayout {
                Layout.fillWidth: true
                visible:          root.showDebug
                spacing:          root._margin / 2

                QGCLabel {
                    Layout.fillWidth:   true
                    wrapMode:           Text.WordWrap
                    font.pointSize:     ScreenTools.smallFontPointSize
                    color:              qgcPal.colorOrange
                    text:               qsTr("直接对飞机发摇杆量，不经过云端。只在台架（无桨或已固定）上用来核方向。")
                }

                // 云端拿着飞行控制权时，这台本地摇杆整块锁死 —— 两边同时发杆量没有任何意义，
                // 而且谁把自己当"当前操作者"都说不清。要拿回本地控制，先点上面的「收回」。
                QGCLabel {
                    Layout.fillWidth:   true
                    visible:            root._localLocked
                    wrapMode:           Text.WordWrap
                    font.pointSize:     ScreenTools.smallFontPointSize
                    color:              qgcPal.colorRed
                    text:               qsTr("云端正在操控，本地摇杆与「发送」「归零」已锁定。")
                }

                Repeater {
                    id:       debugSliders
                    enabled:  !root._localLocked
                    model: [
                        { key: "x", label: qsTr("x 左右（正=左）"),     min: -17, max: 17 },
                        { key: "y", label: qsTr("y 前后（正=前）"),     min: -17, max: 17 },
                        { key: "h", label: qsTr("h 升降（正=上升）"),   min: -4,  max: 5  },
                        { key: "w", label: qsTr("w 偏航（正=顺时针）"), min: -90, max: 90 }
                    ]

                    delegate: ColumnLayout {
                        required property var modelData

                        /// 量程与正方向都照协议（见 DjiDrcControlMapper.cc 顶部的常量），
                        /// 所以这里的滑杆值和云端下发的原始值是同一个刻度。
                        readonly property real axisValue: axisSlider.value
                        /// 清零要写回 Slider，alias 出来比在 Repeater 里翻 delegate 干净
                        property alias axisSliderRef: axisSlider

                        Layout.fillWidth: true
                        spacing:          0

                        QGCLabel {
                            font.pointSize: ScreenTools.smallFontPointSize
                            text:           modelData.label + "  " + Number(axisSlider.value).toFixed(1)
                        }

                        Slider {
                            id:                 axisSlider
                            Layout.fillWidth:   true
                            from:               modelData.min
                            to:                 modelData.max
                            value:              0
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing:          root._margin

                    QGCButton {
                        Layout.fillWidth:   true
                        enabled:            !root._localLocked
                        text:               qsTr("发送")
                        onClicked: {
                            var values = _axisValues()
                            drc.drcDebugStick(values[0], values[1], values[2], values[3])
                        }
                    }

                    QGCButton {
                        Layout.fillWidth:   true
                        enabled:            !root._localLocked
                        text:               qsTr("归零")
                        onClicked: {
                            _zeroSliders()
                            drc.drcDebugZeroSticks()
                        }
                    }
                }
            }
        }

        // ---------------- 退出 DRC ----------------
        QGCButton {
            Layout.fillWidth:   true
            visible:            root.expanded
            text:               qsTr("退出指令飞行")
            enabled:            drc && drc.drcState !== 0
            onClicked:          drc.drcRequestExit()
        }
    }

    /// 按 Repeater 的顺序读出四根轴的值（顺序同 model：x / y / h / w）。
    /// 读的是 delegate 上的 axisValue（= 对应 Slider 的 value），不是 delegate 本身。
    function _axisValues() {
        var values = [0, 0, 0, 0]
        for (var i = 0; i < debugSliders.count; i++) {
            var item = debugSliders.itemAt(i)
            if (item) {
                values[i] = item.axisValue
            }
        }
        return values
    }

    function _zeroSliders() {
        for (var i = 0; i < debugSliders.count; i++) {
            var item = debugSliders.itemAt(i)
            if (item) {
                item.axisSliderRef.value = 0
            }
        }
    }

    // 云端的接管 / 停桨弹窗**不在这里** —— 见 DrcRequestDialogs.qml。
    // 挂在这个面板上会随面板一起出现多份（FlyView / PlanView 各一份），一次 emit 弹两个窗。
}
