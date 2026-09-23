import QtQuick
import QtQuick.Controls

import QGroundControl
import QGroundControl.Palette
import QGroundControl.ScreenTools

Item {
    id:             _joyRoot

    property alias  lightColors:            mapPal.lightColors  ///< true: use light colors from QGCMapPalette for drawing
    property real   xAxis:                  0                   ///< Value range [-1,1], negative values left stick, positive values right stick
    property real   yAxis:                  0                   ///< Value range [-1,1], negative values down stick, positive values up stick
    property bool   yAxisPositiveRangeOnly: false               ///< true: value range [0,1], false: value range [-1,1]
    property bool   yAxisReCenter:          false               ///< true: snaps back to center on release, false: stays at current position on release
    property real   xPositionDelta:         0                   ///< Amount to move the control on x axis
    property real   yPositionDelta:         0                   ///< Amount to move the control on y axis

    /// true 时本控件不响应触摸，改由外部（云端 DRC）通过 driveExternal() 驱动。
    /// 触摸被禁掉是必须的：否则操作员的拖拽会和云端的写入逐帧对打。
    property bool   externalControl:        false

    property real   _centerXY:              width / 2
    property bool   _processTouchPoints:    false
    property color  _fgColor:               QGroundControl.globalPalette.text
    property color  _bgColor:               QGroundControl.globalPalette.window
    property real   _hatWidth:              ScreenTools.defaultFontPixelHeight
    property real   _hatWidthHalf:          _hatWidth / 2
    property bool   calculateYAxisMutex:    true
    property real   stickPositionX:         _centerXY
    property real   stickPositionY:         !yAxisReCenter ? height : height / 2
    property bool   alredyCreated:          false
    
    QGCMapPalette { id: mapPal }

    // 外部驱动期间不反算：driveExternal() 先把 xAxis/yAxis 写定，再算把手位置，
    // 让 stickPosition* 反过来覆盖 xAxis 就是同一次写入的两个方向打架。
    // （calculateYAxis 还带一个 calculateYAxisMutex，在 resize 之后会吞掉一次更新。）
    onStickPositionXChanged:            if (!externalControl) calculateXAxis()
    onStickPositionYChanged:            if (!externalControl) calculateYAxis()
    onYAxisPositiveRangeOnlyChanged:    if (!externalControl) calculateYAxis()
    onYAxisReCenterChanged:             yAxisReCentered()

    /// 由外部驱动本控件（云端 DRC 的杆量显示）。
    ///
    /// **直接写把手位置，不走 stickPosition* → calculateXAxis/YAxis 那条反算路径**：
    /// 那条路上的 calculateXAxis/calculateYAxis 在 !visible 时提前 return，而 QML 只在
    /// 值真正变化时才发 changed 信号 —— 全屏视频之类的场景下外部写入会全部落空，
    /// 并且**永久陈旧**（后续同样的值不再触发信号）。同时 reCenter() 每次触摸释放都会
    /// 覆盖它，calculateYAxisMutex 也会吞掉 resize 后的第一次更新。
    function driveExternal(xValue, yValue) {
        xAxis = Math.max(-1, Math.min(1, xValue))
        yAxis = Math.max(-1, Math.min(1, yValue))

        // 与 calculateXAxis/calculateYAxis 严格互为逆运算：
        //   xAxis = stickPositionX / width * 2 - 1
        //   yAxis = (1 - stickPositionY / height) * (yAxisPositiveRangeOnly ? 1 : 2)
        //          - (yAxisPositiveRangeOnly ? 0 : 1)
        stickPositionX = (xAxis + 1) * width / 2
        stickPositionY = (1 - yAxis) * height / (yAxisPositiveRangeOnly ? 1 : 2)
    }

    /// 回中位。注意 yAxisPositiveRangeOnly 的左杆中位是 **0.5**（推力零点），不是 0。
    function driveNeutral() {
        driveExternal(0, yAxisPositiveRangeOnly ? 0.5 : 0)
    }
    
    function yAxisReCentered() {
        if( yAxisReCenter ) {
            yAxis = yAxisPositiveRangeOnly ? 0.5 : 0
            stickPositionY = _joyRoot.height / 2
        }
        if( !alredyCreated && !yAxisReCenter ) {
            yAxis = yAxisPositiveRangeOnly ? 0 : -1
            stickPositionY = _joyRoot.height            
        }
        if ( alredyCreated && !yAxisReCenter ){
            yAxis = yAxisPositiveRangeOnly ? 0.5 : 0
            stickPositionY = _joyRoot.height / 2
        }
        alredyCreated = true
        return yAxis
    }

    //We prevent Joystick to move while the screen is resizing 
    function resize( yPositionAfterResize ) {
        if(_joyRoot.height <= 0) {
            return;
        }
        calculateYAxisMutex = false
        stickPositionY = ( 1 - ( ( yPositionAfterResize + ( !yAxisPositiveRangeOnly ? 1 : 0) ) /  ( yAxisPositiveRangeOnly ? 1 : 2 ) )) * _joyRoot.height // Reverse the CalculateYAxis Procedure
        stickPositionX = _joyRoot.width / 2 // Manual recenter
        calculateYAxisMutex = true
    }

    function calculateXAxis() {
        if(!_joyRoot.visible) {
            return;
        }
        var xAxisTemp = stickPositionX / width
        xAxisTemp *= 2.0
        xAxisTemp -= 1.0
        xAxis = xAxisTemp
    }

    function calculateYAxis() {
        if(!_joyRoot.visible) {
            return;
        }
        if(!calculateYAxisMutex) {
            return;
        }
        var fullRange = yAxisPositiveRangeOnly ? 1 : 2
        var pctUp = 1.0 - (stickPositionY / height)
        var rangeUp = pctUp * fullRange
        if (!yAxisPositiveRangeOnly) {
            rangeUp -= 1
        }
        yAxis = rangeUp
    }

    function reCenter() {
        _processTouchPoints = false
        _centerXY = _joyRoot.width / 2 // Reload before using it to make sure of using the right value
        // Move control back to original position
        xPositionDelta = 0
        yPositionDelta = 0

        // Re-Center sticks as needed
        stickPositionX = _centerXY
        if (yAxisReCenter) {
            stickPositionY = _centerXY
        }
    }

    function thumbDown(touchPoints) {
        // Position the control around the initial thumb position
        _centerXY = _joyRoot.width / 2  // make sure to know the correct center of the item

        // uiRealX / uiTotalWidth 是 FlyViewWidgetLayer 在 Loader 就绪**且可见**时注入的。
        // 「且可见」这个附加条件是个坑：就绪那一刻若恰好不可见（全屏视频、或此时
        // usingHighLatencyLink 为真），onLoaded 只走 else 分支，这两个值就**永久**是
        // undefined；另外两个补写的处理器只在值**变化**时动手，窗口尺寸不再变就永远补不上。
        //
        // 而 undefined 参与下面任何一个比较都得到 false，于是全部落到最后的 else 提前
        // return —— _processTouchPoints 永远不置真，**两块摇杆都拖不动**。把手不动，
        // 但杆量仍在按默认中位照发，所以从飞机那一侧完全看不出异常。
        //
        // 拿不到这两个值时就不做那套"防误触"限位，直接接受这次触摸。**只是省掉限位，
        // 平移量仍按下面各分支的算法来**（`touch - center` 把摇杆本体挪到手指底下）——
        // 若图省事把 xPositionDelta 写成 0，本体不动、把手会直接跳到手指位置，
        // 一次偏心的按下就等于瞬间打杆。
        // 检出用的是 `>=` 而不是 `!== undefined`，顺带把 null / NaN 一并挡住。
        if (!(uiRealX >= 0) || !(uiTotalWidth > 0)) {
            xPositionDelta = touchPoints[0].x - _centerXY
            yPositionDelta = yAxisPositiveRangeOnly ? touchPoints[0].y - stickPositionY
                                                    : touchPoints[0].y - _centerXY
            _processTouchPoints = true
            return
        }

        var limitOffset = uiRealX >= _joyRoot.width / 2 ? true : false // as the joystick become small the UI too so we limit the maxOffset for reCentering joystick to prevent misclicks
        var maxDelta = _joyRoot.x > uiTotalWidth / 2  ? uiTotalWidth - uiRealX - _joyRoot.x - _centerXY : uiRealX
        var isRightJoystick = _joyRoot.x > uiTotalWidth / 2 ? true : false

        // Check if new xDelta will make joystick to be beyond screen boundaries or can cause a misclick
        if (!limitOffset && isRightJoystick && touchPoints[0].x  <= maxDelta || !limitOffset && !isRightJoystick && touchPoints[0].x >= maxDelta) {
            xPositionDelta = touchPoints[0].x - _centerXY
        } else if (limitOffset && !isRightJoystick && touchPoints[0].x >= _centerXY * 0.25 && touchPoints[0].x <= _centerXY * 2) { // more offset at the side near to the center
            xPositionDelta = touchPoints[0].x - _centerXY
        } else if (limitOffset && isRightJoystick && touchPoints[0].x >= 0 && touchPoints[0].x <= _centerXY * 1.75) {
            xPositionDelta = touchPoints[0].x - _centerXY
        } else {
            return;
        }

        if (yAxisPositiveRangeOnly) {
            yPositionDelta = touchPoints[0].y - stickPositionY
        } else {
            yPositionDelta = touchPoints[0].y - _centerXY
        }
        // We need to wait until we move the control to the right position before we process touch points
        _processTouchPoints = true
    }

    /*
    // Keep in for debugging
    Column {
        QGCLabel { text: xAxis }
        QGCLabel { text: yAxis }
    }
    */

    Image {
        anchors.fill:       parent
        source:             "/res/JoystickBezelLight.png"
        mipmap:             true
        smooth:             true
    }

    Rectangle {
        anchors.fill:       parent
        radius:             width / 2
        color:              _bgColor
        opacity:            0.5

        Rectangle {
            anchors.margins:    parent.width / 4
            anchors.fill:       parent
            radius:             width / 2
            border.color:       _fgColor
            border.width:       2
            color:              "transparent"
        }

        Rectangle {
            anchors.fill:       parent
            radius:             width / 2
            border.color:       _fgColor
            border.width:       2
            color:              "transparent"
        }
    }

    QGCColoredImage {
        color:                      _fgColor
        visible:                    yAxisPositiveRangeOnly
        height:                     ScreenTools.defaultFontPixelHeight
        width:                      height
        sourceSize.height:          height
        mipmap:                     true
        fillMode:                   Image.PreserveAspectFit
        source:                     "/res/clockwise-arrow.svg"
        anchors.right:              parent.right
        anchors.rightMargin:        ScreenTools.defaultFontPixelWidth
        anchors.verticalCenter:     parent.verticalCenter
    }

    QGCColoredImage {
        color:                      _fgColor
        visible:                    yAxisPositiveRangeOnly
        height:                     ScreenTools.defaultFontPixelHeight
        width:                      height
        sourceSize.height:          height
        mipmap:                     true
        fillMode:                   Image.PreserveAspectFit
        source:                     "/res/counter-clockwise-arrow.svg"
        anchors.left:               parent.left
        anchors.leftMargin:         ScreenTools.defaultFontPixelWidth
        anchors.verticalCenter:     parent.verticalCenter
    }

    QGCColoredImage {
        color:                      _fgColor
        visible:                    yAxisPositiveRangeOnly
        height:                     ScreenTools.defaultFontPixelHeight
        width:                      height
        sourceSize.height:          height
        mipmap:                     true
        fillMode:                   Image.PreserveAspectFit
        source:                     "/res/chevron-up.svg"
        anchors.top:                parent.top
        anchors.topMargin:          ScreenTools.defaultFontPixelWidth
        anchors.horizontalCenter:   parent.horizontalCenter
    }

    QGCColoredImage {
        color:                      _fgColor
        visible:                    yAxisPositiveRangeOnly
        height:                     ScreenTools.defaultFontPixelHeight
        width:                      height
        sourceSize.height:          height
        mipmap:                     true
        fillMode:                   Image.PreserveAspectFit
        source:                     "/res/chevron-down.svg"
        anchors.bottom:             parent.bottom
        anchors.bottomMargin:       ScreenTools.defaultFontPixelWidth
        anchors.horizontalCenter:   parent.horizontalCenter
    }

    Rectangle {
        width:          _hatWidth
        height:         _hatWidth
        radius:         _hatWidthHalf
        border.color:   _fgColor
        border.width:   1
        color:          Qt.rgba(_fgColor.r, _fgColor.g, _fgColor.b, 0.5)
        x:              stickPositionX - _hatWidthHalf
        y:              stickPositionY - _hatWidthHalf
    }

    Connections {
        target: touchPoint

        onXChanged: {
            if (_processTouchPoints) {
                _joyRoot.stickPositionX = Math.max(Math.min(touchPoint.x, _joyRoot.width), 0)
            }
        }
        onYChanged: {
            if (_processTouchPoints) {
                _joyRoot.stickPositionY = Math.max(Math.min(touchPoint.y, _joyRoot.height), 0)
            }
        }
    }

    MultiPointTouchArea {
        anchors.fill:           parent
        anchors.bottomMargin:   yAxisReCenter ? 0 : -_hatWidthHalf
        minimumTouchPoints:     1
        maximumTouchPoints:     1
        // 云端持权期间不接受触摸：既防止拖拽与云端写入打架，也让"把手在动但拖不动"
        // 成为操作员看得见的反馈。
        enabled:                !_joyRoot.externalControl
        touchPoints:            [ TouchPoint { id: touchPoint } ]
        onPressed:              touchPoints => _joyRoot.thumbDown(touchPoints)
        onReleased:             _joyRoot.reCenter()
    }
}
