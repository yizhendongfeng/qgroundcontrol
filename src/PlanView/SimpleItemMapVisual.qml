/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

import QtQuick
import QtQuick.Controls
import QtLocation
import QtPositioning

import QGroundControl
import QGroundControl.ScreenTools
import QGroundControl.Palette
import QGroundControl.Controls
import QGroundControl.FlightMap

/// Simple Mission Item visuals
Item {
    id: _root

    property var map        ///< Map control to place item in
    property var vehicle    ///< Vehicle associated with this item
    property bool interactive: true

    property var    _missionItem:       object
    property var    _itemVisual
    property var    _loiterVisual
    property var    _dragArea
    property bool   _itemVisualShowing: false
    property bool   _dragAreaShowing:   false

    signal clicked(int sequenceNumber)

    function hideItemVisuals() {
        if (_itemVisualShowing) {
            _itemVisual.destroy()
            _loiterVisual.destroy()
            _itemVisualShowing = false
        }
    }

    function showItemVisuals() {
        if (!_itemVisualShowing) {
            _itemVisual = indicatorComponent.createObject(map)
            map.addMapItem(_itemVisual)
            _loiterVisual = loiterComponent.createObject(map)
            map.addMapItem(_loiterVisual)
            _itemVisualShowing = true
        }
    }

    function hideDragArea() {
        if (_dragAreaShowing) {
            _dragArea.destroy()
            _dragAreaShowing = false
        }
    }

    function showDragArea() {
        if (!_dragAreaShowing) {
            _dragArea = dragAreaComponent.createObject(map)
            _dragAreaShowing = true
        }
    }

    /// 要不要给这个航点摆一个拖拽区。
    ///
    /// 原版只在**当前项**上摆一个 —— 得先点一下把它选中，再按下去拖，两步。用户按着
    /// 一个没选中的航点直接拖，屏幕上什么也不会发生（那个点上没有接鼠标的东西，地图
    /// 也不会跟着平移），看上去就是「航点拖不动」。这里改成每个带坐标的航点都摆一个，
    /// 按下去就能拖。
    ///
    /// 「点一下选中」这一步没有丢：拖拽区的 MouseArea 会把点击转回本组件（见下面
    /// dragAreaComponent 的 onClicked），选中/拖动是同一个点上的两种手势，不再要求
    /// 先选中才能拖
    function updateDragArea() {
        if (map.planView && _missionItem.specifiesCoordinate) {
            showDragArea()
        } else {
            hideDragArea()
        }
    }

    Component.onCompleted: {
        showItemVisuals()
        updateDragArea()
    }

    Component.onDestruction: {
        hideDragArea()
        hideItemVisuals()
    }


    Connections {
        target: _missionItem

        function onIsCurrentItemChanged() {         updateDragArea() }
        function onSpecifiesCoordinateChanged() {   updateDragArea() }
    }

    Connections {
        target: _missionItem.isSimpleItem ? _missionItem : null

        onLoiterRadiusChanged: {
            _loiterVisual.handleLoiterRadiusChange()
        }

        onCoordinateChanged: {
            _loiterVisual.handleCoordinateChange()
        }
    }

    // Control which is used to drag items
    Component {
        id: dragAreaComponent

        MissionItemIndicatorDrag {
            mapControl:              _root.map
            itemIndicator:           _itemVisual
            itemCoordinate:          _missionItem.coordinate
            visible:                 _root.interactive
            // 拖拽区至少要有「正常大小标记」那么大（~1.4 倍默认字号，就是选中时那个
            // 圆——见 MissionItemIndexLabel 的 mouseAreaFill 也是按这个尺寸外扩的）。
            // 不然未选中的航点拖拽区只有小标记那么大，按偏一点就落到标记的点击区上，
            // 只会选中、拖不动 —— 详见 MissionItemIndicatorDrag 里 minTouchSize 的注释
            minTouchSize:            ScreenTools.defaultFontPixelHeight * 1.4
            onItemCoordinateChanged: _missionItem.coordinate = itemCoordinate
            // 这个矩形盖在航点标记上面，点击先落在它这儿 —— 得自己转出去，
            // 否则航点就点不中、也选不上了（原版只给当前项摆拖拽区，当前项本来
            // 就是选中的，所以这条一直是空的）
            onClicked:               _root.clicked(_missionItem.sequenceNumber)
        }
    }

    Component {
        id: indicatorComponent

        MissionItemIndicator {
            coordinate:     _missionItem.coordinate
            visible:        _missionItem.specifiesCoordinate
            z:              QGroundControl.zOrderMapItems
            missionItem:    _missionItem
            sequenceNumber: _missionItem.sequenceNumber
            onClicked:      if(_root.interactive)  _root.clicked(_missionItem.sequenceNumber)
            opacity:        _root.opacity
        }
    }

    Component  {
        id: loiterComponent

        MapQuickItem {
            id:                               loiterMapQuickItem
            coordinate:                       _root._missionItem.coordinate
            visible:                          _root.interactive && _missionItem.isSimpleItem && _missionItem.showLoiterRadius

            property alias blockSignals:      loiterMapCircleVisuals.blockSignals
            property alias radius:            _mapCircle.radius
            property alias clockwiseRotation: _mapCircle.clockwiseRotation

            function handleLoiterRadiusChange() {
                blockSignals = true
                clockwiseRotation = _missionItem.loiterRadius>= 0
                blockSignals = false
                radius.rawValue = Math.abs(_missionItem.loiterRadius)
            }

            function handleCoordinateChange() {
                coordinate = _missionItem.coordinate
            }

            onCoordinateChanged:              _mapCircle.center = coordinate

            sourceItem: QGCMapCircleVisuals {
                id:                      loiterMapCircleVisuals
                mapControl:              _root.map
                mapCircle:               _mapCircle
                centerDragHandleVisible: false
                borderColor:             _missionItem.terrainCollision ? "red" : QGroundControl.globalPalette.mapMissionTrajectory

                property bool blockSignals: false

                function updateMissionItem() {
                    _missionItem.loiterRadius = _mapCircle.clockwiseRotation ? _mapCircle.radius.rawValue : -_mapCircle.radius.rawValue
                }

                QGCMapCircle {
                    id:                         _mapCircle
                    center:                     loiterMapQuickItem.coordinate
                    interactive:                _root.interactive && _missionItem.isCurrentItem && map.planView
                    showRotation:               true
                    onClockwiseRotationChanged: if(!blockSignals) loiterMapCircleVisuals.updateMissionItem()
                }

                Connections {
                    target:            _mapCircle.radius
                    function onRawValueChanged() {
                        if(!blockSignals) loiterMapCircleVisuals.updateMissionItem()
                    }
                }
            }

            Component.onCompleted: {
                handleLoiterRadiusChange()
                handleCoordinateChange()
            }
        }
    }
}
