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
import QGroundControl.Controls
import QGroundControl.ScreenTools
import QGroundControl.Palette

/// 指令飞行（DRC / 云端控制）状态指示器。
///
/// 放在云服务图标左侧，**始终可见** —— 这是操作员唯一能一眼看出"云端在不在控"的地方。
/// 点开的面板是本地侧的两个断开口：收回控制权 / 退出指令飞行。
///
/// 抽屉右边缘对齐本图标（showIndicatorDrawer 的第三个参数），不是居中对齐：
/// 它离窗口右边缘只有几十像素，居中对齐会被 clamp 推到窗口内侧、跟图标差开一大截。
///
/// "云端遥控中 · 呼号"+"摇杆被锁"那两行原先挂在地图底部（DrcVirtualJoystick），
/// 现在并进本抽屉的第一屏 —— 地图上少一块常驻浮层，状态也只剩一个出处。
///
/// 颜色六态（优先级从高到低，越靠前越该被看见）：
///   待确认   红 + 闪烁   有人在请求接管/停桨，等操作员拍板
///   模式拒收 红          云端持权了，但飞机不在吃手动输入的模式里 —— 指令会被全部拒掉
///   云端     橙          云端正在控
///   本机     绿          DRC 已连接、控制权在本机
///   连接中   黄
///   未连接   灰
///
/// 与 DrcControlPanel 的分工：面板是**诊断台**（上行频率、计数、调试摇杆），
/// 这个抽屉是**动作台**（两个断开口）。状态显示两边都有，是有意的 ——
/// 面板在飞行视图里，抽屉在任何视图里都能点开。
Item {
    id:             control
    anchors.top:    parent.top
    anchors.bottom: parent.bottom

    readonly property var drc: djiBridgeServer.djiDrc

    /// 图标高度按工具栏高度反推，**不要写死** —— toolbarHeightMultiplier 是插件可配的。
    /// 比云图标（CloudIndicator 里的 1.5 倍字高）小一圈：两个图标并排时人形那个天然
    /// 显得更满，同高看着就是"指令飞行比云服务大一号"，那不是想表达的意思。
    readonly property real _iconSize:     Math.min(height * 0.42, ScreenTools.defaultFontPixelHeight * 1.1)
    /// 抽屉宽度。按**字高**算而不是字宽 —— 这样字号与框宽同比例缩放，改字体不会挤爆布局。
    /// StackLayout 的 implicitWidth 取所有子页的最大值，抽屉宽度必须显式给定。
    readonly property real _drawerWidth:  Math.min(ScreenTools.defaultFontPixelHeight * 13, mainWindow.contentItem.width * 0.45)
    /// 抽屉里的字号。原来是 smallFontPointSize（7.5pt），框比字大太多、读起来费劲。
    readonly property real _fontSize:     ScreenTools.defaultFontPointSize

    readonly property bool  _pending:      !!drc && (drc.authPending || drc.emergencyStopPending)
    readonly property bool  _cloudFlight:  !!drc && drc.cloudFlightAuthority
    /// 云端拿着飞行控制权，却落在不吃手动输入的模式里 —— 这正是那 1726 条静默拒绝的形态。
    /// 排在前两态之后：待确认和"云端在日常模式"都比它更常见。
    readonly property bool  _modeBlocked:  _cloudFlight && !drc.manualControlReady
    readonly property int   _drcState:     drc ? drc.drcState : 0

    readonly property color _color: _pending     ? qgcPal.colorRed
                                  : _modeBlocked ? qgcPal.colorRed
                                  : _cloudFlight ? qgcPal.colorOrange
                                  : _drcState === 2 ? qgcPal.colorGreen
                                  : _drcState === 1 ? qgcPal.colorYellow
                                  : qgcPal.colorGrey

    readonly property string _text: _pending      ? qsTr("待确认")
                                  : _modeBlocked  ? qsTr("模式拒收")
                                  : _cloudFlight  ? qsTr("云端")
                                  : _drcState === 2 ? qsTr("本机")
                                  : _drcState === 1 ? qsTr("连接中")
                                  : qsTr("未连接")

    readonly property string _stateText: _drcState === 2 ? qsTr("已连接")
                                       : _drcState === 1 ? qsTr("连接中…")
                                       : qsTr("未连接")

    /// 当前飞机的模式名。没有飞机时给空串 —— 别让抽屉里出现 "null"
    readonly property string _vehicleMode: {
        var vehicle = QGroundControl.multiVehicleManager.activeVehicle
        return vehicle ? vehicle.flightMode : ""
    }

    width: Math.max(_iconSize, labelText.implicitWidth)

    QGCPalette { id: qgcPal }

    Column {
        id:                     labelColumn
        anchors.centerIn:       parent
        spacing:                0

        QGCColoredImage {
            id:                     drcIcon
            width:                  control._iconSize
            height:                 control._iconSize
            anchors.horizontalCenter: parent.horizontalCenter
            // 人形、无字。带 "REMOTE ID" 字样的那版是 RidIconMan.svg，
            // 在这个高度上只剩一团墨。
            source:                 "/qmlimages/RidIconManNoID.svg"
            fillMode:               Image.PreserveAspectFit
            sourceSize.height:      height
            color:                  control._color
        }

        QGCLabel {
            id:                     labelText
            anchors.horizontalCenter: parent.horizontalCenter
            text:                   control._text
            color:                  control._color
            font.pointSize:         ScreenTools.smallFontPointSize * 1.1
            font.bold:              true
        }
    }

    // 待确认时闪一下，别让操作员漏掉弹窗。
    // 用 target 写法而不是 `on opacity`：动画停下时要把不透明度显式复位，
    // 否则会停在闪烁的中间值上，看上去像颗半透明的禁用图标。
    SequentialAnimation {
        id:      blinkAnim
        running: control._pending
        loops:   Animation.Infinite

        onRunningChanged: {
            if (!running) {
                control.opacity = 1.0
            }
        }

        NumberAnimation { target: control; property: "opacity"; to: 0.45; duration: 500 }
        NumberAnimation { target: control; property: "opacity"; to: 1.0;  duration: 500 }
    }

    MouseArea {
        anchors.fill:   parent
        // 右边缘对齐本图标：拖手是工具栏右端那颗，居中对齐会把抽屉推到内侧、差开一大截
        onClicked:      mainWindow.showIndicatorDrawer(drcDrawerComponent, control, true)
    }

    /// 「退出指令飞行」的第一下只是上膛，再点一下才真发
    property bool _exitArmed: false

    Timer {
        id:          exitDisarmTimer
        interval:    4000
        onTriggered: control._exitArmed = false
    }

    Component {
        id: drcDrawerComponent

        // 抽屉根必须是 ToolIndicatorPage：MainRootWindow 会往它上面绑定 expanded / 读取 showExpand。
        // showExpand 保持默认 false —— 没有 expandedComponent，展开箭头会是个点不动的死按钮。
        ToolIndicatorPage {
            contentComponent: Component {
                ColumnLayout {
                    spacing:                ScreenTools.defaultFontPixelHeight / 2
                    Layout.preferredWidth:  control._drawerWidth

                    // ---------------- 状态 ----------------
                    QGCLabel {
                        Layout.fillWidth:   true
                        color:              control._color
                        font.bold:          true
                        font.pointSize:     control._fontSize
                        text:               qsTr("指令飞行") + " · " + control._stateText
                    }

                    // ---------------- 云端正在控 ----------------
                    // 这两行原先贴在地图底部居中（DrcVirtualJoystick 那条浮层）。
                    // 并到这里之后地图上少一块常驻浮层，状态也只剩一个出处。
                    RowLayout {
                        Layout.fillWidth:   true
                        visible:            control._cloudFlight
                        spacing:            control._fontSize / 2

                        Rectangle {
                            Layout.alignment:   Qt.AlignVCenter
                            width:              control._fontSize * 0.55
                            height:             width
                            radius:             width / 2
                            color:              qgcPal.colorOrange
                        }

                        QGCLabel {
                            Layout.fillWidth:   true
                            color:              qgcPal.colorOrange
                            font.bold:          true
                            font.pointSize:     control._fontSize
                            text:               qsTr("云端遥控中") + " · "
                                                    + (control.drc && control.drc.authUserCallsign.length > 0
                                                           ? control.drc.authUserCallsign : qsTr("未知操作员"))
                        }
                    }

                    QGCLabel {
                        Layout.fillWidth:   true
                        // **必须夹住**：带 wrapMode 的 Text 报出来的 implicitWidth 仍是"不换行那一整行"
                        // 的宽度（中文没有空格，整句就是一个词），于是它会把整个 Column 撑开，
                        // 抽屉宽度反被最长的提示句决定 —— 夹住上限才会真的折行。
                        Layout.maximumWidth: control._drawerWidth
                        visible:            control._cloudFlight
                        wrapMode:           Text.WordWrap
                        color:              qgcPal.colorOrange
                        font.pointSize:     control._fontSize
                        text:               qsTr("摇杆由云端驱动，本机输入已锁定（拖不动是正常的）。")
                    }

                    GridLayout {
                        Layout.fillWidth:   true
                        columns:            2
                        columnSpacing:      ScreenTools.defaultFontPixelWidth
                        rowSpacing:         ScreenTools.defaultFontPixelHeight / 4

                        QGCLabel { text: qsTr("Broker"); color: qgcPal.colorGrey; font.pointSize: control._fontSize }
                        QGCLabel {
                            Layout.fillWidth:   true
                            text:               (control.drc && control.drc.brokerAddress.length > 0) ? control.drc.brokerAddress : "—"
                            elide:              Text.ElideMiddle
                            font.pointSize:     control._fontSize
                        }

                        QGCLabel { text: qsTr("下行静默"); color: qgcPal.colorGrey; font.pointSize: control._fontSize }
                        QGCLabel {
                            text:               control.drc ? (control.drc.downSilenceMs + " ms") : "—"
                            font.pointSize:     control._fontSize
                            // 超过 1 秒就变色：这条数字就是断链的第一眼判据
                            color:              (control.drc && control.drc.downSilenceMs > 1000) ? qgcPal.colorOrange : qgcPal.text
                        }

                        QGCLabel { text: qsTr("飞行控制权"); color: qgcPal.colorGrey; font.pointSize: control._fontSize }
                        QGCLabel {
                            text:               control._cloudFlight ? qsTr("云端") : qsTr("本机")
                            font.pointSize:     control._fontSize
                            color:              control._cloudFlight ? qgcPal.colorOrange : qgcPal.text
                        }

                        QGCLabel { text: qsTr("负载控制权"); color: qgcPal.colorGrey; font.pointSize: control._fontSize }
                        QGCLabel {
                            text:               (control.drc && control.drc.cloudPayloadAuthority) ? qsTr("云端") : qsTr("本机")
                            font.pointSize:     control._fontSize
                            color:              (control.drc && control.drc.cloudPayloadAuthority) ? qgcPal.colorOrange : qgcPal.text
                        }

                        QGCLabel { text: qsTr("飞行模式"); color: qgcPal.colorGrey; font.pointSize: control._fontSize }
                        QGCLabel {
                            Layout.fillWidth:   true
                            text:               control._vehicleMode.length > 0
                                                    ? control._vehicleMode + " · " + (control.drc && control.drc.manualControlReady ? qsTr("接受手动") : qsTr("拒绝手动"))
                                                    : "—"
                            font.pointSize:     control._fontSize
                            color:              (control.drc && control.drc.manualControlReady) ? qgcPal.text : qgcPal.colorRed
                        }
                    }

                    // 这一行就是"云端在控但控不动"的唯一解释，必须显眼
                    QGCLabel {
                        Layout.fillWidth:   true
                        Layout.maximumWidth: control._drawerWidth
                        visible:            control._modeBlocked
                        wrapMode:           Text.WordWrap
                        font.pointSize:     control._fontSize
                        color:              qgcPal.colorRed
                        text:               qsTr("云端已持权，但当前飞行模式不接受手动输入，云端的杆量指令会被全部拒绝。请先切到 Position / Loiter。")
                    }

                    QGCLabel {
                        Layout.fillWidth:   true
                        Layout.maximumWidth: control._drawerWidth
                        visible:            control.drc && control.drc.lastErrorText.length > 0
                        wrapMode:           Text.WordWrap
                        font.pointSize:     control._fontSize
                        color:              qgcPal.colorRed
                        text:               control.drc ? control.drc.lastErrorText : ""
                    }

                    // Separator
                    Rectangle {
                        Layout.fillWidth:       true
                        Layout.preferredHeight: 1
                        color:                  qgcPal.windowShade
                    }

                    // ---------------- 断开：收回控制权 ----------------
                    // 方向是安全的（把控制权拿回本地），所以不二次确认；
                    // **不关抽屉** —— 让操作员当场看到"飞行控制权：云端 → 本机"翻过来，
                    // 那就是这个按钮唯一的验收反馈。
                    QGCButton {
                        Layout.fillWidth:   true
                        text:               qsTr("收回控制权")
                        enabled:            !!control.drc && (control.drc.cloudFlightAuthority || control.drc.cloudPayloadAuthority)
                        onClicked: {
                            control.drc.drcGrabFlightAuthority(false)
                            control.drc.drcGrabPayloadAuthority(false)
                        }
                    }

                    // ---------------- 断开：退出指令飞行 ----------------
                    // 这个是会话级的、不可逆的：teardownDrc 会停掉所有定时器、断开 DRC 那条 MQTT、
                    // 发 drc_status_notify(0)，而且会停掉 25 Hz 的保活流（飞控可能因此触发失效保护）。
                    // 所以两次点击确认。
                    QGCButton {
                        Layout.fillWidth:   true
                        text:               control._exitArmed ? qsTr("再点一次确认退出") : qsTr("退出指令飞行")
                        primary:            control._exitArmed
                        enabled:            !!control.drc && control.drc.drcState !== 0
                        onClicked: {
                            if (!control._exitArmed) {
                                control._exitArmed = true
                                exitDisarmTimer.restart()
                                return
                            }
                            exitDisarmTimer.stop()
                            control._exitArmed = false
                            control.drc.drcRequestExit()
                            mainWindow.closeIndicatorDrawer()
                        }
                    }

                    QGCLabel {
                        Layout.fillWidth:   true
                        Layout.maximumWidth: control._drawerWidth
                        wrapMode:           Text.WordWrap
                        font.pointSize:     ScreenTools.smallFontPointSize * 1.15
                        color:              qgcPal.colorGrey
                        text:               qsTr("收回控制权 = 把控制权拿回本机（云端还能继续连）。退出指令飞行 = 结束整个 DRC 会话。")
                    }

                    Item { Layout.fillHeight: true }
                }
            }
        }
    }
}
