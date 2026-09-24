import QtQuick 2.12
import Qt.labs.folderlistmodel 2.2
import QtQuick.Layouts 1.3
import QtQuick.Controls 2.12
// import QtGraphicalEffects 1.12
// import QGroundControl              1.0
import QGroundControl.Controls     1.0
import QGroundControl.ScreenTools  1.0


QGCListView {
    id: listView

    focus: true
    model: folderModel
    delegate: fileDelegate
    property int toolTipDelay: 500
    property var directory
    property string _missionFolder: _appSettings.missionSavePath + "/" + directory

    /// 哪一行的图标动作条是展开的（-1 = 都没展开）。**由这一层持有**：
    /// 「同一时刻只开一行」是列表的性质，行自己不知道别人开没开。
    /// 任意一行的 ⋮ 按下就把它切到那一行，于是别行自然收回
    property int expandedIndex: -1

    property color _splitLineColor: "#707070" // "#424242"
    property color _splitLineColorLighter: "#424242" // "#424242"
    FolderListModel {
        id: folderModel
        folder: "file:" + _appSettings.missionSavePath + "/" + directory
        nameFilters: ["*.plan"]
        sortField: FolderListModel.Time
    }

    // ---------------------------------------------------------------------
    // 多选
    //
    // 用**完整路径**而不是行下标做标识：改名会让文件名变，删除会让下标错位，
    // 路径只在这两种情况下才失效，而且失效了下面 checkedMissions() 会把它们
    // 过滤掉（按当前目录里实际存在的文件重新对一遍）。
    // 上传到云端、批量删除都走这里。
    // ---------------------------------------------------------------------

    property var checkedFiles: []

    function isChecked(path) {
        return checkedFiles.indexOf(path) >= 0
    }

    function toggleChecked(path) {
        var list = checkedFiles.slice()
        var i = list.indexOf(path)
        if (i >= 0) {
            list.splice(i, 1)
        } else {
            list.push(path)
        }
        checkedFiles = list
    }

    function clearChecked() {
        checkedFiles = []
    }

    function _pathAt(index) {
        return _missionFolder + "/" + folderModel.get(index, "fileName")
    }

    /// 勾选的、且当前目录里确实存在的任务，形如 [{name, path}, ...]。
    /// 返回新数组，可以直接喂给上传弹窗。
    function checkedMissions() {
        var out = []
        for (var i = 0; i < folderModel.count; ++i) {
            var path = _pathAt(i)
            if (checkedFiles.indexOf(path) >= 0) {
                out.push({ "name": folderModel.get(i, "fileBaseName"), "path": path })
            }
        }
        return out
    }

    /// 当前选中的那一条，没选返回 null
    function currentMission() {
        if (currentIndex < 0 || currentIndex >= folderModel.count) return null
        return { "name": folderModel.get(currentIndex, "fileBaseName"), "path": _pathAt(currentIndex) }
    }

    /// 要上传/要操作的目标：有勾选就用勾选的，没勾选就用当前选中的那一条
    function selectedMissions() {
        var checked = checkedMissions()
        if (checked.length > 0) return checked
        var current = currentMission()
        return current ? [current] : []
    }

    /// 只勾选这一条（其余取消）。点行的语义就是这个 —— 云端航线库点一下也是
    /// 「勾上它、别的行全取消」（那边是 selectedRows = [row]），两页是同一张列表，
    /// 行为也得一样。勾选是「要拿这条做什么」的入口（底部那排图标、上传对话框都
    /// 按勾选来），只点一下不勾上，用户会以为点了没反应
    function setOnlyChecked(index) {
        if (index >= 0 && index < folderModel.count) {
            checkedFiles = [_pathAt(index)]
        }
    }

    function setAllChecked(all) {
        if (!all) {
            clearChecked()
            return
        }
        var list = []
        for (var i = 0; i < folderModel.count; ++i) {
            list.push(_pathAt(i))
        }
        checkedFiles = list
    }

    // 行样式与「云端航线库」共用 MissionListRow.qml —— 两页的列表必须是同一张脸。
    // 这里只负责把 FolderListModel 的角色和本页的动作喂进去。
    Component {
        id: fileDelegate
        MissionListRow {
            id: localRow

            current:      ListView.isCurrentItem
            expanded:     listView.expandedIndex === index
            title:        fileBaseName
            nameEditable: true                       // 本地任务的名字可以就地改
            checked:      listView.isChecked(listView._pathAt(index))
            metaText:     qsTr("修改时间：") + (fileModified instanceof Date
                                                ? Qt.formatDateTime(fileModified, "yyyy-MM-dd hh:mm") : "--.--")
            actions: [
                { "text": qsTr("载入并编辑"), "icon": "/InstrumentValueIcons/window-open.svg" },
                { "text": qsTr("重命名"),     "icon": "/InstrumentValueIcons/edit-pencil.svg" },
                { "text": qsTr("删除"),       "icon": "/res/TrashDelete.svg" }
            ]

            /// 选中这一条并载入编辑器。点行本身、点「载入并编辑」、点「重命名」
            /// 都要先走它 —— 三个入口一套动作，别各写一遍
            function _loadThisRow() {
                listView.currentIndex = index
                listView.focus = true
                _currentPlanFileName = folderModel.get(index, "fileBaseName")
                // loadFromFile 要**带**扩展名
                _planMasterController.loadFromFile(_missionFolder + "/" + folderModel.get(index, "fileName"))
                _planMasterController.fitViewportToItems()
            }

            function _deleteThisRow() {
                // removeSelectedFiles 自己会补上 ".plan"（PlanMasterController.cc 里
                // QFile::remove(fileName + "." + fileExtension())），所以这里**不能**带扩展名
                var base = _missionFolder + "/" + folderModel.get(index, "fileBaseName")
                if (listView.currentIndex === index) {
                    _planMasterController.removeAll()
                    _planMasterController.removeSelectedFiles(base)
                    listView.currentIndex = -1            // 不选任何任务文件
                    _currentPlanFileName = ""
                } else {
                    _planMasterController.removeSelectedFiles(base)
                    _currentPlanFileName = ""             // 与改动前一致：删非当前行也会清掉
                }
            }

            onActivated: {
                listView.expandedIndex = -1
                _loadThisRow()
                // 点行顺带勾上它 —— 见 setOnlyChecked 的注释
                listView.setOnlyChecked(index)
            }
            onCheckToggled: {
                listView.expandedIndex = -1
                listView.toggleChecked(listView._pathAt(index))
            }
            // ⋮ 的开关。点的是本行就收起，点的是别行就换过去（只留一行开着）
            onMenuToggled: {
                listView.expandedIndex = (listView.expandedIndex === index ? -1 : index)
            }

            onNameEdited: (text) => {
                if (_currentPlanFileName !== text && text !== "") {
                    var previousPlanFileName = _currentPlanFileName
                    _currentPlanFileName = text
                    if (!_planMasterController.renameCurrentFile(text)) {
                        _currentPlanFileName = previousPlanFileName
                        localRow.resetNameText(previousPlanFileName)
                    }
                } else if (text === "") {
                    // 改动前这里写的是 fileName（带 .plan 的全名），清空名字框会把
                    // 「xxx.plan」显示回去；名字框里本来就该是 baseName
                    localRow.resetNameText(fileBaseName)
                }
            }

            onActionTriggered: (i) => {
                listView.expandedIndex = -1      // 选完就收起，别把图标条留在屏幕上
                if (i === 0) {
                    _loadThisRow()
                    enterPlanEditMode(true)
                } else if (i === 1) {
                    // 必须先载入才能改名：renameCurrentFile 改的是 _currentPlanFile
                    _loadThisRow()
                    localRow.focusNameField()
                } else if (i === 2) {
                    _deleteThisRow()
                }
            }
        }
    }

    // Menu 键展开当前行的 ⋮ 图标条。放在列表这一层而不是委托里：委托不抢焦点，
    // ↑/↓ 的列表导航才不会被打断（Qt.Key_Menu 没有别的用途）
    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_Menu && currentItem) {
            currentItem.toggleMenu()
            event.accepted = true
        }
    }

    Component.onCompleted: {
        listView.currentIndex = -1
    }
}
