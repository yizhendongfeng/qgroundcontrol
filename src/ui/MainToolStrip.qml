import QtQuick          2.3
import QtQuick.Controls 1.2

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
                visible: true
                enabled: true
                checkable: true
                onTriggered: {
                    if (!mainWindow.preventViewSwitch()) {
                        mainWindow.showFlyView()
                    }
                }
            },
            ToolStripAction {
                text: qsTr("Plan")
                iconSource: "/qmlimages/Plan.svg"
                visible: true
                checkable: true
                onTriggered: {
                    if (!mainWindow.preventViewSwitch()) {
                        mainWindow.showPlanView()
                    }
                }
            },

            ToolStripAction {
                text: qsTr("Parameters")
                visible: currentVehicleSetupComplete
                checkable: true
                iconSource: "/qmlimages/Gears.svg"
                onTriggered: {
                    if (!mainWindow.preventViewSwitch()) {
                        mainWindow.showParameterTool()
                    }
                }
            }

//            ToolStripAction {       // 作为弹簧控件使用，让设置按钮出现在最下边
//                text: qsTr("")
//                visible: true       //false
//                checkable: false
//                iconSource: ""
//            },

//            ToolStripAction {
//                text: qsTr("Settings")
//                visible: true //currentVehicleSetupComplete
//                checkable: true
//                iconSource: "/qmlimages/Gears.svg"
//                onTriggered: {
//                    if (!mainWindow.preventViewSwitch()) {
//                        mainWindow.showParameterTool()
//                    }
//                }
//            }
//            ToolStripAction {
//                text: qsTr("Analyze")
//                visible: true
//                checkable: true
//                iconSource: "/qmlimages/Analyze.svg"
//                onTriggered: {
//                    if (!mainWindow.preventViewSwitch()) {
//                        mainWindow.showAnalyzeTool()
//                    }
//                }
//            }
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
    ToolStripAction {
        id: toolStripActionSettings
        text: qsTr("Settings")
        visible: true //currentVehicleSetupComplete
        checkable: true
        iconSource: "/res/gear-white.svg"//"/qmlimages/Gears.svg"
        onTriggered: {
            if (!mainWindow.preventViewSwitch()) {
                mainWindow.showSettingsTool()
            }
        }
    }

    ToolStripHoverButton {
        id:                 buttonSettings
        anchors.left:       parent.left
        anchors.right:      parent.right
        anchors.bottom:     parent.bottom
        anchors.bottomMargin: 5

        height:             width
        radius:             ScreenTools.defaultFontPixelWidth / 2
        fontPointSize:      ScreenTools.smallFontPointSize
        toolStripAction:    toolStripActionSettings
        dropPanel:          _dropPanel
        showText:           showActionText
        onDropped:          _root.dropped(index)
        onCheckedChanged: {
            uncheckAllActionButton()
            // We deal with exclusive check state manually since usinug autoExclusive caused all sorts of crazt problems
//            if (checked) {
//                for (var i=0; i<repeater.count; i++) {
//                    if (i != index) {
//                        var button = repeater.itemAt(i)
//                        if (button.checked) {
//                            button.checked = false
//                        }
//                    }
//                }
//            }
        }
    }
}
