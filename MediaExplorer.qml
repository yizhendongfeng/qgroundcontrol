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
            anchors.fill: parent
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

                    QGCButton {
                        text: "刷新"
                        onClicked: fileModel.refreshCurrentFolder()
                    }
                    QGCButton {
                        text: "上传"
                        onClicked: fileModel.uploadFilesToMinio()
                    }

                }

                GridView {
                    id: grid
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    MouseArea {
                        anchors.fill: parent
                        propagateComposedEvents: true
                        onClicked: (mouse) => {
                                       var indexTemp = grid.indexAt(mouse.x, mouse.y)
                                       if (indexTemp === -1) {
                                           fileModel.clearAllSelection()
                                       } else {
                                           // fileModel.clickSelect(indexTemp, mouse.modifiers)
                                           mouse.accepted = false
                                       }
                                       // console.log("GridView clicked indexTemp:", indexTemp)
                                   }
                    }
                    model: fileModel
                    cellWidth: 200
                    cellHeight: 220

                    delegate: Rectangle {
                        width: 190
                        height: 200
                        radius: 6
                        border.color: selected ?  qgcPal.buttonText : "transparent" //"#ddd"
                        color: selected ? "#505050" :  (mouseArea.containsMouse  ? "#505050" : "transparent") //"#fafafa" : "#505050"

                        Column {
                            anchors.centerIn: parent
                            spacing: 2
                            Item {
                                width: 16
                                height: width
                                anchors.right: parent.right
                                anchors.rightMargin: 2
                                Image {
                                    id: cloudTip
                                    smooth:             true
                                    mipmap:             true
                                    antialiasing:       true
                                    sourceSize.height:  height
                                    fillMode:           Image.PreserveAspectFit
                                    anchors.fill:       parent
                                    source: "qrc:/InstrumentValueIcons/CloudServer.svg"
                                    visible: uploadStatus === 2 || uploadStatus === 3 // 0: NotUploaded; 1:Uploading; 2: Uploaded; 3:UploadFailed
                                    Item{
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.bottom: parent.bottom
                                        height: parent.height * uploadProgress
                                        clip: true
                                        ColorOverlay {
                                            anchors.left:  parent.left
                                            anchors.right: parent.right
                                            anchors.bottom: parent.bottom
                                            height: cloudTip.height
                                            source: cloudTip
                                            color: qgcPal.buttonText
                                        }
                                    }
                                }
                            }

                            Image {
                                id: thumb
                                width: 180
                                height: width / 1.77
                                fillMode: Image.PreserveAspectFit
                                visible: thumbReady
                                source: thumbnailUrl
                                // Component.onCompleted: console.log("thumbnailUrl", thumbnailUrl)
                                QGCColoredImage {
                                    anchors.centerIn: parent
                                    width: 48
                                    height: width
                                    visible: isVideo
                                    source: "qrc:/InstrumentValueIcons/play.svg"
                                    color: qgcPal.buttonText
                                    opacity: 0.9
                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                    }
                                }
                            }

                            Rectangle {
                                width: 180
                                height: 180
                                visible: !thumbReady
                                color: "#e0e0e0"
                                radius: 4
                                BusyIndicator {
                                    anchors.centerIn: parent
                                    running: true
                                }
                            }

                            QGCLabel {
                                text: filePath.split("/").pop()
                                width: 180
                                wrapMode: Text.WrapAnywhere
                                horizontalAlignment: Text.AlignHCenter
                                // font.pointSize: ScreenTools.mediumFontPointSize
                                color: qgcPal.buttonText
                            }
                        }

                        MouseArea {
                            id: mouseArea
                            anchors.fill: parent
                            propagateComposedEvents: false
                            hoverEnabled: true
                            onClicked: (mouse) => {
                                           // grid.currentIndex = index
                                           // grid.selectionModel.select(index, SelectionModel.Toggle)
                                           fileModel.clickSelect(index, mouse.modifiers)
                                           // mouse.accepted = true
                                       }

                            onDoubleClicked: {
                                let url = Qt.resolvedUrl("file:///" + filePath)
                                Qt.openUrlExternally(url)
                            }
                        }
                    }
                    Component.onCompleted: fileModel.refreshCurrentFolder()
                }
            }
        }

        Item {
            anchors.fill: parent
            Row {
                id: rowQuery
                spacing: 5
                anchors.top: parent.top
                height: 40
                QGCLabel {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("查询日期范围:")
                    font.pointSize: ScreenTools.mediumFontPointSize
                }
                QGCTextField {
                    id: startDate
                    anchors.verticalCenter: parent.verticalCenter
                    text: "2020-01-01"
                    placeholderText: "2020-01-01"
                }
                QGCLabel {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "--"
                }
                QGCTextField {
                    id: endDate
                    anchors.verticalCenter: parent.verticalCenter
                    text: "2025-01-01"
                    placeholderText: "2025-01-01"
                }
                QGCButton {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("查询")
                    onClicked: {
                        if (startDate.text === "" || endDate.text ==="") {
                            mainWindow.showMessageDialog(qsTr("警告"), qsTr("请输入正确格式的日期:年-月-日"), Dialog.Ok)
                        }

                        fileModel.mediaDownload.refreshMediaInPod(startDate.text, endDate.text)
                    }
                }
                QGCButton {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("下载选中文件")
                    onClicked: fileModel.mediaDownload.startDownloadFiles()
                }
            }

            ListView {
                id: podMediaListView
                anchors.top: rowQuery.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 5
                model: fileModel.mediaDownload
                delegate: Rectangle {
                    height: 30
                    color: (index % 2) ? qgcPal.toolbarBackground : "red"
                    RowLayout {
                        anchors.left: parent.left
                        QGCCheckBox {
                            // checked: fileSelected
                            onCheckedChanged: {
                                console.log("index:", index, ",checked:" , checked)
                                fileSelected = checked
                            }
                        }
                        QGCLabel {
                            text: index + 1
                            Layout.preferredWidth: 30
                            // horizontalAlignment: Qt.AlignRight
                            font.pointSize: ScreenTools.mediumFontPointSize
                        }
                        ProgressBar {
                            id: progressBar
                            value: downloadedProgress
                            Layout.preferredHeight: 20
                            Layout.preferredWidth: 40
                            background:  Rectangle {
                                width: progressBar.width
                                height:progressBar.height
                                color: qgcPal.groupBorder
                                radius: 5
                            }

                            contentItem: Item {
                                implicitWidth: progressBar.implicitWidth
                                implicitHeight: progressBar.implicitHeight
                                Rectangle {
                                    id: indicatorRect
                                    color: qgcPal.buttonHighlight
                                    width: progressBar.width * progressBar.value
                                    height:progressBar.height
                                    radius: 5
                                }
                            }
                        }

                        QGCLabel {
                            text: filePathStr
                            Layout.preferredWidth: 500
                            font.pointSize: ScreenTools.mediumFontPointSize
                        }
                        QGCLabel {
                            text: fileSizeStr
                            Layout.preferredWidth: 50
                            // font.pointSize: ScreenTools.mediumFontPointSize
                        }
                    }
                }
            }
        }
    }
}
