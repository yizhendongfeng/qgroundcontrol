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

/// 云端请求类弹窗（接管请求 / 紧急停桨 / 控制断开提醒）的**唯一宿主**。
///
/// 为什么要单独拆一个文件：这两个请求是 DjiDrcClient 上的信号，而信号是**全局事件** ——
/// 谁连上，谁就弹一个弹窗。这些 Connections 原来挂在 DrcControlPanel 上，而
/// CloudElementToolBar 在 FlyView 和 PlanView 里各有一份（MainRootWindow 的 StackLayout
/// 不做懒实例化，两棵树启动时就全建好了）。两份面板 → 两个 handler → 一次 emit 弹两个模态窗。
///
/// **本文件必须并且只允许实例化一次。** 再加一个实例，"弹两次"就会原样回来。
/// 实例化位置：src/ui/MainRootWindow.qml，紧挨 MainWindowSavedState。
///
/// 拆开之后 DrcControlPanel 不再处理任何信号，可以任意丢弃 / 放进 active=false 的
/// Loader，不会再影响弹窗。
///
/// 根用 Item 而不是 QtObject：QtObject **没有 default property**，下面的
/// QGCPalette / Connections / 两个 Component 会全部挂不上去，报
/// "Cannot assign to non-existent default property"，整个组件 unavailable。
/// 代价是主窗口里多一个不可见的 Item —— 用 visible/尺寸把它压到零。
Item {
    id: root

    visible: false
    width:  0
    height: 0

    readonly property var drc: djiBridgeServer.djiDrc

    /// 同一时刻只留一个弹窗。createObject 出来的对象受 QGCPopupDialog::destroyOnClose
    /// 管，关闭时会被销毁，所以引用要在 closed 里清掉 —— 否则第二次请求会被
    /// 上面的 if 挡住，弹窗再也不出现。
    property var _authDialog:          null
    property var _emergencyStopDialog: null
    property var _lostDialog:          null
    property var _releasedDialog:      null

    QGCPalette { id: qgcPal }

    Connections {
        target: root.drc

        function onAuthRequested(userId, callsign) {
            if (root._authDialog) {
                // 已经在问了，别叠第二个 —— 叠上去操作员要点两次"是"才生效
                return
            }
            root._authDialog = authDialogComponent.createObject(mainWindow)
            root._authDialog.closed.connect(function() { root._authDialog = null })
            root._authDialog.open()
        }

        function onEmergencyStopRequested() {
            if (root._emergencyStopDialog) {
                return
            }
            root._emergencyStopDialog = emergencyStopDialogComponent.createObject(mainWindow)
            root._emergencyStopDialog.closed.connect(function() { root._emergencyStopDialog = null })
            root._emergencyStopDialog.open()
        }

        /// 云端那条控制链断了，而且不是操作员点的（云端要求退出 / 下行静默超时 / 心跳超时）。
        /// 这不是"请求"而是"通报"：只有一个「知道了」按钮，唯一的目的是让操作员
        /// 当场知道飞机失去控制源了 —— 工具栏那颗图标会变色，但人在看飞机的时候不会盯着它。
        function onRemoteControlLost(reason, heldFlightAuthority) {
            if (root._lostDialog) {
                // **更新，而不是丢弃。** 同一次断开可能由多条路径依次报上来
                // （下行静默 → 心跳超时），但那是同一件事、同一条原因链，
                // 文字几乎一样；真正要防的是下面这种：
                // 上一次断开的窗还开着（这个框没有自动关闭，必须点「Ok」），
                // 操作员起身去拿飞机的时候第二次断开发生了 —— 如果这里直接 return，
                // 屏幕上留着的就是上一条**过期的**原因，而"控制权还在云端"这种
                // 最要命的一条被静默吞掉。它恰恰是这个弹窗存在的唯一理由。
                // 所以：新的一条覆盖旧的一条，永远只留一个窗、且内容是最新的。
                // 不用操心这个窗会不会被别的模态窗压住：收尾时 cancelPendingAuth
                // 会发 authCancelled 把云端接管请求窗关掉（cloud_control_auth_notify
                // 那边的同步调用本来也不能一直挂着），所以断开这一刻屏幕上最多
                // 只剩这一个窗。QQuickPopup 也没有 raise()，别想着提层。
                root._lostDialog.reason              = reason
                root._lostDialog.heldFlightAuthority = heldFlightAuthority
                return
            }
            root._lostDialog = lostDialogComponent.createObject(mainWindow)
            // 只在 createObject 之后再赋值（而不是用第二个参数的对象字面量）：
            // 对象字面量里的键会被 qmllint 当成一次"未限定的标识符访问"报出来。
            root._lostDialog.reason              = reason
            root._lostDialog.heldFlightAuthority = heldFlightAuthority
            root._lostDialog.closed.connect(function() { root._lostDialog = null })
            root._lostDialog.open()
        }

        /// 云端主动交还控制权（cloud_control_release）。这不是"断开"：DRC 会话还在、
        /// 链路还在、云端随时能再请求接管 —— 所以不复用上面那个「控制已断开」窗，
        /// 那个标题会把操作员吓一跳。但要弹，因为交还的那一刻飞机可能正被云端驱动着。
        /// 只在云端确实拿过控制权时才发（见 DjiDrcClient::handleAuthRelease）。
        function onRemoteControlReleased(hadFlightAuthority, hadPayloadAuthority) {
            if (root._releasedDialog) {
                // 同 onRemoteControlLost：更新而不是丢弃，永远只留一个窗、内容最新
                root._releasedDialog.hadFlightAuthority   = hadFlightAuthority
                root._releasedDialog.hadPayloadAuthority  = hadPayloadAuthority
                return
            }
            root._releasedDialog = releasedDialogComponent.createObject(mainWindow)
            root._releasedDialog.hadFlightAuthority  = hadFlightAuthority
            root._releasedDialog.hadPayloadAuthority = hadPayloadAuthority
            root._releasedDialog.closed.connect(function() { root._releasedDialog = null })
            root._releasedDialog.open()
        }

        /// 请求在别处被作废了（本地「收回控制权」、授权释放、DRC 退出）。
        /// 用这个专用信号而不是 drcStatusChanged：后者每秒都在发，拿它当边沿会乱关窗。
        function onAuthCancelled() {
            if (root._authDialog) {
                // close() 会走 onClosed → drcRespondAuth(false)，而那边对
                // "已经没有待确认请求"的调用直接返回，不会覆盖任何东西
                root._authDialog.close()
            }
        }
    }

    // ---------------- 云端接管请求 ----------------
    Component {
        id: authDialogComponent

        QGCPopupDialog {
            title:      qsTr("云端请求控制飞机")
            buttons:    Dialog.Yes | Dialog.No

            ColumnLayout {
                spacing: ScreenTools.defaultFontPixelHeight / 2

                QGCLabel {
                    Layout.preferredWidth:  Math.max(mainWindow.width / 3, headerMinWidth)
                    wrapMode:               Text.WordWrap
                    text:                   qsTr("云端用户 %1%2 请求接管本机的飞行控制权。\n\n同意后，对方的虚拟摇杆指令会直接作用在这架飞机上。")
                                                .arg(root.drc ? root.drc.authUserCallsign : "")
                                                .arg(root.drc && root.drc.authUserId.length > 0 ? " (" + root.drc.authUserId + ")" : "")
                }

                QGCLabel {
                    Layout.fillWidth:       true
                    wrapMode:               Text.WordWrap
                    color:                  qgcPal.colorOrange
                    text:                   qsTr("不确定对方是谁就选「否」。")
                }
            }

            onAccepted: root.drc.drcRespondAuth(true)
            onRejected: root.drc.drcRespondAuth(false)
            // 点外面关掉 / 按 ESC = 没同意，也要把结果回给云端，否则它会一直等。
            // closed 在 accepted/rejected 之后才发，所以这里会再调一次 ——
            // DjiDrcClient::drcRespondAuth 对"已经没有待确认请求"的调用直接返回，
            // 不会把已经发出去的同意覆盖成拒绝。
            onClosed:   root.drc.drcRespondAuth(false)
        }
    }

    // ---------------- 云端紧急停桨请求 ----------------
    Component {
        id: emergencyStopDialogComponent

        QGCPopupDialog {
            title:      qsTr("云端请求紧急停桨")
            buttons:    Dialog.Yes | Dialog.No

            ColumnLayout {
                spacing: ScreenTools.defaultFontPixelHeight / 2

                QGCLabel {
                    Layout.preferredWidth:  Math.max(mainWindow.width / 3, headerMinWidth)
                    wrapMode:               Text.WordWrap
                    text:                   qsTr("云端下发了紧急停桨。\n\n这会让飞机立刻终止飞行（空中停桨），不可逆。")
                }

                QGCLabel {
                    Layout.fillWidth:       true
                    wrapMode:               Text.WordWrap
                    color:                  qgcPal.colorRed
                    text:                   qsTr("只有确认飞机确实需要立刻停桨时才选「是」。")
                }
            }

            onAccepted: root.drc.drcRespondEmergencyStop(true)
            onRejected: root.drc.drcRespondEmergencyStop(false)
            // 同上面的接管弹窗：关掉 = 拒绝，且重复调用是安全的（见 drcRespondEmergencyStop）
            onClosed:   root.drc.drcRespondEmergencyStop(false)
        }
    }

    // ---------------- 云端控制已断开（提醒接管） ----------------
    // 触发时机见 DjiDrcClient::teardownDrc 的 notifyOperator 参数：只覆盖
    // "不是操作员点的"那三条断开路径，且信号是在收尾**之后**发的 ——
    // 所以这里读到的 cloudFlightAuthority 已经是 false（控制权已回到本机），
    // "断开时在谁手上"只能靠信号带过来的 heldFlightAuthority。
    Component {
        id: lostDialogComponent

        QGCPopupDialog {
            id:      lostDialog
            title:   qsTr("云端控制已断开")
            buttons: Dialog.Ok

            /// 由 onRemoteControlLost 赋值（createObject 之后）
            property string reason:              ""
            property bool   heldFlightAuthority: false

            readonly property string _vehicleMode: {
                var vehicle = QGroundControl.multiVehicleManager.activeVehicle
                return vehicle ? vehicle.flightMode : ""
            }

            ColumnLayout {
                spacing: ScreenTools.defaultFontPixelHeight / 2

                QGCLabel {
                    Layout.preferredWidth:  Math.max(mainWindow.width / 3, headerMinWidth)
                    wrapMode:               Text.WordWrap
                    text:                   qsTr("原因：%1").arg(lostDialog.reason)
                }

                // 这一行是弹窗存在的全部理由：断开的那一刻飞机还在被云端驱动，
                // 也就是"控制源刚刚消失"。其余情形（云端只是结束了会话、没拿过控制权）
                // 不值得让人紧张，用下一行平铺直叙就够了。
                QGCLabel {
                    Layout.fillWidth:       true
                    Layout.maximumWidth:    Math.max(mainWindow.width / 3, headerMinWidth)
                    visible:                lostDialog.heldFlightAuthority
                    wrapMode:               Text.WordWrap
                    color:                  qgcPal.colorRed
                    font.bold:              true
                    text:                   qsTr("断开时飞行控制权还在云端 —— 这架飞机刚刚失去了控制源，请立即接管。")
                }

                QGCLabel {
                    Layout.fillWidth:       true
                    Layout.maximumWidth:    Math.max(mainWindow.width / 3, headerMinWidth)
                    wrapMode:               Text.WordWrap
                    text:                   qsTr("控制权已回到本机，本机摇杆已解锁，可以直接操作。")
                }

                // 控制权回来了 ≠ 杆量能被吃下去：模式不对的话本机杆量照样会被飞控拒掉
                // （见 DjiDrcClient::manualControlReady 与 DjiDrcControlMapper 的白名单）。
                // 不写这一句，操作员会以为是自己手感不对。
                QGCLabel {
                    Layout.fillWidth:       true
                    Layout.maximumWidth:    Math.max(mainWindow.width / 3, headerMinWidth)
                    visible:                !root.drc.manualControlReady
                    wrapMode:               Text.WordWrap
                    color:                  qgcPal.colorRed
                    text:                   qsTr("但当前飞行模式（%1）不接受手动输入，杆量指令会被飞控拒绝。请先切到 Position / Loiter，再操作。")
                                                .arg(lostDialog._vehicleMode.length > 0 ? lostDialog._vehicleMode : qsTr("未知"))
                }
            }

            // 只通报、不提问：唯一按钮就是「Ok」，点了就关（_accept 自己会 close）。
            // QGCPopupDialog 对 Dialog.Ok 设的是 NoAutoClose —— 点外面关不掉，
            // 这正是"提醒"该有的样子：操作员必须过一下这个窗。
        }
    }

    // ---------------- 云端已交还控制权 ----------------
    // 由 DjiDrcClient::handleAuthRelease 在**真的交还了东西**之后发（cloud_control_release），
    // 且只在那之前飞行/负载控制权至少有一个在云端手上时才发。
    // 与上面的「控制已断开」刻意分开：这里会话没断、链路没断，标题不能说"断开"。
    Component {
        id: releasedDialogComponent

        QGCPopupDialog {
            id:      releasedDialog
            title:   qsTr("云端已交还控制权")
            buttons: Dialog.Ok

            /// 由 onRemoteControlReleased 赋值（createObject 之后）
            property bool hadFlightAuthority:  false
            property bool hadPayloadAuthority: false

            readonly property string _vehicleMode: {
                var vehicle = QGroundControl.multiVehicleManager.activeVehicle
                return vehicle ? vehicle.flightMode : ""
            }

            ColumnLayout {
                spacing: ScreenTools.defaultFontPixelHeight / 2

                QGCLabel {
                    Layout.preferredWidth:  Math.max(mainWindow.width / 3, headerMinWidth)
                    wrapMode:               Text.WordWrap
                    text:                   qsTr("云端主动交还了控制权。")
                }

                // 飞行控制权在云端手上时交还，才是"飞机刚被云端驱动过"那一刻 ——
                // 这一条要显眼。只交还了负载（相机/云台）的话操作员对飞机的操作
                // 本来就没断过，用下面那句平铺直叙就够了。
                QGCLabel {
                    Layout.fillWidth:       true
                    Layout.maximumWidth:    Math.max(mainWindow.width / 3, headerMinWidth)
                    visible:                releasedDialog.hadFlightAuthority
                    wrapMode:               Text.WordWrap
                    color:                  qgcPal.colorOrange
                    font.bold:              true
                    text:                   qsTr("交还前飞行控制权在云端 —— 这架飞机刚才由云端驱动。现在它回到本机了。")
                }

                QGCLabel {
                    Layout.fillWidth:       true
                    Layout.maximumWidth:    Math.max(mainWindow.width / 3, headerMinWidth)
                    wrapMode:               Text.WordWrap
                    visible:                releasedDialog.hadFlightAuthority
                    text:                   qsTr("本机摇杆已解锁，可以直接操作。")
                }

                // 同「控制已断开」窗：控制权回来 ≠ 杆量吃得下去，模式不对照样被飞控拒掉。
                QGCLabel {
                    Layout.fillWidth:       true
                    Layout.maximumWidth:    Math.max(mainWindow.width / 3, headerMinWidth)
                    visible:                releasedDialog.hadFlightAuthority && !root.drc.manualControlReady
                    wrapMode:               Text.WordWrap
                    color:                  qgcPal.colorRed
                    text:                   qsTr("但当前飞行模式（%1）不接受手动输入，杆量指令会被飞控拒绝。请先切到 Position / Loiter，再操作。")
                                                .arg(releasedDialog._vehicleMode.length > 0 ? releasedDialog._vehicleMode : qsTr("未知"))
                }

                QGCLabel {
                    Layout.fillWidth:       true
                    Layout.maximumWidth:    Math.max(mainWindow.width / 3, headerMinWidth)
                    visible:                releasedDialog.hadPayloadAuthority
                    wrapMode:               Text.WordWrap
                    text:                   qsTr("相机 / 云台的负载控制权也已交还本机。")
                }

                // 交还 ≠ 散会：DRC 会话和链路都还开着。不写这句，操作员会以为
                // 云端已经下线了，然后对"它怎么又动了"毫无准备。
                QGCLabel {
                    Layout.fillWidth:       true
                    Layout.maximumWidth:    Math.max(mainWindow.width / 3, headerMinWidth)
                    wrapMode:               Text.WordWrap
                    color:                  qgcPal.colorGrey
                    text:                   qsTr("指令飞行会话与链路仍在，云端随时可以再次请求接管。")
                }
            }
        }
    }
}
