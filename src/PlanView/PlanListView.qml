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
    property color _splitLineColor: "#707070" // "#424242"
    property color _splitLineColorLighter: "#424242" // "#424242"
    FolderListModel {
        id: folderModel
        folder: "file:" + _appSettings.missionSavePath + "/" + directory
        nameFilters: ["*.plan"]
        sortField: FolderListModel.Time
    }

    Component {
        id: fileDelegate
        Rectangle {
            id: rect
//            anchors.left: parent.left
//            anchors.right: parent.right
            width: listView.width
            anchors.leftMargin: 1
            height: 70
            state: "normal"
            property bool rowHovered: mouseArea.containsMouse
            color: (ListView.isCurrentItem || rowHovered) ? qgcPal.toolbarBackground : qgcPal.windowShade

            MouseArea {
                id: mouseArea
                anchors.fill: parent
                hoverEnabled: true
                propagateComposedEvents: true
                onClicked: {
                    listView.currentIndex = index
                    listView.focus = true
                    _currentPlanFileName = folderModel.get(index, "fileBaseName")
                    _planMasterController.loadFromFile(_missionFolder + "/" + folderModel.get(index, "fileName"))
                    _planMasterController.fitViewportToItems()
                    console.log("mouseArea in plan listview", listView.currentIndex, _currentPlanFileName)
                }
                Column {
                    anchors.fill: parent
                    spacing: 2
                    RowLayout {
                        id: rowLayoutPlanName
                        anchors.left: parent.left
                        anchors.right: parent.right
                        // anchors.fill: parent
                        anchors.leftMargin: 5
                        TextField {
                            id: textInput
                            Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter
                            Layout.leftMargin: 5
                            Layout.fillWidth: true
                            implicitHeight:   28
                            font.bold:        true
                            font.pointSize:   11
                            text: fileBaseName
                            color: qgcPal.text
                            hoverEnabled: listView.currentIndex === index
                            enabled:      listView.currentIndex === index
                            background: Rectangle {
                                anchors.fill: parent
                                color: "transparent"
                                border.color: (parent.hovered || parent.focus) ? qgcPal.colorGrey : "transparent"
                            }
                            leftPadding: focus ? 5 : 0
                            onEditingFinished: {
                                focus = false
                                if (_currentPlanFileName !== text && text !== "") {
                                    var previousPlanFileName = _currentPlanFileName
                                    _currentPlanFileName = text
                                    if (!_planMasterController.renameCurrentFile(text)) {
                                        _currentPlanFileName = previousPlanFileName
                                        text = previousPlanFileName
                                    }
                                } else if (text == "") {
                                    text = fileName
                                }
                            }
                        }

                        Row {
                            id: rowBtn
                            spacing: 5
                            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                            Layout.rightMargin: 10
                            // LabelledButton{

                            // }

                            QGCIconButton {
                                id: buttonDelete
                                visible: rect.ListView.isCurrentItem || rect.rowHovered ? true : false
                                showNormal: hovered
                                iconSource: "/qmlimages/DeleteFile.svg"
                                ToolTip.text: qsTr("delete")
                                highlighted: hovered
                                onClicked: {
                                    var removedResule
                                    toolTipDelete.visible = false
                                    if (listView.currentIndex === index) {
                                        _planMasterController.removeAll();
    //                                    _planMasterController.removeAllFromVehicle();
                                        // _currentPlanFileName = folderModel.get(index, "fileBaseName")
                                        removedResule = _planMasterController.removeSelectedFiles(_missionFolder + "/" + folderModel.get(index, "fileBaseName"))
                                        listView.currentIndex = -1  // 不选任何任务文件
                                        _currentPlanFileName = ""
                                    } else {
                                        // var selectedPlanFileName =
                                        removedResule = _planMasterController.removeSelectedFiles(_missionFolder + "/" + folderModel.get(index, "fileBaseName"))
                                        _currentPlanFileName = ""
                                    }
                                }
                                ToolTip {
                                    id: toolTipDelete
                                    text: "delete"
                                    visible: parent.hovered
                                    delay: toolTipDelay
                                }

                            }

                            QGCIconButton {
                                id: buttonEdit
                                visible: rect.ListView.isCurrentItem || rect.rowHovered ? true : false
                                iconSource: "/qmlimages/Edit.svg"
                                ToolTip {
                                    id: toolTipEdit
                                    text: "edit"
                                    visible: parent.hovered
                                    delay: toolTipDelay
                                }
                                highlighted: hovered
                                onClicked: {
                                    toolTipEdit.visible = false
                                    listView.currentIndex = index
                                    _currentPlanFileName = folderModel.get(index, "fileBaseName")
                                    _planMasterController.loadFromFile(_missionFolder + "/" + folderModel.get(index, "fileName"))
                                    _planMasterController.fitViewportToItems()
                                    enterPlanEditMode(true)
                                }
                            }

                        }
                    }
                    RowLayout {
                        id:            rowLayoutPlanInfo
                        anchors.left:  rowLayoutPlanName.left
                        anchors.right: rowLayoutPlanName.right
                        anchors.leftMargin: 5
                        Column {
                            Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter
                                QGCLabel {
                                    text: qsTr("修改时间：") + (fileModified instanceof Date ? Qt.formatDateTime(fileModified, "yyyy-MM-dd hh:mm") : "--.--")
                                }
                                QGCLabel {
                                    text: qsTr("完成时间：") +  (fileAccessed  instanceof Date? Qt.formatDateTime(fileAccessed , "yyyy-MM-dd hh:mm") : "--.--")
                            }

                        }
                        Column {
                            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                            Layout.rightMargin: 10
                            QGCLabel {
                                // width: 100
                                text: qsTr("航线距离：") + "2Km"
                            }
                            QGCLabel {
                                // width: 100
                                text: qsTr("指挥人员：") + qsTr("王强")
                            }
                        }

                    }
                }

            }

            Rectangle {
                height: 1
                anchors.bottom: parent.bottom
                anchors.left:   parent.left
                anchors.right:  parent.right
                color:          qgcPal.groupBorder//_splitLineColorLighter
            }
        }
    }
    Component.onCompleted: {
        listView.currentIndex = -1
    }
}


