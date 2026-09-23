import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGroundControl.ScreenTools
import QGroundControl.Controls
import QGroundControl.Palette

/// 「本地任务」和「云端航线库」两页共用的行样式。
///
/// 刻意**不认识任何 model** —— 显示什么、勾选什么、菜单里放什么全由调用方给。
/// 两个数据源差别很大（本地是 FolderListModel 的角色、名字可以就地改；云端是
/// DjiWaylineListModel 的角色、名字只读），但行的样子必须一模一样，所以样式只在
/// 这一处写。以前两页各写一遍，改一处忘一处，看着就不是一张列表。
///
///     第一行：[勾选]  名称 ........... ★ [⋮] ⊢ [图标][图标][图标]
///     第二行：        修改时间：2026-09-22 10:00
///
/// ★ 是「已收藏」的记号（白色实心星，没有就不占位）。**只有云端页会喂
/// `favorited`** —— 本地 .plan 里没有收藏这个字段，后台也不认本地文件，
/// 本地任务页按用户确认的口径不显示星，两边文档口径一致，别去给本地页补一个。
///
/// 行高、配色、分隔线与原来 PlanListView 里的委托逐字对齐（70 / windowShade /
/// toolbarBackground / groupBorder），换的是右侧那两个「悬停才出现」的图标
/// —— 现在是一个常显的 ⋮，按下去从行右缘往右弹出一条横向图标动作条。
/// 悬停才出现的按钮在列表里很难发现，而且鼠标一移开就点不到了。
///
/// 展开态是**输入**属性（`expanded`）：同一时刻只该有一行张开，而「别的行开没开」
/// 只有列表知道，行自己记不住。
///
/// **不要再改回 QGCMenu 那种弹窗菜单**：在深色列表上它会弹出一块白板
/// （Basic 风格 + 从没人给 QApplication 设过 palette），得逐项接 palette 才压住。
/// 现在这条动作条是自绘底色的 Popup，不吃默认背景。
Rectangle {
    id: row

    // ---------------- 输入（调用方绑） ----------------
    property string title:          ""
    property string metaText:       ""
    /// 名字是不是一个可就地编辑的输入框。本地任务 true，云端航线 false
    /// （云端改名要走「下载 → 重新上传 → 取消收藏旧的」，不能靠输入框敲一下回车就发出去）
    property bool   nameEditable:   false
    property bool   checked:        false
    property bool   current:        false
    /// 已收藏。为 true 时在 ⋮ 左边显示一颗星 —— 收藏与否光看名字看不出来，
    /// 得有个能一眼扫过去的记号
    property bool   favorited:      false
    property real   rowHeight:      70
    property color  normalColor:    qgcPal.windowShade
    property color  highlightColor: qgcPal.toolbarBackground
    /// [{ text, icon, enabled }]，最多四条（云端四条、本地三条）。
    /// text 是图标按钮的提示文案，icon 是 qrc 里的图标路径。图标条是 Repeater 生成的，
    /// 加一项不用改本文件 —— 数组多长就出现几个按钮
    property var    actions:        []
    /// 动作图标条是不是展开着。**输入属性，由调用方驱动** —— 见文件头
    property bool   expanded:       false
    /// 悬停整行时弹出的提示，空串则不弹。云端用它显示机型/负载/模板（那三项在
    /// 窄行里放不下，之前是整页表格的三列）
    property string hoverTip:       ""

    // ---------------- 输出（调用方接） ----------------
    /// 点了行本身（不是勾选框、不是名字输入框、不是 ⋮）
    signal activated()
    signal checkToggled()
    signal nameEdited(string text)
    signal actionTriggered(int index)
    /// 点了 ⋮（或按下键盘 Menu 键）。调用方按 expanded 取反 ——
    /// 行自己不翻转 expanded，那是调用方的事
    signal menuToggled()

    QGCPalette { id: qgcPal; colorGroupEnabled: true }

    readonly property real _pad: ScreenTools.defaultFontPixelWidth / 2

    /// 把名字输入框的文字强制写回（重命名失败时调用方要用它回滚）
    function resetNameText(text) {
        nameField.text = text
    }

    /// 让名字输入框拿到焦点（本地页「重命名」菜单项走这里）
    function focusNameField() {
        if (nameEditable) {
            nameField.forceActiveFocus()
        }
    }

    /// 点 ⋮ 时走这里。只发信号，不翻转自己的 expanded
    function toggleMenu() {
        row.menuToggled()
    }

    /// 图标尺寸。比底部那条图标栏（`defaultFontPixelHeight * 2`）小一号 ——
    /// 这里是一条行内浮出来的动作条，不是页面级动作栏
    readonly property real _actionIconSize: ScreenTools.defaultFontPixelHeight * 1.4
    /// ⋮ 的边长
    readonly property real _dotsSize: ScreenTools.defaultFontPixelHeight * 1.2

    width:  parent ? parent.width : 0
    height: rowHeight
    color:  (current || rowHover.containsMouse) ? highlightColor : normalColor

    /// 展开状态是调用方给的，动作条自己开不了也关不了自己 —— 一改就同步过去。
    ///
    /// 不用 `open: row.expanded` 那种绑定：CloseOnPressOutside 关掉弹窗时会把 open
    /// 命令式地写成 false，绑定当场断掉，之后再点 ⋮ 就不会再弹了（expanded 明明翻了
    /// 一遍，屏幕上看不出任何变化）。这里改成单向地跟着 expanded 走
    onExpandedChanged: {
        if (row.expanded) {
            actionPopup.open()
        } else {
            actionPopup.close()
        }
    }

    ToolTip {
        // 悬停在星上时让位给星自己的「已收藏」提示 —— 两个 ToolTip 同时冒出来
        // 会叠成两块板子
        visible:      rowHover.containsMouse && row.hoverTip !== "" && !row.expanded
                      && !starArea.containsMouse
        delay:        600
        text:         row.hoverTip
    }

    // 整行的点击区。放在内容**下面**（声明在前、位于 z 序下层）——
    // 没有鼠标处理的子项（QGCLabel 等）不吃事件，会落到这里；
    // 勾选框 / 名字输入框 / ⋮ 各自有 MouseArea，它们会先吃掉自己的点击。
    MouseArea {
        id:                      rowHover
        anchors.fill:            parent
        hoverEnabled:            true
        propagateComposedEvents: true
        onClicked:               row.activated()
    }

    Column {
        id:      contentColumn
        anchors.fill: parent
        spacing: 2

        RowLayout {
            id:                   topRow
            anchors.left:         parent.left
            anchors.right:        parent.right
            anchors.leftMargin:   5
            anchors.rightMargin:  5
            anchors.topMargin:    4
            spacing:              2

            // 多选用。调用方把 checked 绑到自己的选中集合上；
            // CheckBox 点击时会命令式写自己的 checked 把绑定顶掉 —— 这里装回去。
            // 注意顶掉的是**本组件**的 checked，调用方绑在 row.checked 上的那个绑定
            // 谁也碰不到，所以这一步只需要在这里做一次（原来两页各写一遍）
            QGCCheckBox {
                Layout.alignment: Qt.AlignVCenter
                checked:          row.checked
                onClicked: {
                    row.checkToggled()
                    checked = Qt.binding(function () { return row.checked })
                }
            }

            // 名字只读时用它（云端）
            QGCLabel {
                Layout.fillWidth:  true
                Layout.alignment:  Qt.AlignLeft | Qt.AlignVCenter
                Layout.leftMargin: 5
                text:              row.title
                font.bold:         true
                elide:             Text.ElideRight
                visible:           !row.nameEditable
            }

            // 名字可编辑时用它（本地任务，就地改名）
            TextField {
                id:                nameField
                Layout.fillWidth:  true
                Layout.alignment:  Qt.AlignLeft | Qt.AlignVCenter
                Layout.leftMargin: 5
                implicitHeight:    28
                text:              row.title
                color:             qgcPal.text
                font.bold:         true
                font.pointSize:    ScreenTools.defaultFontPointSize
                visible:           row.nameEditable
                enabled:           row.current
                hoverEnabled:      row.current
                background: Rectangle {
                    anchors.fill: parent
                    color:        "transparent"
                    border.color: (parent.hovered || parent.focus) ? qgcPal.colorGrey : "transparent"
                }
                leftPadding: focus ? 5 : 0
                onEditingFinished: {
                    focus = false
                    row.nameEdited(text)
                }
            }

            // 已收藏的记号，在 ⋮ 左边。白色实心星，扫一眼就能从一列里挑出来
            // （不上 ❌ 那种「空心/实心两态」：行里的收藏只有「是」这一种显示，
            // 没收藏的行干干净净，不需要再立一个灰星去占位）
            QGCColoredImage {
                id:                favStar
                Layout.alignment:  Qt.AlignRight | Qt.AlignVCenter
                Layout.rightMargin: 2
                visible:           row.favorited
                source:            "/InstrumentValueIcons/star-full.svg"
                // 用 qgcPal.text（跟随主题的前景色）而不是字面 "white"：
                // 与这一行其它图标同一个色源，换主题不会留下一颗孤零零的纯白星
                color:             qgcPal.text
                width:             row._dotsSize
                height:            row._dotsSize
                sourceSize.height: height
                fillMode:          Image.PreserveAspectFit

                MouseArea {
                    id:              starArea
                    anchors.fill:    parent
                    anchors.margins: -4
                    hoverEnabled:    true
                }

                ToolTip {
                    visible: starArea.containsMouse
                    text:    qsTr("已收藏")
                }
            }

            // ⋮ —— 常显。做法照 RallyPointItemEditor.qml 的汉堡菜单：
            // QGCColoredImage + MouseArea。展开时高亮，当作「已按下」的反馈
            // （下面那条图标条的开关就是它，没有别的状态可看）
            QGCColoredImage {
                id:                dots
                Layout.alignment:  Qt.AlignRight | Qt.AlignVCenter
                source:            "/InstrumentValueIcons/dots-horizontal-triple.svg"
                color:             (dotsArea.containsMouse || row.expanded) ? qgcPal.text : qgcPal.colorGrey
                width:             row._dotsSize
                height:            row._dotsSize
                sourceSize.height: height
                fillMode:          Image.PreserveAspectFit

                MouseArea {
                    id:              dotsArea
                    anchors.fill:    parent
                    anchors.margins: -4          // 图标只有十几个像素，命中区放大一点
                    hoverEnabled:    true
                    onClicked:       row.toggleMenu()
                }
            }
        }

        QGCLabel {
            anchors.left:         topRow.left
            anchors.right:        topRow.right
            anchors.leftMargin:   5
            anchors.rightMargin:  5
            text:                 row.metaText
            // 用 defaultFontPointSize 而不是 smallFontPointSize：
            // small 在 1080p/100% 下只有 9pt，这一行是行里唯一的补充信息，
            // 看不清就等于没有（原来两页都是 small，两页一起看不清）
            font.pointSize:       ScreenTools.defaultFontPointSize
            opacity:              0.7
            elide:                Text.ElideRight
        }

    }

    // ⊢ 动作图标：按 ⋮ 之后从行右缘**往右弹出来**的一条横向图标条，
    // 横着挂在 ⋮ 的右边（行本身不长高，列表不会因为弹一下就整体往下窜）。
    //
    // 左栏只有 300px，这条会压在地图上 —— 所以要自己铺一层不透明底 + 描边，
    // 不能像弹窗菜单那样指望控件的默认背景（Basic 风格的默认背景是浅色的，
    // 深色界面上弹出来就是一块白板，这坑在上一版踩过）。
    //
    // 用 QGCIconButton（= 底部动作栏那种图标按钮）：这里要的就是「这是个按钮」的样子。
    // 别拿它当 ⋮ 用 —— 它没有 color 属性，靠 showNormal 在悬停/按下时变灰，
    // 灰在深色行上像禁用
    Popup {
        id:      actionPopup
        padding: 3

        // 位置相对于父项（也就是这一行）：横着从 ⋮ 的右缘再往外一点开始，
        // 纵向跟第一行（⋮ 所在那一行）居中对齐
        x: row.width - 5 + 4
        y: 4 + (topRow.height - height) / 2

        // 宽高按那条图标条算。Popup 默认的 contentItem 是个空 Item，
        // 塞在里面的 Row 撑不出隐式尺寸，只能自己算
        width:  actionRow.implicitWidth  + padding * 2
        height: actionRow.implicitHeight + padding * 2

        background: Rectangle {
            color:        qgcPal.window
            border.color: qgcPal.groupBorder
            border.width: 1
            radius:       3
        }

        // 点到别处（CloseOnPressOutside）或按 Esc 收起时，要回头告诉调用方 ——
        // 否则调用方那边还记着「这行开着」，再点 ⋮ 就是「关」，
        // expanded 翻了一遍而屏幕上什么也没发生
        onClosed: {
            if (row.expanded) {
                row.menuToggled()
            }
        }

        Row {
            id:      actionRow
            spacing: 4

            // 照 actions 数组生成，加一项不用改本文件
            Repeater {
                model: row.actions

                delegate: QGCIconButton {
                    width:      row._actionIconSize
                    height:     row._actionIconSize
                    iconSource: modelData.icon !== undefined ? modelData.icon : ""
                    enabled:    modelData.enabled !== false
                    onClicked:  row.actionTriggered(index)

                    ToolTip.visible: hovered
                    ToolTip.text:    modelData.text !== undefined ? modelData.text : ""
                }
            }
        }
    }

    // 行底那条 1px 分隔线
    Rectangle {
        height:        1
        anchors.bottom: parent.bottom
        anchors.left:   parent.left
        anchors.right:  parent.right
        color:          qgcPal.groupBorder
    }

    // 键盘：Menu 键展开/收起动作图标条、回车当点击，其余（↑/↓ 等）留给父 QGCListView 做导航
    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_Menu) {
            row.toggleMenu()
            event.accepted = true
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            row.activated()
            event.accepted = true
        }
    }
}
