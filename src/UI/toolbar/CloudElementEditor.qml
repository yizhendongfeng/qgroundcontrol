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
import QtPositioning

import QGroundControl
import QGroundControl.ScreenTools
import QGroundControl.Controls
import QGroundControl.Palette

/// 云平台地图元素编辑器（列表 + 编辑表单）
///
/// 显示与绘制都交给地图上的 CloudElementMapLayer，这里只负责：
/// 选择元素、改名、选色、切标绘模式、保存、删除。飞行视图（从图标列左侧弹出）
/// 与规划视图的右侧面板共用这一份，区别只由 compactMode / 宽度决定。
/// 新建元素走 CloudElementToolBar 的图标列，不在这里。
///
/// 注意：标绘是"在地图上点"，所以本面板不能放在主窗口的 indicatorDrawer 里
/// ——那个 Popup 是 modal + CloseOnPressOutside，点一下地图自己就关了。
QGCFlickable {
    id:                 root
    contentHeight:      _contentColumn.height
    clip:               true
    boundsBehavior:     Flickable.StopAtBounds

    /// 用于「定位」按钮，可为空（为空时不显示定位按钮）
    property var    flightMap
    /// true: 飞行视图的小面板，压紧间距
    property bool   compactMode: false

    readonly property real _margin:     ScreenTools.defaultFontPixelWidth / 2
    readonly property real _swatchSize: ScreenTools.defaultFontPixelHeight * 1.2

    /// 与 Web 端控制台一致的元素配色（Cloud-API-Demo-Web/src/constants/map.ts）
    readonly property var _colorPalette: [ "#2D8CF0", "#19BE6B", "#FFBB00", "#E23C39", "#B620E0", "#212121" ]

    readonly property var _editing: djiBridgeServer.cloudMapEditing
    /// 编辑中的是端点：它没有顶点可"标"，文案和那个标绘按钮的含义都不一样
    readonly property bool _editingIsPoint: root._editing !== null && root._editing.type === 0

    QGCPalette { id: qgcPal }

    /// 编辑表单标题：0=端点 1=线段 2=区域
    function _editingTitle(element) {
        if (!element) {
            return ""
        }
        if (element.type === 2) {
            return qsTr("编辑区域")
        }
        if (element.type === 0) {
            return qsTr("编辑端点")
        }
        return qsTr("编辑线段")
    }

    /// 元素行的公共部分：色块 + 名称 + 顶点数 + 三个操作按钮
    Component {
        id: elementRowComponent

        Rectangle {
            id:                 rowRoot
            required property var object
            required property int index

            /// 端点（Point）没有 QGCMap* 几何对象，取坐标的方式和线/面不一样
            readonly property bool _isPoint: rowRoot.object.type === 0

            width:              _contentColumn.width
            implicitHeight:     rowLayout.implicitHeight + (root._margin * 2)
            color:              index % 2 ? qgcPal.windowShade : qgcPal.windowShadeLight
            radius:             ScreenTools.defaultFontPixelWidth / 4

            RowLayout {
                id:                     rowLayout
                anchors.left:           parent.left
                anchors.right:          parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.margins:        root._margin
                spacing:                root._margin

                Rectangle {
                    Layout.preferredWidth:  root._swatchSize
                    Layout.preferredHeight: root._swatchSize
                    radius:                 root._swatchSize / 4
                    color:                  rowRoot.object.color
                    border.width:           1
                    border.color:           qgcPal.text
                }

                QGCLabel {
                    Layout.fillWidth:   true
                    elide:              Text.ElideRight
                    color:              qgcPal.text
                    text:               rowRoot.object.name
                }

                QGCLabel {
                    font.pointSize:     ScreenTools.smallFontPointSize
                    color:              qgcPal.colorGrey
                    text:               rowRoot._isPoint ? qsTr("端点")
                                                         : rowRoot.object.vertexCount + qsTr("点")
                }

                QGCButton {
                    _horizontalPadding: 0
                    text:               qsTr("编辑")
                    onClicked:          djiBridgeServer.cloudMapBeginEdit(rowRoot.object.id)
                }

                QGCButton {
                    _horizontalPadding: 0
                    text:               qsTr("定位")
                    visible:            !!root.flightMap && rowRoot.object.vertexCount > 0
                    onClicked: {
                        // 端点的 geometry 是 null（它只有一个坐标），只有线与面能取顶点
                        var coordinate = rowRoot._isPoint
                                ? rowRoot.object.coordinate
                                : rowRoot.object.geometry.vertexCoordinate(0)
                        if (coordinate && coordinate.isValid) {
                            root.flightMap.center = coordinate
                        }
                    }
                }

                QGCButton {
                    _horizontalPadding: 0
                    text:               qsTr("删除")
                    onClicked:          djiBridgeServer.cloudMapRemoveElement(rowRoot.object.id)
                }
            }
        }
    }

    ColumnLayout {
        id:         _contentColumn
        width:      root.width
        spacing:    root._margin

        // ---------------- 状态 ----------------
        QGCLabel {
            Layout.fillWidth:   true
            wrapMode:           Text.WordWrap
            font.pointSize:     ScreenTools.smallFontPointSize
            color:              djiBridgeServer.cloudWsConnected ? qgcPal.text : qgcPal.colorOrange
            text:               djiBridgeServer.cloudMapStatus.length > 0 ?
                                    djiBridgeServer.cloudMapStatus :
                                    (djiBridgeServer.cloudWsConnected ? qsTr("已连接云平台") : qsTr("未连接云平台，地图元素不可用"))
        }

        // ---------------- 刷新 ----------------
        // 新建线段/新建区域 不在这里了：改由地图右侧的 CloudElementToolBar 图标列触发
        // （两张视图都有那一列），免得面板上出现两个入口、两种交互
        RowLayout {
            Layout.fillWidth:   true
            spacing:            root._margin

            QGCButton {
                Layout.fillWidth:   true
                text:               qsTr("刷新")
                onClicked:          djiBridgeServer.cloudMapRefresh()
            }
        }

        // ---------------- 编辑表单 ----------------
        Rectangle {
            Layout.fillWidth:       true
            implicitHeight:         _editColumn.implicitHeight + (root._margin * 2)
            visible:                root._editing !== null
            color:                  qgcPal.windowShadeDark
            radius:                 ScreenTools.defaultFontPixelWidth / 4

            ColumnLayout {
                id:             _editColumn
                anchors.left:   parent.left
                anchors.right:  parent.right
                anchors.top:    parent.top
                anchors.margins: root._margin
                spacing:        root._margin

                QGCLabel {
                    Layout.fillWidth:   true
                    elide:              Text.ElideRight
                    color:              qgcPal.text
                    text:               (root._editing ? root._editingTitle(root._editing) : "")
                                        + " · " + (root._editing ? root._editing.vertexCount : 0) + qsTr("点")
                }

                QGCTextField {
                    id:                 nameField
                    Layout.fillWidth:   true
                    placeholderText:    qsTr("名称")
                    text:               root._editing ? root._editing.name : ""
                }

                // 颜色：6 个 swatch，点了立刻反映到地图上
                RowLayout {
                    Layout.fillWidth:   true
                    spacing:            root._margin

                    Repeater {
                        model: root._colorPalette

                        Rectangle {
                            required property var modelData

                            Layout.preferredWidth:  root._swatchSize
                            Layout.preferredHeight: root._swatchSize
                            radius:                 root._swatchSize / 4
                            color:                  modelData
                            border.width:           (root._editing && root._editing.color === modelData) ? 3 : 1
                            border.color:           qgcPal.text

                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    if (root._editing) {
                                        root._editing.color = modelData
                                    }
                                }
                            }
                        }
                    }

                    Item { Layout.fillWidth: true }
                }

                QGCLabel {
                    Layout.fillWidth:   true
                    wrapMode:           Text.WordWrap
                    font.pointSize:     ScreenTools.smallFontPointSize
                    color:              qgcPal.colorGrey
                    visible:            root._editing && root._editing.tracing
                    text:               root._editingIsPoint
                                            ? qsTr("放置中：在地图上点一下就是一个端点，点一下放一个，直接同步到平台")
                                            : qsTr("标绘中：在地图上点击添加顶点，结束后点「结束标绘」")
                }

                RowLayout {
                    Layout.fillWidth:   true
                    spacing:            root._margin

                    QGCButton {
                        Layout.fillWidth:   true
                        // 端点没有"顶点"可标，这颗按钮对它就是"再在地图上点一下换个位置"：
                        // 编辑平台上的端点时也能用（改完走 PUT，取消会还原回原坐标）
                        text:               root._editingIsPoint
                                                ? ((root._editing && root._editing.tracing)
                                                       ? qsTr("结束放置") : qsTr("在地图上重新定位"))
                                                : ((root._editing && root._editing.tracing)
                                                       ? qsTr("结束标绘") : qsTr("开始标绘"))
                        onClicked: {
                            if (root._editing) {
                                root._editing.setTracing(!root._editing.tracing)
                            }
                        }
                    }

                    QGCButton {
                        Layout.fillWidth:   true
                        text:               qsTr("取消")
                        onClicked:          djiBridgeServer.cloudMapCancelEdit()
                    }

                    QGCButton {
                        Layout.fillWidth:   true
                        text:               qsTr("保存到平台")
                        // 线段最少 2 个顶点、区域最少 3 个、端点 1 个，和后端校验保持一致
                        enabled:            root._editing && root._editing.vertexCount >=
                                                (root._editing.type === 2 ? 3 : (root._editing.type === 0 ? 1 : 2))
                        onClicked:          djiBridgeServer.cloudMapSaveEdit(nameField.text, root._editing.color)
                    }
                }
            }
        }

        // ---------------- 端点 ----------------
        // 官方控制台上的菱形图钉（resource.type == 0）。新建走图标列那颗菱形按钮：
        // 选完在地图上点一下就是一个，**每点一次直接 POST 上云并准备下一个**（连续放置），
        // 所以列表会一个一个多出来，不用回面板按「保存」。
        // 这里负责改名、改色、删除；平台上的端点还能用上面那颗「在地图上重新定位」挪位置（走 PUT）。
        QGCLabel {
            Layout.fillWidth:   true
            color:              qgcPal.text
            font.pointSize:     ScreenTools.smallFontPointSize
            text:               qsTr("端点") + " (" + djiBridgeServer.cloudMapPoints.count + ")"
        }

        ListView {
            Layout.fillWidth:       true
            Layout.preferredHeight: contentHeight
            interactive:            false
            clip:                   true
            spacing:                1
            boundsBehavior:         Flickable.StopAtBounds
            model:                  djiBridgeServer.cloudMapPoints
            delegate:               elementRowComponent

            QGCLabel {
                anchors.horizontalCenter:   parent.horizontalCenter
                color:                      qgcPal.colorGrey
                font.pointSize:             ScreenTools.smallFontPointSize
                visible:                    parent.count === 0
                text:                       qsTr("暂无端点")
            }
        }

        // ---------------- 线段 ----------------
        QGCLabel {
            Layout.fillWidth:   true
            color:              qgcPal.text
            font.pointSize:     ScreenTools.smallFontPointSize
            text:               qsTr("线段") + " (" + djiBridgeServer.cloudMapLines.count + ")"
        }

        ListView {
            Layout.fillWidth:       true
            Layout.preferredHeight: contentHeight
            interactive:            false
            clip:                   true
            spacing:                1
            boundsBehavior:         Flickable.StopAtBounds
            model:                  djiBridgeServer.cloudMapLines
            delegate:               elementRowComponent

            QGCLabel {
                anchors.horizontalCenter:   parent.horizontalCenter
                color:                      qgcPal.colorGrey
                font.pointSize:             ScreenTools.smallFontPointSize
                visible:                    parent.count === 0
                text:                       qsTr("暂无线段")
            }
        }

        // ---------------- 区域 ----------------
        QGCLabel {
            Layout.fillWidth:   true
            color:              qgcPal.text
            font.pointSize:     ScreenTools.smallFontPointSize
            text:               qsTr("区域") + " (" + djiBridgeServer.cloudMapAreas.count + ")"
        }

        ListView {
            Layout.fillWidth:       true
            Layout.preferredHeight: contentHeight
            interactive:            false
            clip:                   true
            spacing:                1
            boundsBehavior:         Flickable.StopAtBounds
            model:                  djiBridgeServer.cloudMapAreas
            delegate:               elementRowComponent

            QGCLabel {
                anchors.horizontalCenter:   parent.horizontalCenter
                color:                      qgcPal.colorGrey
                font.pointSize:             ScreenTools.smallFontPointSize
                visible:                    parent.count === 0
                text:                       qsTr("暂无区域")
            }
        }
    }

    /// 滚轮挡板：这个面板（飞行视图里）浮在地图上面，滚轮不拦就会穿到下面的 Map 上
    /// 变成地图缩放 —— 在元素列表上滚一下，地图跟着放大缩小，列表自己反而一动不动。
    ///
    /// 它只吃滚轮：acceptedButtons 设成 NoButton，按下/点击一律穿透，所以按钮、输入框、
    /// 拖拽滚动都照旧；声明在内容之后（压在内容上），滚轮先到它这儿。
    ///
    /// 面板自己的滚动只能在这里手动转 —— 事件已经被 accepted，QGCFlickable 收不到了。
    /// y 跟着 contentY 走：Flickable 的 children 是内容层（会被 contentY 平移），
    /// 这样挡板才固定贴在视口上，而不是跟着列表滚出去。
    MouseArea {
        width:              root.width
        height:             root.height
        y:                  root.contentY
        acceptedButtons:    Qt.NoButton

        onWheel: (wheel) => {
            wheel.accepted = true
            // 鼠标滚轮给 angleDelta（一格 120），触摸板给 pixelDelta
            var delta = (wheel.pixelDelta.y !== 0) ? wheel.pixelDelta.y : (wheel.angleDelta.y / 2)
            var limit = Math.max(0, root.contentHeight - root.height)
            root.contentY = Math.max(0, Math.min(limit, root.contentY - delta))
        }
    }
}
