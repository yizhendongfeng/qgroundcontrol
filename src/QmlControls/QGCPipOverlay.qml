/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

import QtQuick                      2.12
import QtQuick.Window               2.12

import QGroundControl               1.0
import QGroundControl.ScreenTools   1.0
import QGroundControl.Controls      1.0
import QGroundControl.Palette       1.0
import QGroundControl.FlightDisplay 1.0
import QGroundControl.FlightMap     1.0
import QGroundControl.OsdOverlay    1.0

Rectangle {
    id:         _root
    width:      _pipSize < 200 ? 200 : _pipSize
    height:     _pipSize * (9/16)
    z:          pipZOrder + 1
    visible:    true //item2 && item2.pipState !== item2.pipState.window && show
    color:      Qt.rgba(0,0,0,0.2)

    property var    item1:                  null    // Required
    property var    item2:                  null    // Optional, may come and go
    property string item1IsFullSettingsKey          // Settings key to save whether item1 was saved in full mode
    property real   fullZOrder:             0       // zOrder for items in full mode
    property real   pipZOrder:              1       // zOrder for items in pip mode
    property bool   show:                   true
    property Component componentFlyVideo
    property alias   _currentItem:           gridViewVideo.currentItem
    readonly property string _pipExpandedSettingsKey: "IsPIPVisible"

    property var    _fullItem
    property var    _pipOrWindowItem
    property alias  _windowContentItem: window.contentItem
    property bool   _isExpanded:        true
    property real   _pipSize:           parent.width * 0.2
    property real   _maxSize:           0.75                // Percentage of parent control size
    property real   _minSize:           0.10
    property bool   _componentComplete: false

    Component.onCompleted: {
        _initForItems()
        _componentComplete = true
    }

    onShowChanged: {
        if (_pipOrWindowItem && _pipOrWindowItem.pipState.state !== _pipOrWindowItem.pipState.windowState) {
            _pipOrWindowItem.visible = show
        }
    }

    onItem2Changed: _initForItems()

    function showWindow() {
        window.width = _root.width
        window.height = _root.height
        window.show()
    }

    function _initForItems() {
        var item1IsFull = QGroundControl.loadBoolGlobalSetting(item1IsFullSettingsKey, true)
        if (item1 && item2) {
            item1.pipState.state = item1IsFull ? item1.pipState.fullState : item1.pipState.pipState
            item2.pipState.state = item1IsFull ? item2.pipState.pipState : item2.pipState.fullState
            _fullItem = item1IsFull ? item1 : item2
            _pipOrWindowItem = item1IsFull ? item2 : item1
        } else {
            item1.pipState.state = item1.pipState.fullState
            _fullItem = item1
            _pipOrWindowItem = null
        }
        _setPipIsExpanded(QGroundControl.loadBoolGlobalSetting(_pipExpandedSettingsKey, true))
        if (item2)
            console.log("_initForItems", item2.pipState.state, item2.z)
    }

    function _swapPip() {
        var item1IsFull = false
        if (item1.pipState.state === item1.pipState.fullState) {
            item1.pipState.state = item1.pipState.pipState
            item2.pipState.state = item2.pipState.fullState
            _fullItem = item2
            _pipOrWindowItem = item1
            item1IsFull = false
        } else {
            item1.pipState.state = item1.pipState.fullState
            item2.pipState.state = item2.pipState.pipState
            _fullItem = item1
            _pipOrWindowItem = item2
            item1IsFull = true
        }
        QGroundControl.saveBoolGlobalSetting(item1IsFullSettingsKey, item1IsFull)
//        console.log("_swapPip", item2.pipState.state, item2.z, buttonTest.z)
    }

    function _setPipIsExpanded(isExpanded) {
        QGroundControl.saveBoolGlobalSetting(_pipExpandedSettingsKey, isExpanded)
        _isExpanded = isExpanded
        if (_pipOrWindowItem) {
            _pipOrWindowItem.visible = isExpanded
        }
    }

    Window {
        id:         window
        visible:    false
        onClosing: {
            var item = contentItem.children[0]
            item.pipState.windowAboutToClose()
            item.pipState.state = item.pipState.windowClosingState
            item.pipState.state = item.pipState.pipState
            item.visible = _root.show
        }
    }

    MouseArea {
        id:             pipMouseArea
        anchors.fill:   parent
        enabled:        _isExpanded
        hoverEnabled:   true
//        onDoubleClicked:      {_swapPip();console.log("pipMouseArea z:", z)}
        propagateComposedEvents: true
        onWheel: wheel.accepted = true
    }

    // MouseArea to drag in order to resize the PiP area
    MouseArea {
        id:             pipResize
        anchors.top:    parent.top
        anchors.right:  parent.right
        height:         ScreenTools.minTouchPixels
        width:          height

        property real initialX:     0
        property real initialWidth: 0

        // When we push the mouse button down, we un-anchor the mouse area to prevent a resizing loop
        onPressed: {
            pipResize.anchors.top = undefined // Top doesn't seem to 'detach'
            pipResize.anchors.right = undefined // This one works right, which is what we really need
            pipResize.initialX = mouse.x
            pipResize.initialWidth = _root.width
        }

        // When we let go of the mouse button, we re-anchor the mouse area in the correct position
        onReleased: {
            pipResize.anchors.top = _root.top
            pipResize.anchors.right = _root.right
        }

        // Drag
        onPositionChanged: {
            console.log("onPositionChanged, height:", height, width)
            if (pipResize.pressed) {
                var parentWidth = _root.parent.width
                var newWidth = pipResize.initialWidth + mouse.x - pipResize.initialX
                if (newWidth < parentWidth * _maxSize && newWidth > parentWidth * _minSize) {
                    _pipSize = newWidth
                }
            }
        }
    }

    // Resize icon
    Image {
        source:         "/qmlimages/pipResize.svg"
        fillMode:       Image.PreserveAspectFit
        mipmap: true
        anchors.right:  parent.right
        anchors.top:    parent.top
        visible:        _isExpanded && (ScreenTools.isMobile || pipMouseArea.containsMouse)
        height:         ScreenTools.defaultFontPixelHeight * 2.5
        width:          ScreenTools.defaultFontPixelHeight * 2.5
        sourceSize.height:  height
        z:              _root.z + 3
    }

    // Check min/max constraints on pip size when when parent is resized
    Connections {
        target: _root.parent

        function onWidthChanged() {
            if (!_componentComplete) {
                // Wait until first time setup is done
                return
            }
            var parentWidth = _root.parent.width
            if (_root.width > parentWidth * _maxSize) {
                _pipSize = parentWidth * _maxSize
            } else if (_root.width < parentWidth * _minSize) {
                _pipSize = parentWidth * _minSize
            }
        }
    }

    // Pip to Window
    Image {
        id:             popupPIP
        source:         "/qmlimages/PiP.svg"
        mipmap:         true
        fillMode:       Image.PreserveAspectFit
        anchors.left:   parent.left
        anchors.top:    parent.top
        visible:        _isExpanded && !ScreenTools.isMobile && pipMouseArea.containsMouse
        height:         ScreenTools.defaultFontPixelHeight * 2.5
        width:          ScreenTools.defaultFontPixelHeight * 2.5
        sourceSize.height:  height
        z:              pipZOrder + 3
        MouseArea {
            anchors.fill:   parent
            onClicked:      _pipOrWindowItem.pipState.state = _pipOrWindowItem.pipState.windowState
        }
    }

    Image {
        id:             hidePIP
        source:         "/qmlimages/pipHide.svg"
        mipmap:         true
        fillMode:       Image.PreserveAspectFit
        anchors.left:   parent.left
        anchors.bottom: parent.bottom
        visible:        _isExpanded && (ScreenTools.isMobile || pipMouseArea.containsMouse)
        height:         ScreenTools.defaultFontPixelHeight * 2.5
        width:          ScreenTools.defaultFontPixelHeight * 2.5
        sourceSize.height:  height
        z:              pipZOrder + 3
        MouseArea {
            anchors.fill:   parent
            onClicked:      _root._setPipIsExpanded(false)
        }
    }

    Rectangle {
        id:                     showPip
        anchors.left :          parent.left
        anchors.bottom:         parent.bottom
        height:                 ScreenTools.defaultFontPixelHeight * 2
        width:                  ScreenTools.defaultFontPixelHeight * 2
        radius:                 ScreenTools.defaultFontPixelHeight / 3
        visible:                !_isExpanded
        color:                  _fullItem.pipState.isDark ? Qt.rgba(0,0,0,0.75) : Qt.rgba(0,0,0,0.5)
        Image {
            width:              parent.width  * 0.75
            height:             parent.height * 0.75
            sourceSize.height:  height
            source:             "/res/buttonRight.svg"
            mipmap:             true
            fillMode:           Image.PreserveAspectFit
            anchors.verticalCenter:     parent.verticalCenter
            anchors.horizontalCenter:   parent.horizontalCenter
        }
        MouseArea {
            anchors.fill:   parent
            onClicked:      _root._setPipIsExpanded(true)
        }
    }

    GridView {
        id:                 gridViewVideo
        flow:               Flow.TopToBottom
        anchors.fill:       parent
        anchors.margins:    5
        model:              1
        delegate:           componentFlyViewVideo
        highlight:          delegateHighlight
        focus:              true
        highlightFollowsCurrentItem: true
//        interactive:        false
        cellWidth:          parent.width
        cellHeight:         cellWidth * 0.7
//        cacheBuffer:    800
        clip:               false
        Component.onCompleted: console.log("cacheBuffer:",cacheBuffer, Column, "gridViewVideo.x:", gridViewVideo.x, "_root.x:", _root.x)
        Component {
            id: delegateHighlight
            Rectangle {
                z: 2
                color: "transparent"
                border.color: "yellow"
                border.width: 2
            }
        }

        Component {
            id: componentFlyViewVideo
            Item {
                id: delegateItem
                width: gridViewVideo.width
                height: width * 0.7
                property Item itemMap: mapControl

                FlyViewVideo {
                    id: videoControl1
                    visible: true
                    anchors.fill: parent
                    OsdOverlay {
                        id: osdOverlay
                        activeVehicle: globals.activeVehicle
                        anchors.fill: parent
                    }

                    FlyViewToolStrip {
                        id:                     toolStrip1
                        anchors.top:            parent.top
                        anchors.right:           parent.right
                        anchors.topMargin:      _toolsMargin * 2    //_toolsMargin + parentToolInsets.topEdgeLeftInset
                        anchors.rightMargin:    _toolsMargin * 2    //_toolsMargin + parentToolInsets.leftEdgeCenterInset
                        //            z:                      videoControl.z + 1
                        maxHeight:              parent.height / 2   //parent.height - y - parentToolInsets.bottomEdgeLeftInset - _toolsMargin
                        //        maxWidth:               parent.width - x - instrumentPanel.width - _toolsMargin//parentToolInsets.bottomEdgeLeftInset - _toolsMargin
                        visible:                !QGroundControl.videoManager.fullScreen
                        radius:                 ScreenTools.defaultFontPixelWidth / 2
                        showActionText:         false
                        onDisplayPreFlightChecklist: mainWindow.showPopupDialogFromComponent(preFlightChecklistPopup)
                        property real leftInset: x + width

                    }

                    PhotoVideoControl {
                        id:                     photoVideoControl1
                        anchors.margins:        _toolsMargin * 2
                        anchors.right:          parent.right
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Component.onCompleted: console.log("videoControl.visible:", videoControl1.visible)
                }
                MouseArea {
                    anchors.fill: parent
                    onDoubleClicked: {
                        console.log("double clicked  111")
                        gridViewVideo.currentIndex = index
                        console.log("double clicked  222")
                        if (videoControl1.pipState.state === videoControl1.pipState.pipState) {
                            videoControl1.pipState.state = videoControl1.pipState.fullState
                            item1.pipState.state = mapControl.pipState.pipState
                            toolStrip1.showActionText = true
                            videoControl1.parent = _root.parent
                            item1.parent = delegateItem
                        } else {//if (videoControl1.state === videoControl1.pipState.fullState) {
                            videoControl1.pipState.state = videoControl1.pipState.pipState
                            item1.pipState.state = mapControl.pipState.fullState
                            toolStrip1.showActionText = false
                            videoControl1.parent = delegateItem
                            item1.parent = _root.parent
                        }

                        console.log("double clicked componentFlyViewVideo index:", index, "videoControl1.state:", videoControl1.pipState.state, gridViewVideo.currentIndex, "toolStrip1.showActionText:", toolStrip1.showActionText)
                    }
                }
                onFocusChanged: {
                    videoControl1.pipState.state = mapControl.pipState.pipState
                    videoControl1.parent = delegateItem
                    console.log("onFocusChanged", videoControl1.pipState.state)
                }
            }
        }
        onCurrentItemChanged: {

            console.log("onCurrentItemChanged,", gridViewVideo.currentIndex)
        }
    }
}
