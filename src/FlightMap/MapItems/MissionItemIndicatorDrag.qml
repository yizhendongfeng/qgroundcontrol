/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

import QtQuick
import QtLocation

import QGroundControl
import QGroundControl.ScreenTools
import QGroundControl.Controls

/// Use to drag a MissionItemIndicator
Rectangle {
    id:             itemDragger
    x:              _itemIndicatorX - _touchMarginHorizontal
    y:              _itemIndicatorY - _touchMarginVertical
    width:          _itemIndicatorWidth + (_touchMarginHorizontal * 2)
    height:         _itemIndicatorHeight + (_touchMarginVertical * 2)
    color:          "transparent"
    z:              QGroundControl.zOrderMapItems + 1    // Above item icons

    // Properties which must be specific by consumer
    property real minTouchSize: 0       ///< 桌面端也想要「最小拖拽区」时填边长（像素）。0 = 原版桌面行为
    property var mapControl     ///< Map control which contains this item
    property var itemIndicator  ///< The mission item indicator to drag around
    property var itemCoordinate ///< Coordinate we are updating during drag

    signal clicked
    signal dragStart
    signal dragStop

    property bool   _preventCoordinateBindingLoop:  false

    property real _itemIndicatorX:          itemIndicator ? itemIndicator.x : 0
    property real _itemIndicatorY:          itemIndicator ? itemIndicator.y : 0
    property real _itemIndicatorWidth:      itemIndicator ? itemIndicator.width : 0
    property real _itemIndicatorHeight:     itemIndicator ? itemIndicator.height : 0
    property bool _mobile:                  ScreenTools.isMobile
    // 桌面端原来把「触摸外扩」整段关掉了（_touchMargin* 直接为 0），于是拖拽区**正好等于
    // 标记本身的大小**。航点标记在没选中时是「小」样式（~15px），而它自己的点击区被
    // MissionItemIndexLabel 里的 mouseAreaFill 撑到了「正常」大小（~26px）—— 于是按在
    // 航点稍微偏一点的地方，命中的是标记自己的点击区：那一下只会**选中**，不会拖动，
    // 看上去就是「航点拖不动」。这里允许消费方指定一个桌面端的最小拖拽边长；
    // 取值 ≤ 标记尺寸时行为和原来完全一样（冒号两边的表达式都退化成 0）
    property real _touchWidth:              Math.max(_itemIndicatorWidth,  _mobile ? ScreenTools.minTouchPixels : minTouchSize)
    property real _touchHeight:             Math.max(_itemIndicatorHeight, _mobile ? ScreenTools.minTouchPixels : minTouchSize)
    property real _touchMarginHorizontal:   (_touchWidth  - _itemIndicatorWidth) / 2
    property real _touchMarginVertical:     (_touchHeight - _itemIndicatorHeight) / 2
    property bool _dragStartSignalled:      false

    onXChanged: liveDrag()
    onYChanged: liveDrag()

    function liveDrag() {
        if (!itemDragger._preventCoordinateBindingLoop && itemDrag.drag.active) {
            var point = Qt.point(itemDragger.x + _touchMarginHorizontal + itemIndicator.anchorPoint.x, itemDragger.y + _touchMarginVertical + itemIndicator.anchorPoint.y)
            var coordinate = mapControl.toCoordinate(point, false /* clipToViewPort */)
            itemDragger._preventCoordinateBindingLoop = true
            coordinate.altitude = itemCoordinate.altitude
            itemCoordinate = coordinate
            itemDragger._preventCoordinateBindingLoop = false
        }
    }

    Drag.active: itemDrag.drag.active

    QGCMouseArea {
        id:                 itemDrag
        anchors.fill:       parent
        drag.target:        parent
        drag.minimumX:      0
        drag.minimumY:      0
        drag.maximumX:      itemDragger.parent.width - parent.width
        drag.maximumY:      itemDragger.parent.height - parent.height
        preventStealing:    true
        enabled:            itemDragger.visible

        onClicked: {
            focus = true
            itemDragger.clicked()
        }

        property bool dragActive: drag.active
        onDragActiveChanged: {
            if (dragActive) {
                focus = true
                if (!_dragStartSignalled) {
                    _dragStartSignalled = true
                    dragStart()
                }
            } else {
                _dragStartSignalled = false
                dragStop()
            }
        }
    }
}
