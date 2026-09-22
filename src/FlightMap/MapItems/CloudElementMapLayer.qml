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
import QtLocation
import QtPositioning

import QGroundControl
import QGroundControl.Controls
import QGroundControl.FlightMap

/// 云平台地图元素图层（Pilot 地图标注）
///
/// 把 DjiCloudMapClient 里的端点/线/面画到地图上，飞行视图与规划视图共用一份。
/// 显示与编辑合一：编辑中的元素（djiBridgeServer.cloudMapEditing 指向的那个）打开 interactive，
/// QGCMap*Visuals 自带顶点拖拽 / 拆段 / 右键菜单；元素自己的 traceMode 由编辑器面板切换，
/// 所以这里 showEditToolbar: false —— Basic/Circular/Trace/Load KML 那排按钮不用出（编辑入口在面板里）。
///
/// 端点（菱形）是唯一由本文件自己接鼠标的：线和面的标绘由 QGCMap*Visuals 负责，
/// 端点只有一个坐标、没有几何对象可标，所以"点地图放点"在这里的 MouseArea 里做。
Item {
    id: _root
    z: QGroundControl.zOrderMapItems

    property var map             ///< Map control to show items on

    // 图层本身跟着地图一样大：端点放置要接全图的左键点击，而 MouseArea 只能锚到
    // 父项或兄弟项 —— 本图层的父项才是地图，锚 map 等于锚"爷爷"，QML 是不认的。
    // 图层没有 MouseArea 时不接收鼠标事件，撑满也不会挡住地图的拖动缩放。
    width:  map ? map.width  : 0
    height: map ? map.height : 0

    // 颜色来自平台（#RRGGBB）。delegate 里先落成 color 属性再算填充色的透明度，
    // 直接对 QString 取 .r 是不行的。
    readonly property real _areaFillOpacity: 0.25

    /// 当前编辑中的元素（null = 不在编辑态）
    readonly property var _editing: djiBridgeServer.cloudMapEditing
    /// 正在等"在地图上点一下放端点"：新建端点（beginCreate(0) 会打开 tracing），
    /// 或编辑一个已有端点时按了「在地图上重新定位」
    readonly property bool _placingPoint: _editing !== null && _editing.type === 0 && _editing.tracing

    /// 端点放置：点一下就在那儿放一个菱形。**连点就连着放** —— 本机草稿每放一个就
    /// 直接 POST 上云、并立刻准备下一个（见 DjiCloudMapClient::placePoint）。
    ///
    /// 只在放置会话里开着 —— 这个 MouseArea 铺满整张地图，常开就会把左键点击全吃掉。
    /// 线的标绘是 QGCMapPolylineVisuals 自己按需建的鼠标区，那是另一条路（两者不会同时开：
    /// 同一时刻只有一个元素在编辑）。
    MouseArea {
        anchors.fill:       parent
        enabled:            _root._placingPoint
        visible:            enabled
        preventStealing:    true

        onClicked: (mouse) => {
            if (mouse.button !== Qt.LeftButton) {
                return
            }
            const coordinate = _root.map.toCoordinate(Qt.point(mouse.x, mouse.y), false /* clipToViewPort */)
            if (coordinate && coordinate.isValid) {
                djiBridgeServer.cloudMapPlacePoint(coordinate.latitude, coordinate.longitude)
            }
        }
    }

    Instantiator {
        model: djiBridgeServer.cloudMapLines

        delegate: QGCMapPolylineVisuals {
            parent:             _root
            mapControl:         map
            mapPolyline:        object.geometry
            lineColor:          object.color
            lineWidth:          3
            interactive:        object.editing
            showEditToolbar:    false
        }
    }

    Instantiator {
        model: djiBridgeServer.cloudMapAreas

        delegate: QGCMapPolygonVisuals {
            parent:             _root
            mapControl:         map
            mapPolygon:         object.geometry
            borderColor:        object.color
            borderWidth:        2
            interiorColor:      Qt.rgba(borderColor.r, borderColor.g, borderColor.b, _root._areaFillOpacity)
            interactive:        object.editing
            showEditToolbar:    false
        }
    }

    /// 端点（Point）—— 官方控制台上的菱形图钉，见 CloudElementMarker
    Instantiator {
        model: djiBridgeServer.cloudMapPoints

        delegate: CloudElementMarker {
            parent:         _root
            mapControl:     map
            coordinate:     object.coordinate
            markerColor:    object.color
        }
    }

    /// 飞行区域（后端 flight-area）—— 任务区域与 GEO 区域，只读
    ///
    /// 模型里只放"当前该画"的那些：显示开关是在 C++ 侧筛完再塞进模型的，不是靠这里的
    /// visible —— visuals 的 MapPolygon 是在 mapControl 上命令式创建的，给 delegate 的
    /// Item 设 visible 藏不住它。开关本身由图标列那两个按钮切。
    /// 配色和填充规则对齐官方 Web 控制台：任务区域绿、GEO/禁飞区红、停用的灰，
    /// 且只有"启用中的 GEO 区域"填色，其余是纯描边。
    Instantiator {
        model: djiBridgeServer.cloudFlightAreas

        delegate: QGCMapPolygonVisuals {
            readonly property bool _filled: object.geoZone && object.enabled

            parent:             _root
            mapControl:         map
            mapPolygon:         object.geometry
            borderColor:        object.color
            borderWidth:        3
            interiorColor:      _filled
                                    ? Qt.rgba(borderColor.r, borderColor.g, borderColor.b, 0.3)
                                    : "transparent"
            interactive:        false
            showEditToolbar:    false
        }
    }
}
