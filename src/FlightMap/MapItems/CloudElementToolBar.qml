/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

import QtQuick
import QtQuick.Layouts

import QGroundControl
import QGroundControl.ScreenTools
import QGroundControl.Controls
import QGroundControl.Palette

/// 地图右侧的云元素图标列（对齐 DJI Cloud 网页版地图元素工具条）
///
/// 只管"触发动作"和"列出元素"，元素本身画在 CloudElementMapLayer 里、编辑逻辑在
/// DjiCloudMapClient 里。飞行视图与规划视图共用这一份：
///   - 飞行视图：showElementPanel = true，元素列表 + 编辑表单从图标列左侧弹出
///   - 规划视图：showElementPanel = false，那边右侧面板已经是编辑器（第 4 个图层 tab）
///
/// 六颗按钮分三档（见 _buttons 的注释）：
///   端点 / 线段 / 区域   —— 可绘制、可保存到平台的地图元素，顺序与官方控制台一致
///   任务区域 / GEO 区域  —— 只读飞行区域图层（后端 flight-area），点一下开/关
///   元素列表             —— 元素面板开关（规划视图不显示这颗）
///
/// 前五颗是**互斥**的（单选）：同一时刻只亮一颗，切换动作见 _activate()。
/// 元素列表那颗不在这个组里 —— 它是面板开关，绘制时面板得开着。
///
/// 图标用 Rectangle 拼几何图形，不引 svg：省一条 qrc/qmldir 登记，缩放也不会糊。
/// 「端点」是菱形、「任务区域」以前也是菱形 —— 两颗粒子长得一样，用户会把任务区域那
/// 当成"点一下放个点"的工具，点完再去点地图什么都不会发生。任务区域现在改画"图层"。
///
/// 注意：面板是普通 Rectangle 而不是 Popup —— 标绘要点地图，Popup 只要带上
/// CloseOnPressOutside 就会在第一次点地图时把自己关掉（主窗口的 indicatorDrawer
/// 就是这么个坑，所以那边的「元素」tab 只能放列表）。
Item {
    id:         root

    /// 要操作的地图，编辑器里的「定位」按钮用它
    property var    map
    /// 是否自带元素面板；规划视图为 false
    property bool   showElementPanel: true

    /// DJI 控制台配色（和地图上实际用的颜色是同一组）
    readonly property color _accentBlue:  "#2D8CF0"
    readonly property color _accentGreen: "#19BE6B"   ///< 任务区域
    readonly property color _accentRed:   "#E23C39"   ///< GEO 区域

    readonly property real _btnSize:    ScreenTools.defaultFontPixelHeight * 1.9
    readonly property real _margin:     ScreenTools.defaultFontPixelWidth / 2
    readonly property real _maxPanelHeight: map ? map.height * 0.8 : ScreenTools.defaultFontPixelHeight * 20

    readonly property bool _connected:  djiBridgeServer.cloudWsConnected
    readonly property var  _editing:    djiBridgeServer.cloudMapEditing

    /// 图标列的内容。三档：
    ///   type  != undefined —— 可绘制的地图元素（端点 0 / 线段 1 / 区域 2）
    ///   layer != undefined —— 只读飞行区域图层的显示开关（任务区域 / GEO 区域）
    ///   "list"             —— 元素面板开关
    /// 前五颗互斥（同一时刻只亮一颗），元素列表是独立的面板开关。
    readonly property var _buttons: [
        { kind: "point",    type: 0,         layer: undefined,  glyph: "diamond", tint: _accentBlue,  tip: qsTr("端点") },
        { kind: "line",     type: 1,         layer: undefined,  glyph: "line",    tint: _accentBlue,  tip: qsTr("线段") },
        { kind: "area",     type: 2,         layer: undefined,  glyph: "square",  tint: _accentBlue,  tip: qsTr("区域") },
        { kind: "taskArea", type: undefined, layer: "taskArea", glyph: "layers",  tint: _accentGreen, tip: qsTr("任务区域") },
        { kind: "geoZone",  type: undefined, layer: "geoZone",  glyph: "geo",     tint: _accentRed,   tip: qsTr("GEO 区域") },
        { kind: "list",     type: undefined, layer: undefined,  glyph: "list",    tint: _accentBlue,  tip: qsTr("元素列表") }
    ]

    property bool   _panelVisible: false
    property string _hint: ""

    implicitWidth:  _btnSize
    implicitHeight: _column.implicitHeight

    QGCPalette { id: qgcPal }

    // 断开上云后本地元素会被清空，面板再留着就是个空壳
    on_ConnectedChanged: {
        if (!_connected) {
            _panelVisible = false
        }
    }

    /// 桩提示：不支持的按钮和未连接时的点击都走这里，3 秒后自己消失
    function _showHint(text) {
        _hint = text
        hintTimer.restart()
    }

    /// 让某一颗地图内容按钮成为唯一"选中"的那颗 —— 端点 / 线段 / 区域 / 任务区域 / GEO 区域
    /// 五颗互斥，和 DJI 控制台的工具条一样是个单选框：
    ///   点绘制键   → 关掉两个只读图层，再开绘制会话（图层压在上面没法画）
    ///   点图层键   → 结束绘制会话，再开这个图层、关另一个
    ///   再点当前这颗图层键 → 关掉它（等于取消选择）
    /// 元素列表那颗不在这个组里：它是面板开关，绘制时面板得开着才有「保存到平台」。
    function _activate(kind) {
        var isLayer = (kind === "taskArea" || kind === "geoZone")

        if (isLayer) {
            // 选中的那颗再点一下 = 关掉
            if ((kind === "taskArea" && djiBridgeServer.cloudShowTaskAreas) ||
                (kind === "geoZone"  && djiBridgeServer.cloudShowGeoZones)) {
                if (kind === "taskArea") {
                    djiBridgeServer.cloudToggleTaskAreas()
                } else {
                    djiBridgeServer.cloudToggleGeoZones()
                }
                _showHint(djiBridgeServer.cloudFlightAreaStatus)
                return
            }
            // 看图层和画图形不同时进行：正在画就先收掉（草稿也会一并丢掉）
            if (_editing !== null) {
                djiBridgeServer.cloudMapCancelEdit()
            }
            if (kind === "taskArea") {
                if (djiBridgeServer.cloudShowGeoZones) {
                    djiBridgeServer.cloudToggleGeoZones()
                }
                djiBridgeServer.cloudToggleTaskAreas()
            } else {
                if (djiBridgeServer.cloudShowTaskAreas) {
                    djiBridgeServer.cloudToggleTaskAreas()
                }
                djiBridgeServer.cloudToggleGeoZones()
            }
            _showHint(djiBridgeServer.cloudFlightAreaStatus)
            return
        }

        // 绘制键：0=端点 1=线段 2=区域
        var type = (kind === "point") ? 0 : (kind === "line") ? 1 : 2

        // 已经在画这一种就不再重开（重开等于把当前草稿丢掉，按「取消」才是有意的），
        // 只把"点地图"重新打开 —— 可能刚按过「结束标绘 / 结束放置」，又想接着画
        if (_editing !== null && _editing.type === type) {
            _editing.setTracing(true)
            if (type === 0) {
                _showHint(qsTr("在地图上点一下放一个端点，可以连着点"))
            }
            return
        }

        if (djiBridgeServer.cloudShowTaskAreas) {
            djiBridgeServer.cloudToggleTaskAreas()
        }
        if (djiBridgeServer.cloudShowGeoZones) {
            djiBridgeServer.cloudToggleGeoZones()
        }
        if (showElementPanel) {
            _panelVisible = true
        }
        djiBridgeServer.cloudMapBeginCreate(type)
        // 端点建完就在等地图点击了，提示一句，免得对着面板找「开始标绘」
        if (type === 0) {
            _showHint(qsTr("在地图上点击放置端点"))
        }
    }

    Timer {
        id:          hintTimer
        interval:    3000
        onTriggered: root._hint = ""
    }

    /// 一个图标按钮
    Component {
        id: iconButtonComponent

        Rectangle {
            id:                 buttonRect
            required property var modelData

            /// 这颗按钮的主色（线段/区域蓝、任务区域绿、GEO 区域红）
            readonly property color _tint:     modelData.tint
            readonly property bool _isList:    modelData.kind === "list"
            readonly property bool _isLayer:   modelData.layer !== undefined
            readonly property bool _supported: modelData.type !== undefined
            /// 现在就能用：列表键随时可用，图层键与绘制键都要已连上云平台
            readonly property bool _available: _isList || ((_isLayer || _supported) && root._connected)
            /// 选中态：列表键看面板，图层键看该图层开没开，绘制键看正在编辑的是不是这一种
            readonly property bool _active:    _isList  ? root._panelVisible
                                             : _isLayer ? (modelData.layer === "taskArea"
                                                               ? djiBridgeServer.cloudShowTaskAreas
                                                               : djiBridgeServer.cloudShowGeoZones)
                                             : (root._editing !== null && root._editing.type === modelData.type)
            readonly property real _barWidth:  width * 0.5

            // 规划视图没有这个面板（右侧面板就是编辑器），列表键留着是个点了没反应的死键
            visible:      root.showElementPanel || !_isList
            width:        root._btnSize
            height:       root._btnSize
            radius:       3
            // 选中态用"半透明的本色底 + 本色描边"：既看得出选的是哪颗（绿/蓝/红各不相同），
            // 又不会像以前那样把底色铺成高亮蓝 —— 线段那颗的图标本身就是蓝的，
            // 铺上高亮蓝之后图标和底色同色，看上去就是"选中了反而没有图标"。
            color:        _active ? Qt.rgba(_tint.r, _tint.g, _tint.b, 0.3)
                                  : (mouseArea.pressed ? qgcPal.buttonHighlight : qgcPal.windowShade)
            border.width: _active ? 2 : 1
            border.color: _active ? _tint : qgcPal.text
            opacity:      _available ? 1 : 0.4

            MouseArea {
                id:           mouseArea
                anchors.fill: parent
                onClicked: {
                    if (buttonRect._isList) {
                        root._panelVisible = !root._panelVisible
                        return
                    }
                    // 图层键和绘制键都要已连上云平台
                    if (!root._connected) {
                        root._showHint(buttonRect._isLayer
                                           ? qsTr("未连接云平台，看不到平台上的区域。")
                                           : qsTr("未连接云平台，无法新建元素。"))
                        return
                    }
                    if (!buttonRect._supported && !buttonRect._isLayer) {
                        root._showHint(qsTr("%1 本次未接入：只做了线段与区域，没有圆。").arg(buttonRect.modelData.tip))
                        return
                    }
                    root._activate(buttonRect.modelData.kind)
                }
            }

            Item {
                anchors.centerIn: parent
                width:            parent.width * 0.52
                height:           parent.height * 0.52

                // 端点：菱形（正方形转 45°）—— 和地图上那个菱形图钉是同一个形状
                Rectangle {
                    visible:          buttonRect.modelData.glyph === "diamond"
                    anchors.centerIn: parent
                    width:            parent.width * 0.7
                    height:           width
                    rotation:         45
                    color:            "transparent"
                    border.width:     2
                    border.color:     buttonRect.modelData.tint
                }

                // 线段：一根斜杠
                Rectangle {
                    visible:          buttonRect.modelData.glyph === "line"
                    anchors.centerIn: parent
                    width:            parent.width
                    height:           2
                    rotation:         -45
                    color:            buttonRect.modelData.tint
                }

                // 区域：空心方框
                Rectangle {
                    visible:          buttonRect.modelData.glyph === "square"
                    anchors.centerIn: parent
                    width:            parent.width * 0.85
                    height:           width
                    color:            "transparent"
                    border.width:     2
                    border.color:     buttonRect.modelData.tint
                }

                // 任务区域：两张错位的方框（"图层"的意思）
                // 这里原来是菱形，和「端点」撞脸 —— 用户会把它当成放点工具
                Item {
                    visible:          buttonRect.modelData.glyph === "layers"
                    anchors.centerIn: parent
                    width:            parent.width * 0.85
                    height:           width

                    Rectangle {
                        x:                0
                        y:                0
                        width:            parent.width * 0.62
                        height:           parent.height * 0.62
                        color:            "transparent"
                        border.width:     2
                        border.color:     buttonRect.modelData.tint
                    }

                    // 前面这张实心盖住后面那张的一角，"两张叠着"才看得出来
                    Rectangle {
                        x:                parent.width * 0.38
                        y:                parent.height * 0.38
                        width:            parent.width * 0.62
                        height:           parent.height * 0.62
                        color:            buttonRect.modelData.tint
                    }
                }

                // GEO 区域：红框 + 半透明红填充
                Rectangle {
                    visible:          buttonRect.modelData.glyph === "geo"
                    anchors.centerIn: parent
                    width:            parent.width * 0.85
                    height:           width
                    color:            Qt.rgba(buttonRect.modelData.tint.r, buttonRect.modelData.tint.g,
                                              buttonRect.modelData.tint.b, 0.35)
                    border.width:     2
                    border.color:     buttonRect.modelData.tint
                }

                // 元素列表：三条横杠
                Column {
                    visible:          buttonRect.modelData.glyph === "list"
                    anchors.centerIn: parent
                    spacing:          parent.height * 0.18

                    Repeater {
                        model: 3

                        Rectangle {
                            width:  buttonRect._barWidth
                            height: 2
                            color:  buttonRect._available ? qgcPal.buttonText : qgcPal.colorGrey
                        }
                    }
                }
            }
        }
    }

    Column {
        id:      _column
        width:   root._btnSize
        spacing: root._margin

        Repeater {
            model:    root._buttons
            delegate: iconButtonComponent
        }
    }

    /// 元素面板：从图标列左侧弹出，只有飞行视图用
    Rectangle {
        id:                     elementPanel
        visible:                root._panelVisible && root.showElementPanel
        anchors.right:          _column.left
        anchors.rightMargin:    root._margin
        anchors.top:            _column.top
        width:                  ScreenTools.defaultFontPixelWidth * 24
        // CloudElementEditor 是 QGCFlickable，contentHeight 由内容列推出来（只跟宽度有关），
        // 所以"高度取自 contentHeight"不会成环；再夹一个上限免得顶出地图。
        height:                 Math.min((editorLoader.item ? editorLoader.item.contentHeight : 0) + (root._margin * 2),
                                        root._maxPanelHeight)
        color:                  qgcPal.windowShade
        radius:                 3
        border.width:           1
        border.color:           qgcPal.text

        // 用 Loader 而不是直接摆一个：规划视图里 showElementPanel 为 false，
        // 直接摆的话那张地图上会常驻一个看不见、却挂着两个 ListView 的编辑器。
        Loader {
            id:              editorLoader
            anchors.fill:    parent
            anchors.margins: root._margin
            active:          root.showElementPanel
            sourceComponent: cloudElementEditorComponent
        }
    }

    Component {
        id: cloudElementEditorComponent

        CloudElementEditor {
            compactMode: true
            flightMap:   root.map
        }
    }

    /// 提示行贴在图标列下方；比图标列宽，往左溢出，不遮地图中间。
    /// 元素面板弹出来的时候要再往左让一让 —— 面板占的正是这一片，压在面板上就成了一行
    /// 被挡住一半、谁也读不出来的橙字。
    QGCLabel {
        id:                  hintLabel
        anchors.top:         _column.bottom
        anchors.topMargin:   root._margin
        anchors.right:       elementPanel.visible ? elementPanel.left : parent.right
        anchors.rightMargin: elementPanel.visible ? root._margin : 0
        width:               ScreenTools.defaultFontPixelWidth * 15
        horizontalAlignment: Text.AlignRight
        wrapMode:            Text.WordWrap
        visible:             root._hint.length > 0
        text:                root._hint
        color:               qgcPal.colorOrange
        font.pointSize:      ScreenTools.smallFontPointSize
    }

    /// 飞行区域的拉取结果。两个图层是异步拉的，3 秒就消失的 _hint 说不清楚
    /// "拉了没有、拉到几个"，所以只要有一层开着就常驻一行（"任务区域 3 个 / GEO 区域 0 个"）。
    QGCLabel {
        anchors.top:         hintLabel.visible ? hintLabel.bottom : _column.bottom
        anchors.topMargin:   root._margin
        anchors.right:       elementPanel.visible ? elementPanel.left : parent.right
        anchors.rightMargin: elementPanel.visible ? root._margin : 0
        width:               ScreenTools.defaultFontPixelWidth * 15
        horizontalAlignment: Text.AlignRight
        wrapMode:            Text.WordWrap
        visible:             (djiBridgeServer.cloudShowTaskAreas || djiBridgeServer.cloudShowGeoZones)
                                 && djiBridgeServer.cloudFlightAreaStatus.length > 0
        text:                djiBridgeServer.cloudFlightAreaStatus
        color:               qgcPal.colorOrange
        font.pointSize:      ScreenTools.smallFontPointSize
    }
}
