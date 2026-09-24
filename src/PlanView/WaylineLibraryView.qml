/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

import QtQuick          2.12
import QtQuick.Layouts  1.3
import QtQuick.Controls 2.12

import QGroundControl.Controls     1.0
import QGroundControl.ScreenTools  1.0

/// 云端航线库 —— 左栏那一页（和「本地任务」同一个位置、同一个宽度）
///
/// 行样式与「本地任务」**完全共用** MissionListRow.qml：一张卡片一条航线，
///     第一行：[勾选] 航线名称 ................. [⋮] ⊢ [下载并载入][重命名][收藏][删除]
///     第二行：        修改时间：2026-09-22 22:34  ·  航点飞行
/// 操作全在这一行的 ⋮ 里：按下去从行右缘往右弹出一条横向图标条，不再做成
/// 「悬停才出现的图标」—— 那种按钮鼠标一移开就没了。
///
/// 机型 / 负载 / 模板不作为筛选控件（之前照 design/wayline_ui_prototype.html 做过
/// 整页九列表格和三个下拉，左栏 300px 塞不下，而且信息量很低：我们只按 M3E 一套
/// 枚举上传，外部来的 kmz 也一律登记成 M3E，那三列基本恒定）。它们现在在
/// **悬停提示**和**选中后的右侧详情面板**里显示。
///
/// 选中一条会按需拉那条航线的 kmz（几 KB）解析出航点数 / 起点 / 规划长度 / 航迹，
/// 经 preview 交给 PlanView 画在右侧详情面板和地图上。列表接口本身给不了这些
/// （对过 /v3/api-docs，没有 start_wayline_point，也没有 GET 详情接口）。
///
/// 上传不在这页：上传的对象是本地任务，入口是底部那条动作栏的「上传到云端」。
/// 这页只管云端的增删查改。
Item {
    id: root

    property var wayline: djiBridgeServer ? djiBridgeServer.djiWayline : null

    // ---------------------------------------------------------------------
    // 对外接口
    // ---------------------------------------------------------------------

    /// 下载完成。每条都会发一次：PlanView 把它转成 .plan 落进「本地任务」目录。
    /// autoLoad 为 true 时（单条下载）还额外把它载进编辑器；批量下载时为 false
    /// —— 一次下 5 条连着载 5 次编辑器毫无意义
    signal waylineDownloaded(string kmzPath, string name, bool autoLoad)

    /// 一批下载全部结束了：landed 是拿到的条数，failed 是失败的条数。
    /// 批量下载是并发跑的，一条一条弹窗 —— 一次下 5 条就是 5 个模态框，
    /// 所以逐条不弹，由上层在这个时点汇总说一句。
    ///
    /// 命名和 DjiWaylineManager::batchDownloadFinished 差一个词（这里是
    /// downloadBatchFinished）纯粹是为了在 Connections 里一眼分得清是谁的信号
    signal downloadBatchFinished(int landed, int failed)

    /// 当前选中那条航线的解析结果（内容见 DjiWaylineManager::waylinePreviewReady）。
    /// PlanView 绑它来画地图上的航迹、填右侧详情面板。**每次整体重新赋值**，
    /// 否则 QML 看不到对象内部的变化
    property var    preview:       ({})
    property string previewId:     ""
    property bool   previewBusy:   false
    property bool   previewFailed: false

    /// 哪一行的图标动作条是展开的（-1 = 都没展开）。见 PlanListView 里同一处注释
    property int expandedIndex: -1

    /// 「当前有没有选中一条航线」。详情面板靠它决定露不露面。
    /// currentWayline 没有选中时是空对象 `({})`，取 .id 得 undefined —— 用 id 判空，
    /// 不另设哨兵值（哨兵值迟早会和某种真实 id 撞上，比如 false 与 ""）
    readonly property bool hasWaylineInfo: currentWayline !== undefined && currentWayline !== null
                                           && currentWayline.id !== undefined

    // ---------------------------------------------------------------------
    // 尺寸
    // ---------------------------------------------------------------------

    readonly property real _pad:    ScreenTools.defaultFontPixelWidth
    readonly property real _rowGap: ScreenTools.defaultFontPixelHeight / 3

    // ---------------------------------------------------------------------
    // 显示用的枚举表
    //
    // 表里没有的键**原样显示**，不猜 —— 现场后台出现过 0-91-0 / 1-81-0 这种
    // 表外的值，猜错了比显示原文更糟。数字是 DJI Cloud API 的
    // drone_enum_value / payload_enum_value。
    // ---------------------------------------------------------------------

    /// 后台只认 name / update_time / create_time 三个排序列，别的不认
    readonly property var _orderChoiceKeys:  ["update_time", "name", "create_time"]
    readonly property var _orderChoiceDesc:  [true, false, true]
    readonly property var _orderChoiceNames: [qsTr("最近更新"), qsTr("按名称"), qsTr("最近创建")]

    readonly property var _droneNameTable:   { "77": qsTr("M3E"), "89": qsTr("M3D"), "91": qsTr("M3TD"),
                                               "67": qsTr("M30 / M30T"), "68": qsTr("M350 RTK"), "60": qsTr("M300 RTK") }
    readonly property var _payloadNameTable: { "66": qsTr("M3E 相机"), "81": qsTr("M3TD 相机"), "52": qsTr("M30 双光"),
                                               "53": qsTr("H20T"), "42": qsTr("H20") }

    /// 模板类型。下标就是 template_types 里的数字，0 是航点飞行
    readonly property var _templateNameTable: [qsTr("航点飞行"), qsTr("建图航拍"), qsTr("倾斜摄影")]

    // ---------------------------------------------------------------------
    // 状态
    // ---------------------------------------------------------------------

    /// 勾选的行号（模型下标，不是航线 id）。整体重新赋值，否则 QML 看不到 var 数组内部变化
    property var    selectedRows: []
    /// 点卡片选中的那条。存的是快照对象而不是 listView.currentItem ——
    /// 代理会被回收，currentItem 的字段随时可能变成别的行
    property var    currentWayline: ({})

    property int    pageSize: 20
    property bool   favoritedOnly: false

    /// 本次会话登录云平台了没有。没登录就没有 token：列表拉不到，上传下载也做不了 ——
    /// 这页该显示「未连接服务器」，而不是摆着上一次会话留下的航线（或者一个空列表，
    /// 让人以为云端真的一条都没有）。登录/退出登录时 C++ 侧发 cloudLoggedInChanged，
    /// 见下面的 onLoggedInChanged
    readonly property bool loggedIn: djiBridgeServer ? djiBridgeServer.cloudLoggedIn : false

    /// 最近一次失败原因，显示在底部状态栏。刻意不用模态框：拉列表失败多数是
    /// 云端没连/token 过期，属于"看一眼就知道"的反馈，弹窗只会挡路。
    property string statusText: ""

    property string lastSyncText: "--:--:--"

    /// 最近一次拉列表成功没有。状态行那个小圆点用它，不用 cloudWsConnected
    /// —— 见状态行那里的注释
    property bool   _listOk: false
    property string transferOperation: ""
    property int    transferPercent: 0

    /// 本次下载是不是「单条」。只有单条下载才自动载入编辑器，见 waylineDownloaded
    property bool   _autoLoadOnDownload: false

    /// 批量下载的账不在这里数：errorOccurred 是全局信号（上传/预览/重命名失败
    /// 都发它），在 QML 侧按它扣账会被无关的错误打乱。整批的账由发请求的
    /// C++ 侧算，算完发 batchDownloadFinished，这里只负责往上转发
    // ---------------------------------------------------------------------
    // 显示辅助
    // ---------------------------------------------------------------------

    /// 机型 / 负载的键是三段式 domain-type-subType（0-77-0、1-66-0），
    /// **型号数字在中间那段**。取错段就查不到表，只能原样显示键。
    function _enumSegment(key) {
        var parts = String(key).split("-")
        return parts.length > 1 ? parts[1] : parts[0]
    }

    function _modelText(key) {
        if (!key) return "--"
        var name = _droneNameTable[_enumSegment(key)]
        return name === undefined ? key : name
    }

    function _payloadText(keys) {
        if (!keys || keys.length === 0) return "--"
        var out = []
        for (var i = 0; i < keys.length; ++i) {
            var name = _payloadNameTable[_enumSegment(keys[i])]
            out.push(name === undefined ? keys[i] : name)
        }
        return out.join(" / ")
    }

    function _templateText(types) {
        if (!types || types.length === 0) return "--"
        var out = []
        for (var i = 0; i < types.length; ++i) {
            var t = types[i]
            out.push((t >= 0 && t < _templateNameTable.length) ? _templateNameTable[t] : ("#" + t))
        }
        return out.join(" / ")
    }

    /// 行第二行：时间 · 模板。前缀与本地任务一致（那页就是「修改时间：」），
    /// 两页的行才会是同一张脸
    function _metaText(timeText, templateTypes) {
        var parts = []
        if (timeText) parts.push(timeText)
        var tpl = _templateText(templateTypes)
        if (tpl !== "--") parts.push(tpl)
        return parts.length > 0 ? (qsTr("修改时间：") + parts.join("  ·  ")) : ""
    }

    /// 悬停提示：机型 / 负载 / 上传者这些细节一次看全，省得点开
    function _tooltipText(item) {
        return item.name + "\n"
             + qsTr("机型：") + _modelText(item.drone) + "\n"
             + qsTr("负载：") + _payloadText(item.payload) + "\n"
             + qsTr("模板：") + _templateText(item.template) + "\n"
             + qsTr("上传者：") + (item.user ? item.user : "--") + "\n"
             + qsTr("更新：") + (item.time ? item.time : "--")
             + (item.fav ? ("\n" + qsTr("★ 已收藏")) : "")
    }

    /// 一行的快照。存快照而不是代理的字段：代理会被回收，
    /// 选中之后它的字段随时可能变成别的行
    function _rowSnapshot(item) {
        return {
            "name":     item.name,
            "drone":    item.drone,
            "payload":  item.payload,
            "template": item.template,
            "user":     item.user,
            "time":     item.time,
            "id":       item.id,
            "fav":      item.fav === true
        }
    }

    // ---------------------------------------------------------------------
    // 勾选
    // ---------------------------------------------------------------------

    function _isSelected(row) {
        return selectedRows.indexOf(row) >= 0
    }

    function _toggleRow(row) {
        var rows = selectedRows.slice()
        var i = rows.indexOf(row)
        if (i >= 0) {
            rows.splice(i, 1)
        } else {
            rows.push(row)
        }
        selectedRows = rows
    }

    /// 点一行：设成当前行，并且让它成为唯一的勾选项，然后去取这条的航线数据。
    /// 「点一下选中一条，勾多个框才多选」—— 这样底部动作栏上的批量操作
    /// 永远有对象，不用先教用户去勾框。
    function _selectWayline(row, info) {
        listView.currentIndex = row
        selectedRows = [row]
        currentWayline = _rowSnapshot(info)
        previewId     = currentWayline.id
        preview       = ({})
        previewFailed = false
        previewBusy   = true
        if (!currentWayline.id) {
            // 理论上到不了：列表里每条都有后台给的 id。真要漏了就别发请求 ——
            // 发过去也认不出回的是哪条，面板会一直停在「正在取航线文件…」
            previewBusy   = false
            previewFailed = true
            return
        }
        if (wayline) wayline.requestWaylinePreview(row)
    }

    function _clearSelection() {
        selectedRows = []
    }

    function _setAllSelected(selectAll) {
        if (!selectAll) {
            selectedRows = []
            return
        }
        var rows = []
        for (var i = 0; i < listView.count; ++i) {
            rows.push(i)
        }
        selectedRows = rows
    }

    /// 按航线 id 反查当前行号。
    ///
    /// ⋮ 菜单里的动作**必须**这样重查一遍行号：删除 / 重命名都会让 C++ 侧
    /// 重新拉列表（DjiWaylineManager::deleteWayline / renameWayline 里调
    /// refreshList），行号一变，菜单里闭包存下的那个下标就指到别的航线上了。
    function rowForId(id) {
        if (!wayline || !id) return -1
        for (var i = 0; i < listView.count; ++i) {
            if (wayline.listModel.idAt(i) === id) return i
        }
        return -1
    }

    // ---------------------------------------------------------------------
    // 拉数据
    // ---------------------------------------------------------------------

    /// 机型 / 负载 / 模板不再作为筛选条件，一律"不限"（空串 / -1）。
    /// refreshListEx 那三个参数留着 —— C++ 侧和别的调用方还在用。
    function refresh(page) {
        if (!wayline) return
        if (!loggedIn) {
            // 没登录不发请求 —— 后台只会回鉴权失败，白白把状态行刷成一片红。
            // 上一次登录留下的列表和选中态一并收干净
            _resetListState()
            return
        }
        // 先置灰再拉：拉失败时 onErrorOccurred 只写 statusText，不动 _listOk，
        // 所以这个 false 会一直留着 —— 正好是「接口挂了」的样子。
        // 下载/上传失败也走 onErrorOccurred，但那条路径不经过这里，不会把
        // 列表的状态带坏。
        _listOk = false
        _clearSelection()
        listView.currentIndex = -1
        currentWayline = ({})
        preview        = ({})
        previewId      = ""
        previewFailed  = false
        previewBusy    = false
        wayline.refreshListEx(page === undefined ? 1 : page, pageSize,
                              searchField.text,
                              "",                     // 机型：不限
                              "",                     // 负载：不限
                              -1,                     // 模板：不限
                              favoritedOnly,
                              _orderChoiceKeys[orderCombo.currentIndex],
                              _orderChoiceDesc[orderCombo.currentIndex])
    }

    function refreshFromFirstPage() {
        refresh(1)
    }

    /// 把这一页的列表状态收干净：状态行、勾选、当前行、右侧详情、地图上那条蓝航迹。
    /// 没登录时用它，退出登录时也用它
    function _resetListState() {
        _listOk    = false
        statusText = ""
        clearPreview()
    }

    /// 登录状态变了：登录上就拉第一页（这页可能正开着，切回来就是现成的），
    /// 退出登录就把列表收干净。列表本身在 model 上也会被摘掉，见 listView.model
    onLoggedInChanged: {
        if (loggedIn) {
            refreshFromFirstPage()
        } else {
            _resetListState()
        }
    }

    /// 上传成功后由 PlanView 调用：从第一页重拉，让新航线出现在列表里
    function refreshAfterUpload() {
        refreshFromFirstPage()
    }

    /// 当前这一页已加载的云端航线名，给上传弹窗做重名预判用。
    ///
    /// 刻意用**本地精确比对**而不是调后台的 checkDuplicateNames —— 后台那个是
    /// 「按前缀查」：传 "巡检" 连 "巡检01" 也算重名。拿它给自动加后缀做依据会
    /// 永远收敛不了（"巡检_2" 同样以 "巡检" 开头，还是判重名）。这里只求"别撞上
    /// 已经看得见的名字"，真正撞了后台会拒，上传结果里如实报出来就是。
    function loadedNames() {
        var out = []
        if (!wayline) return out
        for (var i = 0; i < listView.count; ++i) {
            var n = wayline.listModel.nameAt(i)
            if (n) out.push(n)
        }
        return out
    }

    // ---------------------------------------------------------------------
    // 行菜单的动作（在 MissionListRow 的 ⋮ 里）
    // ---------------------------------------------------------------------

    /// i: 0=下载并载入 1=重命名 2=收藏/取消收藏 3=删除
    function _rowAction(i, id, name, fav) {
        if (!wayline) return

        // 收藏只需要 id，不依赖行号
        if (i === 2) {
            wayline.collectWaylines([id], !fav)
            return
        }

        var row = rowForId(id)
        if (row < 0) {
            // 列表已经变了（后台刚刷过），这条不在当前页里 —— 宁可不动作，
            // 也不能按旧下标操作到别的航线上
            root.statusText = qsTr("这条航线已经不在当前页里了，请刷新后再试")
            return
        }

        if (i === 0) {
            _autoLoadOnDownload = true          // 单条下载 —— 下完直接载进编辑器
            wayline.downloadWayline(row)
        } else if (i === 1) {
            renameDialog.createObject(mainWindow, { "row": row, "waylineName": name }).open()
        } else if (i === 3) {
            deleteDialog.createObject(mainWindow, { "row": row, "waylineName": name }).open()
        }
    }

    /// 底部动作栏「下载任务」在云端页的行为：把勾选的几条都下下来，
    /// 不自动载入编辑器（一次下 N 条连着载 N 次毫无意义）
    function downloadSelected() {
        if (!wayline || selectedRows.length === 0) return
        _autoLoadOnDownload = false
        wayline.downloadWaylines(selectedRows)
        _clearSelection()
    }

    /// 底部动作栏「删除」在云端页的行为：删掉勾选的几条。
    /// 后台没有批量删除接口，C++ 侧按 id 一条条删；后台不支持 DELETE 时会降级成
    /// 「取消收藏」（和单条删除同一套，详见 DjiWaylineManager::deleteWaylines）
    function deleteSelected() {
        if (!wayline || selectedRows.length === 0) return
        batchDeleteDialog.createObject(mainWindow, { "rows": selectedRows.slice() }).open()
    }

    /// 把当前选中航线的解析结果整个清掉：地图上那条蓝色航迹、右侧详情面板的
    /// 解析段、行选中态一起收走。
    ///
    /// 蓝色航迹是「库里那条航线」的预览，不是正在编的任务。切页、下载完、
    /// 进编辑模式时都得把它收掉 —— 留着的话，用户开始编自己的任务以后地图上
    /// 还挂着一条别人的航线，看着像是任务里多了几个航点。
    function clearPreview() {
        preview        = ({})
        previewId      = ""
        previewFailed  = false
        previewBusy    = false
        currentWayline = ({})
        _clearSelection()
        listView.currentIndex = -1
    }

    // =====================================================================
    // 页面
    // =====================================================================

    ColumnLayout {
        anchors.fill:    parent
        anchors.margins: 2
        spacing:         root._rowGap

        // -----------------------------------------------------------------
        // 搜索 + 收藏过滤 + 排序
        // -----------------------------------------------------------------
        QGCTextField {
            id:                     searchField
            Layout.fillWidth:       true
            placeholderText:        qsTr("搜索航线名称…")
            // 没登录时这一排（搜索 / 收藏过滤 / 排序）全是空转的：搜索框敲回车
            // 也不会发请求，看着却像能用，索性整排置灰
            enabled:                root.loggedIn
            onAccepted:             root.refreshFromFirstPage()
        }

        RowLayout {
            Layout.fillWidth: true
            spacing:          root._pad / 2
            enabled:          root.loggedIn

            // 选中态用 qgcPal.buttonHighlight（蓝）而不是 primaryButton：
            // primaryButton 在本主题下是青色 #8cb3be，跟这一排「二选一筛选开关」的身份
            // 对不上；蓝色正是全应用统一的选中色（QGCTabButton 高亮、标签栏选中的就是它）。
            //
            // 不走 Button 的 checkable/checked：checked 绑定到 favoritedOnly 上以后，
            // 点一下按钮会由控件自己写 checked，写一次就把这个绑定**拆掉**，
            // 之后 favoritedOnly 再变它也回不来了。所以颜色直接按状态算
            QGCButton {
                text:            qsTr("全部")
                backgroundColor: !root.favoritedOnly ? qgcPal.buttonHighlight : qgcPal.button
                textColor:       !root.favoritedOnly ? qgcPal.buttonHighlightText : qgcPal.buttonText
                onClicked: {
                    root.favoritedOnly = false
                    root.refreshFromFirstPage()
                }
            }
            QGCButton {
                text:            qsTr("★ 收藏")
                backgroundColor: root.favoritedOnly ? qgcPal.buttonHighlight : qgcPal.button
                textColor:       root.favoritedOnly ? qgcPal.buttonHighlightText : qgcPal.buttonText
                onClicked: {
                    root.favoritedOnly = true
                    root.refreshFromFirstPage()
                }
            }

            Item { Layout.fillWidth: true }

            QGCComboBox {
                id:                    orderCombo
                Layout.preferredWidth: ScreenTools.defaultFontPixelWidth * 12
                model:                 root._orderChoiceNames
                currentIndex:          0
                onActivated:           root.refreshFromFirstPage()
            }
        }

        // -----------------------------------------------------------------
        // 列表
        // -----------------------------------------------------------------
        Rectangle {
            Layout.fillWidth:  true
            Layout.fillHeight: true
            color:             qgcPal.window
            border.color:      qgcPal.groupBorder
            border.width:      1

            ColumnLayout {
                anchors.fill:    parent
                anchors.margins: 1
                spacing:         0

                // 全选条。列表空的时候不占地方
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: listView.count > 0 ? selectRow.implicitHeight + 2 : 0
                    visible:          listView.count > 0
                    color:            qgcPal.windowShade
                    clip:             true

                    RowLayout {
                        id:      selectRow
                        anchors.fill:        parent
                        anchors.leftMargin:  4
                        anchors.rightMargin: 4
                        spacing:             root._pad / 2

                        QGCCheckBox {
                            text:     qsTr("全选")
                            checked:  listView.count > 0 && root.selectedRows.length === listView.count
                            onClicked: {
                                root._setAllSelected(!(root.selectedRows.length === listView.count && listView.count > 0))
                                // 点击会命令式写 checked，把上面的绑定顶掉，这里装回去
                                checked = Qt.binding(function () {
                                    return listView.count > 0 && root.selectedRows.length === listView.count
                                })
                            }
                        }
                        Item { Layout.fillWidth: true }
                        QGCLabel {
                            text:           root.wayline ? (qsTr("共 ") + root.wayline.totalCount + qsTr(" 条")) : ""
                            font.pointSize: ScreenTools.defaultFontPointSize
                            color:          qgcPal.text
                            opacity:        0.7
                        }
                    }
                }

                QGCListView {
                    id:                listView
                    Layout.fillWidth:  true
                    Layout.fillHeight: true
                    // 没登录就把模型整个摘掉：上一次登录（甚至上一个会话）留下的
                    // 航线不能挂在屏幕上 —— 那些名字/缩略图看着跟真的一样
                    model:             (root.loggedIn && root.wayline) ? root.wayline.listModel : null
                    clip:              true
                    spacing:           0
                    currentIndex:      -1

                    // 行样式与「本地任务」共用 MissionListRow.qml。这一页只负责
                    // 把模型角色和这一页的动作喂进去
                    delegate: MissionListRow {
                        id: cloudRow

                        // 模型角色名就叫 favorited，而 MissionListRow 自己也有一个
                        // `favorited` 属性 —— QML 里不带限定的名字**先看对象自己的属性**，
                        // 后看委托上下文里的模型角色。所以 `favorited: favorited === true`
                        // 是自己绑自己：Qt 判成绑定环，值一直停在默认的 false，
                        // 星永远不亮，下面几处 `favorited === true` 也一律是 false
                        // （收藏按钮对已收藏的行会再去收藏一遍，而不是取消收藏）。
                        // 必须用 `model.` 限定拿角色值，再另存给下面几处共用
                        readonly property bool _fav: model.favorited === true

                        current:   ListView.isCurrentItem
                        expanded:  root.expandedIndex === index
                        title:     name
                        checked:   root._isSelected(index)
                        favorited: _fav
                        metaText:  root._metaText(updateTimeText, templateTypes)
                        hoverTip:  root._tooltipText({
                                       "name":     name,
                                       "drone":    droneModelKey,
                                       "payload":  payloadModelKeys,
                                       "template": templateTypes,
                                       "user":     userName,
                                       "time":     updateTimeText,
                                       "fav":      _fav
                                   })
                        actions: [
                            { "text": qsTr("下载并载入"), "icon": "/InstrumentValueIcons/inbox-download.svg" },
                            { "text": qsTr("重命名"),     "icon": "/InstrumentValueIcons/edit-pencil.svg" },
                            // 收藏/取消收藏用同一个星：图标是「收藏」这个状态，
                            // 提示文案才是当前会做什么（点了是收藏还是取消收藏）
                            { "text": _fav ? qsTr("取消收藏") : qsTr("收藏"),
                              "icon": "/InstrumentValueIcons/star-full.svg" },
                            { "text": qsTr("删除"),       "icon": "/res/TrashDelete.svg" }
                        ]

                        onActivated: {
                            root.expandedIndex = -1
                            root._selectWayline(index, {
                                "name":     name,
                                "drone":    droneModelKey,
                                "payload":  payloadModelKeys,
                                "template": templateTypes,
                                "user":     userName,
                                "time":     updateTimeText,
                                "id":       waylineId,
                                "fav":      _fav
                            })
                        }
                        onCheckToggled: {
                            root.expandedIndex = -1
                            root._toggleRow(index)
                        }
                        onMenuToggled: {
                            root.expandedIndex = (root.expandedIndex === index ? -1 : index)
                        }
                        onActionTriggered: (i) => {
                            root.expandedIndex = -1
                            root._rowAction(i, waylineId, name, _fav)
                        }
                    }

                    QGCLabel {
                        anchors.centerIn:    parent
                        width:               parent.width * 0.85
                        wrapMode:            Text.WordWrap
                        horizontalAlignment: Text.AlignHCenter
                        font.pointSize:      ScreenTools.defaultFontPointSize
                        color:               qgcPal.text
                        opacity:             0.75
                        visible:             listView.count === 0 && !(root.wayline && root.wayline.busy)
                        text:                root.loggedIn
                                             ? qsTr("没有航线。切到「本地任务」勾选后点「上传到云端」即可上传第一条。")
                                             : qsTr("未连接服务器。请先到「用户」页登录云平台账号，登录后这里会自动拉取云端航线。")
                    }
                }

                // ---- 页脚：翻页 ----
                Rectangle {
                    Layout.fillWidth:       true
                    Layout.preferredHeight: pagerRow.implicitHeight + 4
                    color:                  qgcPal.windowShade

                    RowLayout {
                        id:      pagerRow
                        anchors.fill:        parent
                        anchors.leftMargin:  4
                        anchors.rightMargin: 4
                        spacing:             root._pad / 2

                        QGCButton {
                            text:      qsTr("‹")
                            enabled:   root.loggedIn && root.wayline && root.wayline.currentPage > 1 && !root.wayline.busy
                            onClicked: root.refresh(root.wayline.currentPage - 1)
                        }
                        QGCLabel {
                            text: root.loggedIn && root.wayline
                                  ? (root.wayline.currentPage + " / " + root.wayline.totalPages)
                                  : "--"
                        }
                        QGCButton {
                            text:      qsTr("›")
                            enabled:   root.loggedIn && root.wayline && root.wayline.currentPage < root.wayline.totalPages && !root.wayline.busy
                            onClicked: root.refresh(root.wayline.currentPage + 1)
                        }

                        Item { Layout.fillWidth: true }

                        QGCButton {
                            text:      qsTr("刷新")
                            enabled:   root.loggedIn && root.wayline && !root.wayline.busy
                            onClicked: root.refreshFromFirstPage()
                        }
                    }
                }
            }
        }

        // -----------------------------------------------------------------
        // 批量操作条。有勾选才出现
        //
        // 只剩「已选 N 项」和一个清空勾选的 ✕：这里原来还有「收藏 / 取消收藏」
        // 两个按钮（一次能给多条加收藏），去掉了 —— 收藏是**单条**的语义，批量勾
        // 十几条一起收进收藏夹不是谁真会做的事，而且收藏夹满了还得再一条条取消。
        // 单条的收藏在每行的 ⋮ 图标条里（星）。批量动作里真正有用的是底部的
        // 「下载任务」，那个在动作栏上，不在这条里
        // -----------------------------------------------------------------
        // Rectangle {
        //     Layout.fillWidth:       true
        //     Layout.preferredHeight: root.selectedRows.length > 0 ? batchRow.implicitHeight + 6 : 0
        //     visible:                root.selectedRows.length > 0
        //     color:                  qgcPal.windowShadeDark
        //     radius:                 3
        //     clip:                   true

        //     RowLayout {
        //         id:              batchRow
        //         anchors.fill:    parent
        //         anchors.margins: 3
        //         spacing:         root._pad / 2

        //         QGCLabel {
        //             text:      qsTr("已选 ") + root.selectedRows.length + qsTr(" 项")
        //             font.bold: true
        //         }
        //         Item { Layout.fillWidth: true }
        //         QGCButton {
        //             text:      qsTr("✕")
        //             onClicked: root._clearSelection()
        //         }
        //     }
        // }

        // -----------------------------------------------------------------
        // 状态行
        // -----------------------------------------------------------------
        ColumnLayout {
            Layout.fillWidth: true
            spacing:          2

            RowLayout {
                Layout.fillWidth: true
                spacing:          root._pad / 2

                // 刻意**不用** djiBridgeServer.cloudWsConnected：那说的是 WebSocket
                // 推送通道（态势感知 / 直播用），跟航线走的 REST 是两条路。本页拉列表、
                // 上传、下载全走 REST，推送没连上照样能传 —— 挂个红的「云端未连接」，
                // 会让人以为上传坏了。实际就这么误导过：列表明明返回了 2 条，左下角
                // 却写着「云端未连接」。这里只报本页真正依赖的那条通道。
                //
                // 三种状态：没登录（连 token 都没有，谈不上接口）→ 未连接服务器；
                // 登录了但最近一次拉列表失败 → 接口未就绪；拉到了 → 正常
                QGCLabel {
                    text:           !root.loggedIn
                                    ? qsTr("○ 未连接服务器")
                                    : (root._listOk ? qsTr("● 航线接口正常") : qsTr("○ 航线接口未就绪"))
                    color:          (root.loggedIn && root._listOk) ? qgcPal.colorGreen : qgcPal.colorGrey
                    font.pointSize: ScreenTools.defaultFontPointSize
                }
                QGCLabel {
                    text:           qsTr("同步 ") + root.lastSyncText
                    // 没登录就没有「同步」可言，时间戳留着只会让人以为刚同步过
                    visible:        root.loggedIn
                    font.pointSize: ScreenTools.defaultFontPointSize
                    color:          qgcPal.text
                    opacity:        0.6
                }
                Item { Layout.fillWidth: true }

                // 传输进度。上传/下载各占一条，同时只跑一条时另一条不出现
                RowLayout {
                    spacing: root._pad / 2
                    visible: root.transferOperation !== ""

                    QGCLabel {
                        text:           root.transferOperation === "upload" ? qsTr("⬆ 上传") : qsTr("⬇ 下载")
                        font.pointSize: ScreenTools.defaultFontPointSize
                    }
                    Rectangle {
                        Layout.preferredWidth:  50
                        Layout.preferredHeight: 4
                        radius:                 2
                        color:                  qgcPal.windowShade

                        Rectangle {
                            width:  parent.width * Math.max(0, Math.min(100, root.transferPercent)) / 100
                            height: parent.height
                            radius: parent.radius
                            color:  qgcPal.colorBlue
                        }
                    }
                }
            }

            QGCLabel {
                Layout.fillWidth: true
                wrapMode:         Text.WordWrap
                font.pointSize:   ScreenTools.defaultFontPointSize
                // 完整信息 C++ 侧已 qWarning 出来，这里只求一眼能看懂
                color:            qgcPal.warningText
                text:             root.statusText
                visible:          root.statusText !== ""
            }
        }
    }

    // Menu 键展开当前行的 ⋮ 图标条。放在列表这一层而不是委托里：委托不抢焦点，
    // ↑/↓ 的列表导航才不会被打断（Qt.Key_Menu 没有别的用途）
    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_Menu && listView.currentItem) {
            listView.currentItem.toggleMenu()
            event.accepted = true
        }
    }

    // =====================================================================
    // 信号
    // =====================================================================

    Timer {
        id:       transferClearTimer
        interval: 4000
        onTriggered: root.transferOperation = ""
    }

    Connections {
        target: root.wayline

        function onErrorOccurred(message) {
            root.statusText = message
            root.transferOperation = ""
            transferClearTimer.stop()
        }

        function onListRefreshed(totalCount, currentPage) {
            root.statusText = ""
            root._listOk = true
            root.lastSyncText = Qt.formatTime(new Date(), "hh:mm:ss")
            // 列表换了一批行（上传后重拉、删完重拉都会到这儿），旧的行号和
            // 选中集/详情面板都对不上了，一并清掉
            root._clearSelection()
            listView.currentIndex = -1
            root.currentWayline = ({})
            root.preview        = ({})
            root.previewId      = ""
            root.previewFailed  = false
            root.previewBusy    = false
        }

        function onTransferProgress(operation, percent) {
            root.transferOperation = operation
            root.transferPercent = percent
            transferClearTimer.restart()
        }

        function onDownloadFinished(kmzPath) {
            root.transferOperation = ""
            transferClearTimer.stop()
            // 下载目录里的 kmz，名字就是航线名
            var base = kmzPath.substring(kmzPath.lastIndexOf("/") + 1)
            if (base.toLowerCase().endsWith(".kmz")) {
                base = base.substring(0, base.length - 4)
            }
            // 先取再复位：单条下载只用这一次，批量下载进来的本来就是 false
            var autoLoad = root._autoLoadOnDownload
            root._autoLoadOnDownload = false
            root.waylineDownloaded(kmzPath, base, autoLoad)
        }

        /// 整批下载的账在 C++ 那边结（见 DjiWaylineManager::_noteDownloadOutcome），
        /// 结完这里只往上转发。单条下载不会发这个信号
        function onBatchDownloadFinished(landed, failed) {
            root.downloadBatchFinished(landed, failed)
        }

        /// 选中那条航线的 kmz 解析好了。id 对不上就丢弃 —— 用户已经点到别的行了，
        /// 慢到的响应不该把面板和地图改回去
        function onWaylinePreviewReady(id, info) {
            if (id !== root.previewId) return
            root.previewBusy   = false
            root.preview       = info
            root.previewFailed = !(info && info.ok)
        }
    }

    // =====================================================================
    // 弹窗
    // =====================================================================

    /// 删除确认。云端 DELETE 不生效时会自己退化成「取消收藏」，见 deleteWayline
    Component {
        id: deleteDialog
        QGCPopupDialog {
            id: deleteDialogRoot
            property int    row: -1
            property string waylineName: ""
            title:  qsTr("删除航线")
            buttons: Dialog.NoButton

            ColumnLayout {
                spacing: ScreenTools.defaultFontPixelHeight / 2

                QGCLabel {
                    Layout.preferredWidth:  Math.max(mainWindow.width / 3, 240)
                    wrapMode:               Text.WordWrap
                    text:                   qsTr("确定要删除「") + deleteDialogRoot.waylineName + qsTr("」吗？")
                                            + "\n\n"
                                            + qsTr("云端不支持删除时，会退化为「取消收藏」—— 记录仍留在后台，只是不再显示。")
                }
                RowLayout {
                    Layout.alignment: Qt.AlignRight
                    spacing: ScreenTools.defaultFontPixelWidth / 2
                    QGCButton {
                        text:      qsTr("取消")
                        onClicked: close()
                    }
                    QGCButton {
                        text:      qsTr("删除")
                        primary:   true
                        onClicked: {
                            root.wayline.deleteWayline(deleteDialogRoot.row)
                            close()
                        }
                    }
                }
            }
        }
    }

    /// 批量删除确认（底部动作栏那个垃圾桶）。单条删除在每行的 ⋮ 里，各有各的弹窗
    Component {
        id: batchDeleteDialog
        QGCPopupDialog {
            id: batchDeleteRoot
            /// 要删的行号快照。**存快照**：弹窗开着的时候列表可能被后台刷新过，
            /// 闭包里现取的行号会指到别的航线上
            property var rows: []
            title:  qsTr("删除航线")
            buttons: Dialog.NoButton

            ColumnLayout {
                spacing: ScreenTools.defaultFontPixelHeight / 2

                QGCLabel {
                    Layout.preferredWidth:  Math.max(mainWindow.width / 3, 240)
                    wrapMode:               Text.WordWrap
                    text:                   qsTr("确定要删除选中的 ") + batchDeleteRoot.rows.length + qsTr(" 条航线吗？")
                                            + "\n\n"
                                            + qsTr("云端不支持删除时，会退化为「取消收藏」—— 记录仍留在后台，只是不再显示。")
                }
                RowLayout {
                    Layout.alignment: Qt.AlignRight
                    spacing: ScreenTools.defaultFontPixelWidth / 2
                    QGCButton {
                        text:      qsTr("取消")
                        onClicked: close()
                    }
                    QGCButton {
                        text:      qsTr("删除")
                        primary:   true
                        onClicked: {
                            root.wayline.deleteWaylines(batchDeleteRoot.rows)
                            root._clearSelection()
                            close()
                        }
                    }
                }
            }
        }
    }

    /// 重命名。云端不支持改名，走「下载-改名-重传-旧条目取消收藏」。
    Component {
        id: renameDialog
        QGCPopupDialog {
            id: renameDialogRoot
            property int    row: -1
            property string waylineName: ""
            title:  qsTr("重命名航线")
            buttons: Dialog.NoButton

            ColumnLayout {
                spacing: ScreenTools.defaultFontPixelHeight / 2

                QGCLabel {
                    Layout.preferredWidth:  Math.max(mainWindow.width / 3, 240)
                    wrapMode:               Text.WordWrap
                    text:                   qsTr("云端不允许重名，也不支持直接改名。确认后会下载原文件、用新名字重新上传，"
                                                + "并把旧的那条取消收藏 —— 旧记录仍留在后台。"
                                                + "新名字里的 _ . * ? 等字符云端不接受，会自动换成 -。")
                }
                QGCTextField {
                    id:               renameField
                    Layout.fillWidth: true
                    text:             renameDialogRoot.waylineName
                }
                RowLayout {
                    Layout.alignment: Qt.AlignRight
                    spacing: ScreenTools.defaultFontPixelWidth / 2
                    QGCButton {
                        text:      qsTr("取消")
                        onClicked: close()
                    }
                    QGCButton {
                        text:      qsTr("重命名")
                        primary:   true
                        enabled:   renameField.text.length > 0
                        onClicked: {
                            root.wayline.renameWayline(renameDialogRoot.row, renameField.text)
                            close()
                        }
                    }
                }
            }
        }
    }

    // 刻意不在 Component.onCompleted 里自动拉列表。
    // PlanView 是 MainRootWindow 的 StackLayout 直接子项，启动就会实例化，
    // 那时云端多半还没连上 —— 自动拉会走 _fail -> errorOccurred ->
    // 变成本页第一眼看到的就是「获取航线列表失败」。
    // 改由 PlanView 在切到本页时调用 refresh()。
}
