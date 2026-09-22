/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

import QtQuick
import QtQuick.Shapes
import QtLocation
import QtPositioning

import QGroundControl
import QGroundControl.ScreenTools

/// 云元素里的端点（Point）—— 官方 Web 控制台画的是彩色**菱形**图钉
///
/// 大小与形状照抄官方图标 `assets/icons/pin-<color>.svg`：那是个 24×24 的图标，外框路径
/// `M6.56,0 L13.12,9.90 L6.56,19.80 L0,9.90 Z` —— 宽 13.12、高 19.80，也就是**瘦高**的
/// 菱形（宽:高 = 1 : 1.5085），不是正方形转 45° 出来的等边菱形；路径里还套了一条反向的
/// 内框线（nonzero 填充规则把它挖空），所以是空心菱形，描边宽度约 2.4 个单位。
/// 控制台用 AMap.Marker 按 svg 原尺寸显示（没给 size，就是图标的 24px），所以这里也按
/// 同样的像素比例画：宽 = 字高 × 0.73（默认字号下 13.1px，高 19.8px）。
///
/// MapQuickItem 不像 MapPolyline/MapPolygon 那样声明在哪个 Item 下面都能出来，
/// 它必须挂在 mapControl 上，所以写法照抄 QGCMapPolygonVisuals.qml 里的手柄：
/// Component.createObject(mapControl) → mapControl.addMapItem(...)，
/// 销毁只 destroy()（QGC 自己也是这么写的，不加 removeMapItem）。
Item {
    id: _root

    property var    mapControl                      ///< Map control to place item in
    property var    coordinate                      ///< QGeoCoordinate，无效就不画
    property color  markerColor:    "#2D8CF0"
    /// 菱形宽度（短对角线）；高度按官方 svg 的宽高比推出来
    property real   markerWidth:    ScreenTools.defaultFontPixelHeight * 0.73
    readonly property real _markerHeight: markerWidth * 1.5085

    property var    _marker

    function _destroyMarker() {
        if (_marker) {
            _marker.destroy()
            _marker = null
        }
    }

    function _syncMarker() {
        if (!mapControl || !coordinate || !coordinate.isValid) {
            _destroyMarker()
            return
        }
        if (!_marker) {
            _marker = markerComponent.createObject(mapControl)
            if (!_marker) {
                return
            }
            mapControl.addMapItem(_marker)
        }
        _marker.coordinate = coordinate
    }

    onMapControlChanged:    _syncMarker()
    onCoordinateChanged:    _syncMarker()

    Component.onCompleted:  _syncMarker()
    Component.onDestruction: _destroyMarker()

    Component {
        id: markerComponent

        MapQuickItem {
            anchorPoint.x:  sourceItem.width / 2
            anchorPoint.y:  sourceItem.height / 2
            z:              QGroundControl.zOrderMapItems

            sourceItem: Item {
                width:  _root.markerWidth
                height: _root._markerHeight

                // 四个顶点直接按 svg 的比例摆，不靠"画正方形再拉伸"——
                // 拉伸会把描边一起拉粗，画出来就不像官方的那个细框了
                Shape {
                    anchors.fill:   parent
                    antialiasing:   true

                    ShapePath {
                        strokeColor: _root.markerColor
                        strokeWidth: 2
                        fillColor:   "transparent"
                        startX:      _root.markerWidth / 2
                        startY:      0
                        PathLine { x: _root.markerWidth;     y: _root._markerHeight / 2 }
                        PathLine { x: _root.markerWidth / 2; y: _root._markerHeight }
                        PathLine { x: 0;                     y: _root._markerHeight / 2 }
                        PathLine { x: _root.markerWidth / 2; y: 0 }
                    }
                }
            }
        }
    }
}
