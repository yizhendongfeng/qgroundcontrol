import QtQuick 2.12
import Qt.labs.folderlistmodel 2.2
import QtQuick.Layouts 1.3
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.12

import QGroundControl.Controls 1.0
import QGroundControl.ScreenTools 1.0


QGCListView {
    id: listView

    focus: true
    model: folderModel
    delegate: fileDelegate
    property int toolTipDelay: 500
    property var directory
    property color _splitLineColor: "#707070" // "#424242"
    property color _splitLineColorLighter: "#424242" // "#424242"
    FolderListModel {
        id: folderModel
        folder: "file:" + _appSettings.missionSavePath
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
            height: 50
            state: "normal"
            property bool rowHovered: mouseArea.containsMouse
            color: ListView.isCurrentItem ? qgcPal.window : (rowHovered ? qgcPal.windowShadeDark : qgcPal.windowShade)

            MouseArea {
                id: mouseArea
                anchors.fill: parent
                hoverEnabled: true
                propagateComposedEvents: true
                onClicked: {
                    listView.currentIndex = index
                    listView.focus = true
                    _currentPlanFileName = folderModel.get(index, "fileBaseName")
                    _planMasterController.loadFromFile(_currentPlanFileDir + "/" + _currentPlanFileName)
                    _planMasterController.fitViewportToItems()
                    console.log("mouseArea in plan listview", listView.currentIndex)
                }

                RowLayout {
                    id: rowLayout
                    anchors.fill: parent
                    anchors.leftMargin: 5
                    TextField {
                        id: textInput
                        Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter
                        Layout.leftMargin: 5
                        Layout.fillWidth: true
                        implicitHeight: rect.height / 2
                        text: fileBaseName
                        color: qgcPal.text
                        hoverEnabled: listView.currentIndex === index
                        enabled: listView.currentIndex === index
                        background: Rectangle {
                            anchors.fill: parent
                            color: "transparent"
                            border.color: textInput.focus ? qgcPal.colorGrey : "transparent"
                        }
                        leftPadding: focus ? 5 : 0
                        onEditingFinished: {
                            focus = false                           
                            if (_currentPlanFileName !== text && text != "") {
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
                        Layout.alignment: Qt.AlignRight
                        Layout.rightMargin: 10
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
                                    _currentPlanFileName = folderModel.get(index, "fileBaseName")
                                    removedResule = _planMasterController.removeSelectedFiles(_currentPlanFileDir + "/" + _currentPlanFileName)
                                    listView.currentIndex = -1  // 不选任何任务文件
                                    _currentPlanFileName = ""
                                } else {
                                    var selectedPlanFileName = folderModel.get(index, "fileBaseName")
                                    removedResule = _planMasterController.removeSelectedFiles(_currentPlanFileDir + "/" + selectedPlanFileName)
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
                                console.log("folderModel.get:", folderModel.get(index, "fileName"))
                                _currentPlanFileName = folderModel.get(index, "fileBaseName")
                                _planMasterController.loadFromFile(_currentPlanFileDir + "/" + _currentPlanFileName)
                                _planMasterController.fitViewportToItems()
                                enterPlanEditMode(true)
                            }
                        }

                    }
                }

            }

            Rectangle {
                height: 1
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                color: _splitLineColorLighter
            }
        }
    }
    Component.onCompleted: listView.currentIndex = -1
}


