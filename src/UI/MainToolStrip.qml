import QtQuick
import QtQuick.Controls

import QGroundControl                   1.0
import QGroundControl.Controls          1.0
import QGroundControl.ScreenTools       1.0
import QGroundControl.FlightMap         1.0

ToolStripColumn {
    anchors.left: parent.left
    anchors.top: parent.top
    anchors.bottom: parent.bottom
    radius: 0
    z: QGroundControl.zOrderWidgets
    width:            60//ScreenTools.toolbarHeight
    readonly property int flyButtonIndex:       0
    readonly property int fileButtonIndex:      1
    readonly property int takeoffButtonIndex:   2
    readonly property int waypointButtonIndex:  3
    readonly property int roiButtonIndex:       4
    readonly property int patternButtonIndex:   5
    readonly property int landButtonIndex:      6
    readonly property int centerButtonIndex:    7
    //        property bool _isRallyLayer:    _editingLayer == _layerRallyPoints
    //        property bool _isMissionLayer:  _editingLayer == _layerMission
    property bool currentVehicleSetupComplete: false
    ToolStripActionList {
        id: toolStripActionList
        model: [
            ToolStripAction {
                text:  qsTr("Fly")
                iconSource: "/qmlimages/PaperPlane.svg"
                visible:   true
                enabled:   true
                checkable: true
                checked:   true
                onTriggered: {
                    stackLayoutMain.currentIndex = 0
                    // if (!mainWindow.preventViewSwitch()) {
                        // mainWindow.showFlyView()
                    // }
                }
            },
            ToolStripAction {
                text: qsTr("Plan")
                iconSource: "/qmlimages/Plan.svg"
                visible:   true
                checkable: true
                onTriggered: {
                    stackLayoutMain.currentIndex = 1
                    // if (!mainWindow.preventViewSwitch()) {
                        // mainWindow.showPlanView()
                    // }
                }
            },

            ToolStripAction {
                text: qsTr("Parameters")
                visible: true //currentVehicleSetupComplete
                checkable: true
                iconSource: "/qmlimages/Gears.svg"
                onTriggered: {
                    stackLayoutMain.currentIndex = 2
                    // if (!mainWindow.preventViewSwitch()) {
                        // mainWindow.showParameterTool()
                    // }
                }
            },
            ToolStripAction {
                id: toolStripActionSettings
                text: qsTr("Settings")
                visible: true //currentVehicleSetupComplete
                checkable: true
                iconSource: "/res/gear-white.svg"//"/qmlimages/Gears.svg"
                onTriggered: {
                    stackLayoutMain.currentIndex = 3
                    // if (!mainWindow.preventViewSwitch()) {
                        // mainWindow.showSettingsTool()
                    // }
                }
            },
            ToolStripAction {
                id: toolStripActionUser
                text: qsTr("User")
                visible: true //currentVehicleSetupComplete
                checkable: true
                iconSource: "/qmlimages/User.svg"//"/qmlimages/Gears.svg"
                onTriggered: {
                    stackLayoutMain.currentIndex = 4
                    // if (!mainWindow.preventViewSwitch()) {
                        // mainWindow.showSettingsTool()
                    // }
                }
            }
        ]
    }
    model: toolStripActionList.model

    //- ToolStrip DropPanel Components
    Component {
        id: centerMapDropPanel

        CenterMapDropPanel {
            map:            editorMap
            fitFunctions:   mapFitFunctions
        }
    }
    // ToolStripAction {
    //     id: toolStripActionSettings
    //     text: qsTr("Settings")
    //     visible: true //currentVehicleSetupComplete
    //     checkable: true
    //     iconSource: "/res/gear-white.svg"//"/qmlimages/Gears.svg"
    //     onTriggered: {
    //         if (!mainWindow.preventViewSwitch()) {
    //             mainWindow.showSettingsTool()
    //         }
    //     }
    // }

    // ToolStripHoverButton {
    //     id:                 buttonSettings
    //     anchors.left:       parent.left
    //     anchors.right:      parent.right
    //     anchors.bottom:     parent.bottom
    //     anchors.bottomMargin: 5

    //     height:             width
    //     radius:             ScreenTools.defaultFontPixelWidth / 2
    //     fontPointSize:      ScreenTools.smallFontPointSize
    //     toolStripAction:    toolStripActionSettings
    //     dropPanel:          _dropPanel
    //     // showText:           showActionText
    //     onDropped:          _root.dropped(index)
    //     onCheckedChanged: {
    //         uncheckAllActionButton()
    //     }
    // }
    // ToolStripAction {
    //     id: toolStripActionUser
    //     text: qsTr("User")
    //     visible: true //currentVehicleSetupComplete
    //     checkable: true
    //     iconSource: "/qmlimages/User.svg"//"/qmlimages/Gears.svg"
    //     onTriggered: {
    //         if (!mainWindow.preventViewSwitch()) {
    //             mainWindow.showSettingsTool()
    //         }
    //     }
    // }

    // ToolStripHoverButton {
    //     id:                 buttonUser
    //     anchors.left:       parent.left
    //     anchors.right:      parent.right
    //     anchors.bottom:     buttonSettings.top
    //     anchors.bottomMargin: 5

    //     height:             width
    //     radius:             ScreenTools.defaultFontPixelWidth / 2
    //     fontPointSize:      ScreenTools.smallFontPointSize
    //     toolStripAction:    toolStripActionUser
    //     dropPanel:          _dropPanel
    //     // showText:           showActionText
    //     onDropped:          _root.dropped(index)
    //     onCheckedChanged: {
    //         uncheckAllActionButton()
    //     }
    // }
}
