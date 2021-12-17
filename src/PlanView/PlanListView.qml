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
    property var toolTipDelay: 1000
    property var directory

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
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 1
            height: 50
            state: "normal"
            property var rowHovered: mouseArea.containsMouse
            color: ListView.isCurrentItem ? qgcPal.window : (rowHovered ? qgcPal.windowShadeDark : qgcPal.windowShade)
            focus: true

            MouseArea {
                id: mouseArea
                anchors.fill: parent
                hoverEnabled: true
                propagateComposedEvents: true
                onClicked: {
                    listView.currentIndex = index
                    _currentPlanFileName = folderModel.get(index, "fileBaseName")
                    console.log(_currentPlanFileName)
                    _planMasterController.loadFromFile(_currentPlanFileDir + "/" + folderModel.get(index, "fileName"))
                    _planMasterController.fitViewportToItems()
                }
                onEntered: rowHovered = true
                onExited: rowHovered = false

                RowLayout {
                    id: rowLayout
                    anchors.fill: parent
                    anchors.leftMargin: 5
//                    anchors.rightMargin: 5
                    QGCLabel {
                        id: fileNameLable
                        text: fileBaseName
                        elide: Text.ElideRight
                        Layout.maximumWidth: rowBtn.visible ? parent.width - rowBtn.width - 15 : rect.width - rect.anchors.margins
                        Layout.fillWidth: true
//                        Layout.verticalCenter: parent.verticalCenter
//                        height
//                        Layout.fillHeight: true
//                        font.pointSize: ScreenTools.defaultFontPointSize * 1.5
                        color: qgcPal.buttonText
                        property var lableHovered: mouseArea.containsMouse
                        ToolTip {
                            id: toolTipText
                            text: fileNameLable.text
                            delay: toolTipDelay
                            visible: mouseAreaText.containsMouse
                        }

                        MouseArea {
                            id: mouseAreaText
                            anchors.fill: parent
                            hoverEnabled: true
                            propagateComposedEvents: true
                            onDoubleClicked: {
                                textInput.visible = true
                                textInput.focus = true
                                textInput.text = _currentPlanFileName
                                mouse.accepted = true
                                console.log("double clicked", rect.height, mouseArea.height,rowLayout.height, fileNameLable.height)
                            }
                        }

                        QGCTextField {
                            id: textInput
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            implicitHeight: rect.height / 2
                            Layout.fillWidth: true
                            text: qsTr("")
                            visible: focus
                            onAccepted: {
                                visible = false
                                _planMasterController.renameCurrentFile(text)
                            }
                        }

                    }

                    Row {
                        id: rowBtn
                        spacing: 5
                        Layout.alignment: Qt.AlignRight
                        Layout.rightMargin: ScreenTools.defaultFontPixelWidth
                        QGCIconButton {
                            id: btnEdit
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
                                _currentPlanFileName = folderModel.get(index, "fileName")
                                _planMasterController.loadFromFile(_currentPlanFileDir + "/" + _currentPlanFileName)
                                _planMasterController.fitViewportToItems()
                                enterPlanEditMode(true)
                            }
                        }
                        QGCIconButton {
                            id: btnSave
                            visible: rect.ListView.isCurrentItem || rect.rowHovered ? true : false
                            _showHighlight: hovered
                            iconSource: "/qmlimages/DeleteFile.svg"
                            ToolTip.text: qsTr("delete")
                            highlighted: hovered
                            onClicked: {
                                toolTipDelete.visible = false
                                if (listView.currentIndex === index) {
                                    _planMasterController.removeAll();
                                    _planMasterController.removeAllFromVehicle();
                                }

                                listView.currentIndex = index
                                _currentPlanFileName = folderModel.get(index, "fileName")
                                var removedResule = _planMasterController.removeSelectedFiles(_currentPlanFileDir + "/" + _currentPlanFileName)
                                console.log("remove file:", _currentPlanFileDir + "/" + _currentPlanFileName, removedResule)
                            }
                            ToolTip {
                                id: toolTipDelete
                                text: "delete"
                                visible: parent.hovered
                                delay: toolTipDelay
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
                color: "grey"
            }
        }
    }
}


