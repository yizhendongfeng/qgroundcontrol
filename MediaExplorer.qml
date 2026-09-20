import QtQuick 2.4
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QGroundControl.FileItemModel 1.0
import QGroundControl.Controls
import QGroundControl.ScreenTools
import QGroundControl.Palette
import Qt5Compat.GraphicalEffects

Item {
    property var currentMediaFolder: ""
    QGCPalette { id: qgcPal; colorGroupEnabled: enabled }
    FileItemModel {
        id: fileModel
    }

    FolderDialog {
        id: folderDialog
        title: "选择文件夹"
        currentFolder: fileModel.mediaRootFolder
        onAccepted: {
            console.log(selectedFolder)
            currentMediaFolder = selectedFolder
            fileModel.changeFolder(selectedFolder);

        }
    }


    QGCTabBar {
        id:         mediaTabBar
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.margins: 5
        width: 400
        Component.onCompleted: currentIndex = 0
        QGCTabButton {
            text:       qsTr("本地文件")
        }
        QGCTabButton {
            text:       qsTr("吊舱文件")
        }
    }

    Rectangle {
        id: lineSpliter
        anchors.top: mediaTabBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: 5
        anchors.bottomMargin: 5
        height: 1
        color: qgcPal.groupBorder
    }
    StackLayout {
        id: stackLayoutMedia
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: lineSpliter.bottom
        anchors.bottom: parent.bottom
        anchors.margins: 10
        currentIndex: mediaTabBar.currentIndex
        Item {
            id: localTabItem
            anchors.fill: parent

            Component.onCompleted: fileModel.refreshCurrentFolder()

            ListModel { id: localDateGroups }

            function extractDate(fp) {
                var parts = fp.split("/")
                for (var j = 0; j < parts.length; j++) {
                    if (/^\d{4}-\d{2}-\d{2}$/.test(parts[j])) return parts[j]
                }
                var nm = parts[parts.length - 1]
                var mm = nm.match(/^(\d{4})(\d{2})(\d{2})/)
                if (mm) return mm[1] + "-" + mm[2] + "-" + mm[3]
                return "其他"
            }

            function rebuildLocalGroups() {
                localDateGroups.clear()
                var groups = {}
                var order = []
                for (var i = 0; i < fileModel.count; i++) {
                    var f = fileModel.get(i)
                    var date = extractDate(f.filePath)
                    if (!groups[date]) {
                        groups[date] = Qt.createQmlObject('import QtQuick 2.15; ListModel {}', localDateGroups)
                        order.push(date)
                    }
                    groups[date].append({
                        filePath: f.filePath, thumbReady: f.thumbReady,
                        thumbnailUrl: f.thumbnailUrl, isVideo: f.isVideo,
                        selected: f.selected, rowIndex: i,
                        fileSizeStr: f.fileSizeStr
                    })
                }
                order.sort().reverse()
                for (var k = 0; k < order.length; k++) {
                    localDateGroups.append({ date: order[k], files: groups[order[k]] })
                }
                // 同步日期筛选下拉框
                var prevFilter = dateCombo.currentText
                dateCombo.model = ["全部"].concat(order)
                if (prevFilter && dateCombo.model.indexOf(prevFilter) >= 0) {
                    dateCombo.currentIndex = dateCombo.model.indexOf(prevFilter)
                } else {
                    dateCombo.currentIndex = 0
                }
            }

            Connections {
                target: fileModel
                function onCountChanged() { localTabItem.rebuildLocalGroups() }
                function onDataChanged(topLeft, bottomRight) {
                    for (var g = 0; g < localDateGroups.count; g++) {
                        var lm = localDateGroups.get(g).files
                        for (var r = 0; r < lm.count; r++) {
                            var ri = lm.get(r).rowIndex
                            if (ri >= topLeft.row && ri <= bottomRight.row) {
                                var ff = fileModel.get(ri)
                                lm.setProperty(r, "thumbReady", ff.thumbReady)
                                lm.setProperty(r, "thumbnailUrl", ff.thumbnailUrl)
                                lm.setProperty(r, "selected", ff.selected)
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                anchors.fill: parent
                spacing: 8
                anchors.margins: 8

                Row {
                    id: rowRoot
                    spacing: 8
                    // 替换为你主目录（按你实际存放年/月/日 的根目录）
                    QGCLabel {
                        text: fileModel.mediaRootFolder
                        anchors.verticalCenter: parent.verticalCenter
                        font.pointSize:     ScreenTools.mediumFontPointSize
                    }

                    QGCButton {
                        text: "选择目录"
                        onClicked: {
                            folderDialog.open()
                        }
                    }

                    // 日期筛选下拉框
                    ComboBox {
                        id: dateCombo
                        anchors.verticalCenter: parent.verticalCenter
                        width: 150
                        font.pointSize: ScreenTools.mediumFontPointSize
                        model: ["全部"]
                        onActivated: { /* QML 层过滤，无需重新扫描 */ }
                    }

                    QGCButton {
                        text: "刷新"
                        onClicked: {
                            fileModel.refreshCurrentFolder()
                        }
                    }
                    QGCButton {
                        text: "上传"
                        onClicked: fileModel.uploadFilesToMinio()
                    }

                }

                Flickable {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    contentHeight: colGroups.height
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds

                    Column {
                        id: colGroups
                        width: parent.width
                        spacing: 8

                        Repeater {
                            model: localDateGroups
                            delegate: Column {
                                width: colGroups.width
                                spacing: 4
                                visible: dateCombo.currentText === "全部" || model.date === dateCombo.currentText
                                property var groupFiles: model.files

                                // 日期标题
                                Rectangle {
                                    width: parent.width
                                    height: 30
                                    color: qgcPal.window
                                    QGCLabel {
                                        anchors.left: parent.left
                                        anchors.leftMargin: 8
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: model.date
                                        font.bold: true
                                        font.pointSize: ScreenTools.mediumFontPointSize
                                        color: qgcPal.buttonText
                                    }
                                }

                                // 文件网格
                                Flow {
                                    width: colGroups.width
                                    spacing: 10
                                    Repeater {
                                        model: groupFiles
                                        delegate: Rectangle {
                                            width: 190
                                            height: 200
                                            radius: 6
                                            border.color: selected ? qgcPal.buttonText : "transparent"
                                            color: selected ? "#505050" : "transparent"
                                            Column {
                                                anchors.centerIn: parent
                                                spacing: 2
                                                Image {
                                                    width: 180
                                                    height: width / 1.77
                                                    fillMode: Image.PreserveAspectFit
                                                    visible: thumbReady
                                                    source: thumbnailUrl
                                                    QGCColoredImage {
                                                        anchors.centerIn: parent
                                                        width: 48
                                                        height: width
                                                        visible: isVideo
                                                        source: "qrc:/InstrumentValueIcons/play.svg"
                                                        color: qgcPal.buttonText
                                                        opacity: 0.9
                                                    }
                                                }
                                                Rectangle {
                                                    width: 180
                                                    height: 180
                                                    visible: !thumbReady
                                                    color: "#e0e0e0"
                                                    radius: 4
                                                    BusyIndicator { anchors.centerIn: parent; running: true }
                                                }
                                                QGCLabel {
                                                    text: filePath.split("/").pop()
                                                    width: 180
                                                    wrapMode: Text.WrapAnywhere
                                                    horizontalAlignment: Text.AlignHCenter
                                                    color: qgcPal.buttonText
                                                }
                                                QGCLabel {
                                                    text: fileSizeStr
                                                    width: 180
                                                    horizontalAlignment: Text.AlignHCenter
                                                    color: qgcPal.buttonText
                                                    opacity: 0.7
                                                }
                                            }
                                            MouseArea {
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                onClicked: (mouse) => fileModel.clickSelect(rowIndex, mouse.modifiers)
                                                onDoubleClicked: Qt.openUrlExternally(Qt.url("file:///" + filePath))
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // ============================================================
        // 吊舱文件 Tab
        // ============================================================
        Item {
            id: podTabItem
            anchors.fill: parent

            ListModel { id: podDateGroups }

            function rebuildPodGroups() {
                podDateGroups.clear()
                var groups = {}
                var order = []
                var dm = fileModel.mediaDownload
                for (var i = 0; i < dm.count; i++) {
                    var f = dm.get(i)
                    var mt = f.filePathStr.match(/\/download\/(\d{4}-\d{2}-\d{2})\//)
                    var date = mt ? mt[1] : "其他"
                    if (!groups[date]) {
                        groups[date] = Qt.createQmlObject('import QtQuick 2.15; ListModel {}', podDateGroups)
                        order.push(date)
                    }
                    groups[date].append({
                        fileNameStr: f.fileNameStr, filePathStr: f.filePathStr,
                        thumbReady: f.thumbReady, thumbnailUrl: f.thumbnailUrl,
                        isVideo: f.isVideo, fileSelected: f.fileSelected,
                        downloadedProgress: f.downloadedProgress, fileSizeStr: f.fileSizeStr,
                        rowIndex: i
                    })
                }
                order.sort().reverse()
                for (var k = 0; k < order.length; k++) {
                    podDateGroups.append({ date: order[k], files: groups[order[k]] })
                }
            }

            Connections {
                target: fileModel.mediaDownload
                function onCountChanged() { podTabItem.rebuildPodGroups() }
                function onPodListLoaded() { podTabItem.rebuildPodGroups() }
                function onDataChanged(topLeft, bottomRight) {
                    var dm = fileModel.mediaDownload
                    for (var g = 0; g < podDateGroups.count; g++) {
                        var lm = podDateGroups.get(g).files
                        for (var r = 0; r < lm.count; r++) {
                            var ri = lm.get(r).rowIndex
                            if (ri >= topLeft.row && ri <= bottomRight.row) {
                                var f = dm.get(ri)
                                lm.setProperty(r, "thumbReady", f.thumbReady)
                                lm.setProperty(r, "thumbnailUrl", f.thumbnailUrl)
                                lm.setProperty(r, "fileSelected", f.fileSelected)
                                lm.setProperty(r, "downloadedProgress", f.downloadedProgress)
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                anchors.fill: parent
                spacing: 6

                // 顶部工具栏
                Row {
                    id: rowQuery
                    spacing: 6
                    height: 40
                    Layout.preferredHeight: 40

                    QGCLabel {
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("日期范围:")
                        font.pointSize: ScreenTools.mediumFontPointSize
                    }

                    // 开始日期（点击弹日历）
                    Rectangle {
                        id: startDateBox
                        anchors.verticalCenter: parent.verticalCenter
                        width: 130
                        height: 28
                        color: qgcPal.window
                        border.width: 1
                        border.color: qgcPal.groupBorder
                        radius: 3
                        QGCLabel {
                            id: startDateLabel
                            anchors.fill: parent
                            verticalAlignment: Text.AlignVCenter
                            horizontalAlignment: Text.AlignHCenter
                            text: ""
                            font.pointSize: ScreenTools.mediumFontPointSize
                            Component.onCompleted: {
                                var d = new Date()
                                var m = ("0" + (d.getMonth() + 1)).slice(-2)
                                var dd = ("0" + d.getDate()).slice(-2)
                                text = d.getFullYear() + "-" + m + "-" + dd
                            }
                        }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: startCalPopup.open()
                        }
                    }

                    QGCLabel {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "--"
                    }

                    // 结束日期
                    Rectangle {
                        id: endDateBox
                        anchors.verticalCenter: parent.verticalCenter
                        width: 130
                        height: 28
                        color: qgcPal.window
                        border.width: 1
                        border.color: qgcPal.groupBorder
                        radius: 3
                        QGCLabel {
                            id: endDateLabel
                            anchors.fill: parent
                            verticalAlignment: Text.AlignVCenter
                            horizontalAlignment: Text.AlignHCenter
                            text: ""
                            font.pointSize: ScreenTools.mediumFontPointSize
                            Component.onCompleted: {
                                var d = new Date()
                                var m = ("0" + (d.getMonth() + 1)).slice(-2)
                                var dd = ("0" + d.getDate()).slice(-2)
                                text = d.getFullYear() + "-" + m + "-" + dd
                            }
                        }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: endCalPopup.open()
                        }
                    }

                    QGCButton {
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("查询")
                        onClicked: {
                            if (startDateLabel.text > endDateLabel.text) {
                                mainWindow.showMessageDialog(qsTr("日期错误"),
                                    qsTr("开始日期不能晚于结束日期，请重新选择"), Dialog.Ok)
                                return
                            }
                            fileModel.mediaDownload.refreshMediaInPod(startDateLabel.text, endDateLabel.text)
                        }
                    }

                    // 全选
                    QGCCheckBox {
                        id: selectAllChk
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("全选")
                        onCheckedChanged: fileModel.mediaDownload.selectAll(checked)
                    }

                    QGCButton {
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("下载选中")
                        onClicked: fileModel.mediaDownload.startDownloadFiles()
                    }
                }

                // 吊舱文件缩略图网格（按日期分组）
                Flickable {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    contentHeight: podColGroups.height
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds

                    Column {
                        id: podColGroups
                        width: parent.width
                        spacing: 8

                        Repeater {
                            model: podDateGroups
                            delegate: Column {
                                id: dateGroupCol
                                width: podColGroups.width
                                spacing: 4
                                property var groupFiles: model.files
                                Rectangle {
                                    width: parent.width
                                    height: 30
                                    color: qgcPal.window
                                    QGCLabel {
                                        anchors.left: parent.left
                                        anchors.leftMargin: 8
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: model.date
                                        font.bold: true
                                        font.pointSize: ScreenTools.mediumFontPointSize
                                        color: qgcPal.buttonText
                                    }
                                }

                                Column {
                                    width: podColGroups.width
                                    spacing: 0
                                    Repeater {
                                        model: dateGroupCol.groupFiles
                                        delegate: Rectangle {
                                            width: podColGroups.width
                                            height: 36
                                            color: fileSelected ? "#505050" : (podMouse.containsMouse ? "#404040" : "transparent")

                                            GridLayout {
                                                anchors.left: parent.left
                                                anchors.top:   parent.top
                                                anchors.bottom: parent.bottom
                                                width:      600
                                                anchors.leftMargin: 8
                                                anchors.rightMargin: 8
                                                columns: 5
                                                columnSpacing: 8
                                                rowSpacing: 0
                                                // anchors.verticalCenter: parent.verticalCenter

                                                // 复选框（列宽16）
                                                Rectangle {
                                                    width: 16
                                                    height: 16
                                                    border.width: 1
                                                    border.color: qgcPal.buttonText
                                                    color: fileSelected ? qgcPal.buttonText : "transparent"
                                                    Layout.alignment: Qt.AlignVCenter
                                                }

                                                // 图标（列宽20）
                                                QGCColoredImage {
                                                    width: 20
                                                    height: 20
                                                    source: isVideo ? "qrc:/InstrumentValueIcons/play.svg" : "qrc:/InstrumentValueIcons/camera.svg"
                                                    color: qgcPal.buttonText
                                                    Layout.alignment: Qt.AlignVCenter
                                                }

                                                // 文件名（弹性列）
                                                QGCLabel {
                                                    text: fileNameStr
                                                    color: qgcPal.buttonText
                                                    font.pointSize: ScreenTools.mediumFontPointSize
                                                    Layout.preferredWidth: 350
                                                    Layout.alignment: Qt.AlignVCenter
                                                    elide: Text.ElideRight
                                                }

                                                // 下载状态（列宽80，右对齐）
                                                QGCLabel {
                                                    Layout.preferredWidth: 80
                                                    text: downloadedProgress > 0.99 ? qsTr("已下载") : (downloadedProgress > 0.01 ? qsTr("下载中 %1%").arg(Math.round(downloadedProgress*100)) : "")
                                                    color: downloadedProgress > 0.99 ? "#00aa00" : qgcPal.buttonText
                                                    opacity: 0.8
                                                    font.pointSize: ScreenTools.smallFontPointSize
                                                    Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                                                }

                                                // 文件大小（列宽80，右对齐）
                                                QGCLabel {
                                                    Layout.preferredWidth: 80
                                                    text: fileSizeStr
                                                    color: qgcPal.buttonText
                                                    opacity: 0.7
                                                    font.pointSize: ScreenTools.smallFontPointSize
                                                    Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                                                }
                                            }

                                            // 下载进度条
                                            Rectangle {
                                                anchors.bottom: parent.bottom
                                                anchors.left: parent.left
                                                anchors.right: parent.right
                                                height: 2
                                                visible: downloadedProgress > 0 && downloadedProgress < 1
                                                color: "#303030"
                                                Rectangle {
                                                    width: parent.width * downloadedProgress
                                                    height: parent.height
                                                    color: qgcPal.buttonText
                                                }
                                            }

                                            MouseArea {
                                                id: podMouse
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                acceptedButtons: Qt.LeftButton | Qt.RightButton
                                                onClicked: {
                                                    fileModel.mediaDownload.toggleSelection(rowIndex, mouse.modifiers)
                                                }
                                                onDoubleClicked: {
                                                    fileModel.mediaDownload.podOpenFile(rowIndex)
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // 日历 Popup：开始日期
            Popup {
                id: startCalPopup
                x: startDateBox.x
                y: startDateBox.height
                width: 320
                implicitHeight: 340
                closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
                modal: true
                padding: 10

                property var curDate: new Date()
                property string maxDate: endDateLabel.text   // 开始日期不能晚于结束日期

                background: Rectangle {
                    color: qgcPal.window
                    border.color: qgcPal.groupBorder
                    radius: 2
                }

                ListModel {
                    id: startCalModel
                }

                function rebuild() {
                    startCalModel.clear()
                    var d = startCalPopup.curDate
                    var y = d.getFullYear()
                    var m = d.getMonth()
                    var firstDay = new Date(y, m, 1).getDay()
                    var offset = (firstDay + 6) % 7
                    var daysInMonth = new Date(y, m + 1, 0).getDate()
                    var now = new Date()
                    for (var i = 0; i < 42; i++) {
                        var day = i - offset + 1
                        if (day >= 1 && day <= daysInMonth) {
                            var isToday = (y === now.getFullYear() && m === now.getMonth() && day === now.getDate())
                            var fullStr = y + "-" + ("0" + (m + 1)).slice(-2) + "-" + ("0" + day).slice(-2)
                            var outOfRange = (maxDate !== "" && fullStr > maxDate)
                            startCalModel.append({ "dayNum": day, "present": true, "today": isToday, "outOfRange": outOfRange })
                        } else {
                            startCalModel.append({ "dayNum": 0, "present": false, "today": false, "outOfRange": false })
                        }
                    }
                }

                onAboutToShow: rebuild()

                Column {
                    anchors.fill: parent
                    spacing: 6

                    Row {
                        width: parent.width
                        spacing: 8
                        QGCToolBarButton {
                            text: "‹"
                            padding: 6
                            font.pointSize: ScreenTools.largeFontPointSize
                            onClicked: {
                                var d = startCalPopup.curDate
                                startCalPopup.curDate = new Date(d.getFullYear(), d.getMonth() - 1, 1)
                                startCalPopup.rebuild()
                            }
                        }
                        QGCLabel {
                            text: (startCalPopup.curDate.getFullYear()) + "-" +
                                  ("0" + (startCalPopup.curDate.getMonth() + 1)).slice(-2)
                            anchors.verticalCenter: parent.verticalCenter
                            horizontalAlignment: Text.AlignHCenter
                            width: 150
                            font.pointSize: ScreenTools.largeFontPointSize
                            color: qgcPal.buttonText
                        }
                        QGCToolBarButton {
                            text: "›"
                            padding: 6
                            font.pointSize: ScreenTools.largeFontPointSize
                            onClicked: {
                                var d = startCalPopup.curDate
                                startCalPopup.curDate = new Date(d.getFullYear(), d.getMonth() + 1, 1)
                                startCalPopup.rebuild()
                            }
                        }
                    }

                    Row {
                        width: parent.width
                        spacing: 2
                        Repeater {
                            model: ["一","二","三","四","五","六","日"]
                            Item {
                                width: (startCalPopup.width - 20 - 12*2) / 7
                                height: 28
                                QGCLabel {
                                    anchors.centerIn: parent
                                    text: modelData
                                    font.pointSize: ScreenTools.mediumFontPointSize
                                    color: qgcPal.buttonText
                                    opacity: 0.7
                                }
                            }
                        }
                    }

                    GridView {
                        id: startCalGrid
                        width: parent.width
                        height: 220
                        cellWidth: (startCalPopup.width - 20 - 12*2) / 7
                        cellHeight: 38
                        interactive: false
                        model: startCalModel

                        delegate: Item {
                            width: startCalGrid.cellWidth
                            height: startCalGrid.cellHeight
                            Rectangle {
                                anchors.centerIn: parent
                                width: 32
                                height: 32
                                radius: 16
                                color: !present ? "transparent" :
                                       outOfRange ? "#20808080" :
                                       today ? qgcPal.buttonHighlight :
                                       pm.containsMouse ? "#40808080" : "transparent"
                                MouseArea {
                                    id: pm
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    enabled: present && !outOfRange
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        var y = startCalPopup.curDate.getFullYear()
                                        var m = startCalPopup.curDate.getMonth()
                                        startDateLabel.text = y + "-" +
                                            ("0" + (m + 1)).slice(-2) + "-" +
                                            ("0" + dayNum).slice(-2)
                                        startCalPopup.close()
                                    }
                                    QGCLabel {
                                        anchors.centerIn: parent
                                        text: dayNum
                                        visible: present
                                        font.pointSize: ScreenTools.mediumFontPointSize
                                        color: outOfRange ? qgcPal.buttonText
                                                : qgcPal.buttonText
                                        opacity: outOfRange ? 0.35 : 1.0
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // 日历 Popup：结束日期
            Popup {
                id: endCalPopup
                x: endDateBox.x
                y: endDateBox.height
                width: 320
                implicitHeight: 340
                closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
                modal: true
                padding: 10

                property var curDate: new Date()
                property string minDate: startDateLabel.text   // 结束日期不能早于开始日期

                background: Rectangle {
                    color: qgcPal.window
                    border.color: qgcPal.groupBorder
                    radius: 2
                }

                ListModel {
                    id: endCalModel
                }

                function rebuild() {
                    endCalModel.clear()
                    var d = endCalPopup.curDate
                    var y = d.getFullYear()
                    var m = d.getMonth()
                    var firstDay = new Date(y, m, 1).getDay()
                    var offset = (firstDay + 6) % 7
                    var daysInMonth = new Date(y, m + 1, 0).getDate()
                    var now = new Date()
                    for (var i = 0; i < 42; i++) {
                        var day = i - offset + 1
                        if (day >= 1 && day <= daysInMonth) {
                            var isToday = (y === now.getFullYear() && m === now.getMonth() && day === now.getDate())
                            var fullStr = y + "-" + ("0" + (m + 1)).slice(-2) + "-" + ("0" + day).slice(-2)
                            var outOfRange = (minDate !== "" && fullStr < minDate)
                            endCalModel.append({ "dayNum": day, "present": true, "today": isToday, "outOfRange": outOfRange })
                        } else {
                            endCalModel.append({ "dayNum": 0, "present": false, "today": false, "outOfRange": false })
                        }
                    }
                }

                onAboutToShow: rebuild()

                Column {
                    anchors.fill: parent
                    spacing: 6

                    Row {
                        width: parent.width
                        spacing: 8
                        QGCToolBarButton {
                            text: "‹"
                            padding: 6
                            font.pointSize: ScreenTools.largeFontPointSize
                            onClicked: {
                                var d = endCalPopup.curDate
                                endCalPopup.curDate = new Date(d.getFullYear(), d.getMonth() - 1, 1)
                                endCalPopup.rebuild()
                            }
                        }
                        QGCLabel {
                            text: (endCalPopup.curDate.getFullYear()) + "-" +
                                  ("0" + (endCalPopup.curDate.getMonth() + 1)).slice(-2)
                            anchors.verticalCenter: parent.verticalCenter
                            horizontalAlignment: Text.AlignHCenter
                            width: 150
                            font.pointSize: ScreenTools.largeFontPointSize
                            color: qgcPal.buttonText
                        }
                        QGCToolBarButton {
                            text: "›"
                            padding: 6
                            font.pointSize: ScreenTools.largeFontPointSize
                            onClicked: {
                                var d = endCalPopup.curDate
                                endCalPopup.curDate = new Date(d.getFullYear(), d.getMonth() + 1, 1)
                                endCalPopup.rebuild()
                            }
                        }
                    }

                    Row {
                        width: parent.width
                        spacing: 2
                        Repeater {
                            model: ["一","二","三","四","五","六","日"]
                            Item {
                                width: (endCalPopup.width - 20 - 12*2) / 7
                                height: 28
                                QGCLabel {
                                    anchors.centerIn: parent
                                    text: modelData
                                    font.pointSize: ScreenTools.mediumFontPointSize
                                    color: qgcPal.buttonText
                                    opacity: 0.7
                                }
                            }
                        }
                    }

                    GridView {
                        id: endCalGrid
                        width: parent.width
                        height: 220
                        cellWidth: (endCalPopup.width - 20 - 12*2) / 7
                        cellHeight: 38
                        interactive: false
                        model: endCalModel

                        delegate: Item {
                            width: endCalGrid.cellWidth
                            height: endCalGrid.cellHeight
                            Rectangle {
                                anchors.centerIn: parent
                                width: 32
                                height: 32
                                radius: 16
                                color: !present ? "transparent" :
                                       outOfRange ? "#20808080" :
                                       today ? qgcPal.buttonHighlight :
                                       pm2.containsMouse ? "#40808080" : "transparent"
                                MouseArea {
                                    id: pm2
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    enabled: present && !outOfRange
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        var y = endCalPopup.curDate.getFullYear()
                                        var m = endCalPopup.curDate.getMonth()
                                        endDateLabel.text = y + "-" +
                                            ("0" + (m + 1)).slice(-2) + "-" +
                                            ("0" + dayNum).slice(-2)
                                        endCalPopup.close()
                                    }
                                    QGCLabel {
                                        anchors.centerIn: parent
                                        text: dayNum
                                        visible: present
                                        font.pointSize: ScreenTools.mediumFontPointSize
                                        color: qgcPal.buttonText
                                        opacity: outOfRange ? 0.35 : 1.0
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
