/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/


import QtQuick

import QGroundControl
import QGroundControl.ScreenTools
import QGroundControl.Controls
import QGroundControl.Palette
import QGroundControl.Vehicle

Item {
    // The following properties must be passed in from the Loader
    // property bool autoCenterThrottle - true: throttle will snap back to center when released
    // property bool leftHandedMode - true: virtual joystick layout will be reversed

    id: virtualJoysticks

    property var   _activeVehicle:            QGroundControl.multiVehicleManager.activeVehicle
    property bool  _initialConnectComplete:   _activeVehicle ? _activeVehicle.initialConnectComplete : false
    property real  leftYAxisValue:            autoCenterThrottle ? height / 2 : height
    property var   calibration:               false
    property var   uiTotalWidth
    property var   uiRealX

    /// DRC 客户端。djiBridgeServer 是 root context property，全应用可见。
    property var   _djiDrc:                   djiBridgeServer.djiDrc
    /// 云端是否持有飞行控制权。持权期间这两块摇杆**变成云端的显示器**：
    /// 不再发送、不再接受触摸，把手随后台下发的杆量走。
    property bool  _cloudDriving:             _djiDrc ? _djiDrc.cloudFlightAuthority : false

    on_CloudDrivingChanged: {
        // externalControl 不在这里赋值 —— 它在下面两个 pad 上是**绑定**到 _cloudDriving 的。
        // 赋值会静默拆掉绑定，锁就变成"依赖这个处理器被调用过"，万一哪天属性在组件
        // 加载时就已是 true（change 信号不补发），触摸锁会漏掉而没人看得出来。
        // 这里只负责回中位，两个边沿都要。

        // 两个方向都要回中位。**失权那一次是安全相关的**：本地那条 25Hz 流读的正是
        // pad 的 xAxis/yAxis，而它们刚被云端写过 —— 不回中的话，本地恢复的第一帧
        // 会把云端最后一杆当成操作员的输入发出去。
        // 注意左杆中位是 0.5（推力零点）而不是 0，见 JoystickThumbPad.driveNeutral。
        leftStick.driveNeutral()
        rightStick.driveNeutral()
    }

    /// 云端杆量变化 → 推动两块摇杆的把手。用的是 cloudStick 里**映射后**的轴值
    /// （roll/pitch/yaw/thrust），不是原始值 —— 映射里有取反和 drcInvertX/Y/W，
    /// 用原始值画会出现"把手指一边、飞机飞另一边"。
    Connections {
        target: virtualJoysticks._djiDrc

        function onCloudStickChanged() {
            if (!virtualJoysticks._cloudDriving) {
                return
            }
            var s = virtualJoysticks._djiDrc.cloudStick
            if (!s || !s.has_data) {
                return
            }
            // 左右手的分配与下面 Timer 里发杆量时保持一致：
            // virtualTabletJoystickValue(roll, pitch, yaw, thrust)
            //
            // 这里必须用**裸 id / 裸属性名**，不能加 virtualJoysticks. 前缀。
            // QML 的 id 不是对象的属性，父 id.子 id 求值是 undefined（可复现：
            // `Item { id: child }` 里 `root.child` 是 undefined，`child` 才是对象）；
            // leftHandedMode 也不是本对象的属性，它是 Loader 注入到本组件作用域的。
            // 写成限定名的话 rollStick 是 undefined，第一句 driveExternal 直接抛
            // TypeError，后面的 thrustStick 那一句永远不执行 —— 两块摇杆都不动，
            // 而工具栏那颗云端控制图标照常显示"云端"，看着像在正常工作。
            var rollStick   = leftHandedMode ? leftStick : rightStick
            var thrustStick = leftHandedMode ? rightStick : leftStick
            rollStick.driveExternal(s.roll, s.pitch)
            thrustStick.driveExternal(s.yaw, s.thrust)
        }
    }

    Timer {
        interval:   40  // 25Hz, same as real joystick rate
        running:    QGroundControl.settingsManager.appSettings.virtualJoystick.value
        repeat:     true
        onTriggered: {
            // 云端持权时跳过发送。Vehicle::virtualTabletJoystickValue 里也有一道
            // _cloudStickLock，这里再挡一次是为了不白跑一趟跨层调用、也让意图在本地可读。
            if (!virtualJoysticks._cloudDriving && _activeVehicle && _initialConnectComplete) {
                leftHandedMode ? _activeVehicle.virtualTabletJoystickValue(leftStick.xAxis, leftStick.yAxis, rightStick.xAxis, rightStick.yAxis) : _activeVehicle.virtualTabletJoystickValue(rightStick.xAxis, rightStick.yAxis, leftStick.xAxis, leftStick.yAxis)
            }
            leftYAxisValue = leftStick.yAxis // We keep Y axis value from the throttle stick for using it while there is a resize
        }
    }

    onHeightChanged:        { keepYAxisWhileChanged() }
    onWidthChanged:         { keepXAxisWhileChanged() }
    onCalibrationChanged:   { calibration ? calibrateJoysticks() : undefined }

    function calibrateJoysticks() {
        if( virtualJoysticks.visible ) {
        keepXAxisWhileChanged()
        leftYAxisValue = leftStick.yAxisReCentered() // Keep track of the correct leftYAxisValue while the width is adjusted at first start up
        }
    }

    function keepYAxisWhileChanged () {
        if( virtualJoysticks.visible ) {
            leftStick.resize( leftYAxisValue )
            rightStick.reCenter()
        }
    }

    function keepXAxisWhileChanged () {
        if( virtualJoysticks.visible ) {
            leftStick.reCenter()
            rightStick.reCenter()
        }
    }

    JoystickThumbPad {
        id:                     leftStick
        anchors.leftMargin:     xPositionDelta
        anchors.bottomMargin:   -yPositionDelta
        anchors.left:           parent.left
        anchors.bottom:         parent.bottom
        width:                  parent.height
        height:                 parent.height
        yAxisPositiveRangeOnly: _activeVehicle && !_activeVehicle.rover && !leftHandedMode
        yAxisReCenter:          autoCenterThrottle
        // 绑定而不是赋值：云端持权期间本控件**不接受触摸**，这是操作员一侧
        // "看得到但摸不动"的保证。见 JoystickThumbPad.MultiPointTouchArea.enabled。
        externalControl:        virtualJoysticks._cloudDriving
    }

    JoystickThumbPad {
        id:                     rightStick
        anchors.rightMargin:    -xPositionDelta
        anchors.bottomMargin:   -yPositionDelta
        anchors.right:          parent.right
        anchors.bottom:         parent.bottom
        width:                  parent.height
        height:                 parent.height
        yAxisPositiveRangeOnly: _activeVehicle && !_activeVehicle.rover && leftHandedMode
        yAxisReCenter:          true
        externalControl:        virtualJoysticks._cloudDriving
    }
}
