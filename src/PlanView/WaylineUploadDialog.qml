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

/// 上传本地任务到 DJI 云。
///
/// 用法是「先选后传」：在「本地任务」列表里勾选（不勾就传当前选中的那一条），
/// 点「上传到云端」，这里收一个 missions 列表（`[{name, path}, ...]`）就开工。
/// 不再让用户去文件选择器里翻 —— 要传的东西本来就在眼前那个列表里。
///
/// 可以一次传多条。后台**没有批量登记接口**（POST /upload-callback 一次只登记
/// 一条航线），所以这里是客户端排队：一条接一条地
///     本地 .plan → .kmz（convertPlanToKmz）→ C++ 直传对象存储 → 登记
/// 每条的结果单独记录，一条失败不影响后面几条。
///
/// 重名：云端不允许重名，但**不支持覆盖**，也不支持改名，所以查出撞名不是
/// 「覆盖」而是换个名字传一条新的，旧的照旧留在后台。这里在开工前按
/// WaylineLibraryView::loadedNames() 做一次本地精确比对（理由见那边的注释），
/// 撞上的自动加 `_2` / `_3` 后缀，并在行上标出来 —— 名字在下面都能改。
QGCPopupDialog {
    id: root

    property var wayline: null
    /// [{ name: "航线名", path: "/abs/path/xxx.plan" }, ...]
    property var missions: []
    /// 云端当前已加载的航线名，用于开工前的重名预判
    property var cloudNames: []

    /// 每条任务的状态。整体重新赋值，否则 QML 看不到数组内部变化
    property var jobs: []
    property int  currentJob: -1
    property int  progressPercent: -1
    property bool uploading: false

    readonly property int  doneCount:  _countStatus("done")
    readonly property int  failCount:  _countStatus("fail")
    readonly property bool allDone:    currentJob >= 0 && !uploading
                                       && (doneCount + failCount) === jobs.length
    /// 有任意一条被自动加过后缀。jobs 一变整体换数组，这个绑定会跟着重算
    readonly property bool hasRenamed: _anyRenamed()

    function _countStatus(kind) {
        var n = 0
        for (var i = 0; i < jobs.length; ++i) {
            if (jobs[i].status === kind) n++
        }
        return n
    }

    function _refreshJobs() {
        // 换个新数组，触发依赖 jobs 的绑定重新求值
        jobs = jobs.slice()
    }

    function _setJob(i, key, value) {
        if (i < 0 || i >= jobs.length) return
        var next = jobs.slice()
        var job = {}
        for (var k in next[i]) job[k] = next[i][k]
        job[key] = value
        next[i] = job
        jobs = next
    }

    // ---------------------------------------------------------------------
    // 开工前的重名预判
    // ---------------------------------------------------------------------

    /// 在 taken 之外给 name 找一个没被占用的名字：name、name_2、name_3 …
    function _freeName(name, taken) {
        if (taken.indexOf(name) < 0) return { "name": name, "renamed": false }
        for (var n = 2; n < 1000; ++n) {
            var candidate = name + "_" + n
            if (taken.indexOf(candidate) < 0) return { "name": candidate, "renamed": true }
        }
        return { "name": name, "renamed": false }
    }

    function _buildJobs() {
        var taken = (cloudNames ? cloudNames.slice() : [])
        var out = []
        for (var i = 0; i < missions.length; ++i) {
            var m = missions[i]
            var free = _freeName(m.name, taken)
            taken.push(free.name)
            out.push({
                "name":    free.name,
                "path":    m.path,
                "kmz":     "",
                "status":  "",          // "" / "running" / "done" / "fail"
                "renamed": free.renamed,
                "error":   ""
            })
        }
        return out
    }

    function _statusText(job) {
        if (job.status === "running") return qsTr("上传中…")
        if (job.status === "done")    return qsTr("✓ 完成")
        if (job.status === "fail")    return qsTr("✕ ") + job.error
        return qsTr("待上传")
    }

    function _statusColor(job) {
        if (job.status === "done")    return qgcPal.colorGreen
        if (job.status === "fail")    return qgcPal.warningText
        if (job.status === "running") return qgcPal.colorBlue
        return qgcPal.text
    }

    // ---------------------------------------------------------------------
    // 上传队列
    // ---------------------------------------------------------------------

    function _startJob(i) {
        if (i >= jobs.length) {
            uploading = false
            progressPercent = -1
            // 全部结束才刷新，中途刷新列表会把行号打乱（没必要）
            root.uploadCompleted(root.doneCount, root.failCount)
            return
        }

        currentJob = i
        progressPercent = 0

        var job = jobs[i]
        var kmz = job.kmz

        // 1. 本地 .plan 先转成 DJI 的 kmz（已经在队列里传过的就用现成的）
        if (kmz.length === 0) {
            var target = wayline.downloadDir() + "/" + job.name + ".kmz"
            if (!wayline.convertPlanToKmz(job.path, target)) {
                _setJob(i, "status", "fail")
                _setJob(i, "error", wayline.lastConvertError())
                _startJob(i + 1)
                return
            }
            kmz = target
            _setJob(i, "kmz", target)
        }

        _setJob(i, "status", "running")
        // 2. 交给 C++：取 STS 凭证 → 直传对象存储 → 登记航线。
        //    结果通过 uploadFinished / errorOccurred 回来，见下面的 Connections
        wayline.uploadWayline(kmz, jobs[i].name)
    }

    /// 「上传到云端」按钮的入口
    function startUpload() {
        if (uploading || jobs.length === 0) return
        uploading = true
        currentJob = -1
        progressPercent = -1
        _startJob(0)
    }

    function _renameJob(i, newName) {
        var trimmed = String(newName).trim()
        if (trimmed.length === 0 || i < 0 || i >= jobs.length) return
        if (trimmed === jobs[i].name) return
        _setJob(i, "name", trimmed)
        // 改过名就不再说"已自动加后缀"了
        _setJob(i, "renamed", false)
    }

    // 全部任务传完后发出去，PlanView 接住切页/刷新
    signal uploadCompleted(int doneCount, int failCount)

    title:   qsTr("上传到云端")
    buttons: Dialog.NoButton

    Component.onCompleted: jobs = _buildJobs()

    Connections {
        target: root.wayline

        function onUploadFinished(name) {
            if (!root.uploading) return
            root._setJob(root.currentJob, "status", "done")
            root.progressPercent = 100
            root._startJob(root.currentJob + 1)
        }

        function onErrorOccurred(message) {
            // 上传过程中只可能是本队列的失败。非上传期间（比如列表刷新失败）
            // 不归这里管，直接放过
            if (!root.uploading) return
            root._setJob(root.currentJob, "status", "fail")
            root._setJob(root.currentJob, "error", message)
            root._startJob(root.currentJob + 1)
        }

        function onTransferProgress(operation, percent) {
            if (operation === "upload" && root.uploading) {
                root.progressPercent = percent
            }
        }
    }

    ColumnLayout {
        spacing: ScreenTools.defaultFontPixelHeight * 0.35

        QGCLabel {
            id:               summaryLabel
            Layout.fillWidth: true
            wrapMode:         Text.WordWrap
            text: root.uploading
                  ? qsTr("正在上传第 ") + (root.currentJob + 1) + qsTr(" / ") + root.jobs.length + qsTr(" 条…")
                  : (root.allDone
                     ? qsTr("上传结束：成功 ") + root.doneCount + qsTr(" 条，失败 ") + root.failCount + qsTr(" 条。")
                     : qsTr("将上传 ") + root.jobs.length + qsTr(" 条本地任务到云端（按 M3E / M3E 相机登记）。"))
        }

        // 重名提示。只在这种情况才占版面
        QGCLabel {
            Layout.fillWidth: true
            wrapMode:         Text.WordWrap
            visible:          root.hasRenamed && !root.uploading
            font.pointSize:   ScreenTools.defaultFontPointSize
            color:            qgcPal.colorOrange
            text:             qsTr("云端已有同名航线，重名的已自动加后缀。云端不支持覆盖/改名，"
                                   + "确认后会传成一条新航线，旧记录仍留在后台。名字可以改。")
        }

        // ------------------------------------------------------------------
        // 任务清单：一行一条，名字可改
        // ------------------------------------------------------------------
        Rectangle {
            Layout.fillWidth:       true
            Layout.preferredHeight: Math.min(Math.max(jobList.contentHeight, 1) + 4,
                                             ScreenTools.defaultFontPixelHeight * 12)
            color:                  qgcPal.window
            border.color:           qgcPal.groupBorder
            border.width:           1
            visible:                root.jobs.length > 0

            QGCListView {
                id:      jobList
                anchors.fill:    parent
                anchors.margins: 2
                model:   root.jobs
                clip:    true
                spacing: 1

                delegate: RowLayout {
                    width:   jobList.width
                    spacing: ScreenTools.defaultFontPixelWidth * 0.5

                    QGCTextField {
                        Layout.fillWidth: true
                        text:             modelData.name
                        enabled:          !root.uploading && modelData.status !== "done"
                        onEditingFinished: root._renameJob(index, text)
                    }

                    QGCLabel {
                        Layout.preferredWidth:  ScreenTools.defaultFontPixelWidth * 9
                        text:                   modelData.renamed ? qsTr("已改名") : ""
                        font.pointSize:         ScreenTools.defaultFontPointSize
                        color:                  qgcPal.colorOrange
                        elide:                  Text.ElideRight
                    }

                    QGCLabel {
                        Layout.preferredWidth:  ScreenTools.defaultFontPixelWidth * 14
                        text:                   root._statusText(modelData)
                        color:                  root._statusColor(modelData)
                        font.pointSize:         ScreenTools.defaultFontPointSize
                        elide:                  Text.ElideRight
                        ToolTip.text:           modelData.error
                        ToolTip.visible:        hovered && modelData.error !== ""
                        property bool hovered:  jobHover.containsMouse

                        QGCMouseArea { id: jobHover; anchors.fill: parent; hoverEnabled: true }
                    }
                }
            }
        }

        // 进度条
        RowLayout {
            Layout.fillWidth: true
            visible:          root.progressPercent >= 0
            spacing:          ScreenTools.defaultFontPixelWidth * 0.5

            Rectangle {
                Layout.fillWidth:       true
                Layout.preferredHeight: 5
                radius:                 2
                color:                  qgcPal.windowShade

                Rectangle {
                    width:  parent.width * Math.max(0, Math.min(100, root.progressPercent)) / 100
                    height: parent.height
                    radius: parent.radius
                    color:  qgcPal.colorBlue
                }
            }
            QGCLabel {
                text: root.progressPercent + "%"
            }
        }

        // 失败的单独列出来，别让用户去小字里找
        QGCLabel {
            Layout.fillWidth: true
            wrapMode:         Text.WordWrap
            visible:          root.allDone && root.failCount > 0
            font.pointSize:   ScreenTools.defaultFontPointSize
            color:            qgcPal.warningText
            text:             qsTr("失败的条目可以关掉这个窗口后重试；若提示签名/权限相关，"
                                   + "多半是云端 token 过期，重连后即可。")
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: qgcPal.groupBorder; opacity: 0.5 }

        RowLayout {
            Layout.fillWidth: true
            spacing:          ScreenTools.defaultFontPixelWidth * 0.5

            Item { Layout.fillWidth: true }

            QGCButton {
                text:      qsTr("取消")
                visible:   !root.allDone
                enabled:   !root.uploading
                onClicked: close()
            }
            QGCButton {
                text:      qsTr("开始上传")
                primary:   true
                visible:   !root.allDone
                enabled:   !root.uploading && root.jobs.length > 0
                onClicked: root.startUpload()
            }
            QGCButton {
                text:      qsTr("关闭")
                primary:   true
                visible:   root.allDone
                onClicked: close()
            }
        }
    }

    /// 「已改名」提示要不要出现的判断。从 hasRenamed 那个只读属性调过来
    function _anyRenamed() {
        for (var i = 0; i < jobs.length; ++i) {
            if (jobs[i].renamed) return true
        }
        return false
    }
}
