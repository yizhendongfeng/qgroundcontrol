/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

import QtLocation
import QtPositioning
import QtQuick.Window
import QtQml.Models
import Qt.labs.platform 1.1

import QGroundControl
import QGroundControl.Controllers
import QGroundControl.Controls
import QGroundControl.FactSystem
import QGroundControl.FlightDisplay
import QGroundControl.FlightMap
import QGroundControl.Palette
import QGroundControl.ScreenTools
import QGroundControl.Vehicle
import QGroundControl.GCU
// 3D Viewer modules
import Viewer3D

Item {
    id: _root

    // These should only be used by MainRootWindow
    property var planController:    _planController
    property var guidedController:  _guidedController
    property var borderColor:       QGroundControl.globalPalette.groupBorder
    // Properties of UTM adapter
    property bool utmspSendActTrigger: false

    PlanMasterController {
        id:                     _planController
        flyView:                true
        Component.onCompleted:  start()
    }

    property bool   _mainWindowIsMap:       mapControl.pipState.state === mapControl.pipState.fullState
    property bool   _isFullWindowItemDark:  _mainWindowIsMap ? mapControl.isSatelliteMap : true
    property var    _activeVehicle:         QGroundControl.multiVehicleManager.activeVehicle
    property var    _missionController:     _planController.missionController
    property var    _geoFenceController:    _planController.geoFenceController
    property var    _rallyPointController:  _planController.rallyPointController
    property real   _margins:               ScreenTools.defaultFontPixelWidth / 2
    property var    _guidedController:      guidedActionsController
    property var    _guidedValueSlider:     guidedValueSlider
    property var    _widgetLayer:           widgetLayer
    property real   _toolsMargin:           ScreenTools.defaultFontPixelWidth * 0.75
    property rect   _centerViewport:        Qt.rect(0, 0, width, height)
    property real   _rightPanelWidth:       ScreenTools.defaultFontPixelWidth * 30
    property var    _mapControl:            mapControl

    property real   _fullItemZorder:    0
    property real   _pipItemZorder:     QGroundControl.zOrderWidgets
    property string _signalLightColor:  "green"
    property string _selectedMp3File:  qsTr("前方雨雪.mp3")
    property var    _gcu:              QGroundControl.videoManager.gcu
    function _calcCenterViewPort() {
        var newToolInset = Qt.rect(0, 0, width, height)
        toolstrip.adjustToolInset(newToolInset)
    }



    function dropMainStatusIndicatorTool() {
        toolbar.dropMainStatusIndicatorTool();
    }

    QGCToolInsets {
        id:                     _toolInsets
        leftEdgeBottomInset:    _pipView.leftEdgeBottomInset
        bottomEdgeLeftInset:    _pipView.bottomEdgeLeftInset
    }

    // FlyViewToolBar {
    //     id:         toolbar
    //     visible:    !QGroundControl.videoManager.fullScreen
    // }

    Rectangle {
        id:               leftPanel
        anchors.left:     parent.left
        anchors.top:      parent.top
        anchors.bottom:   parent.bottom
        width:            _pipView.width
        color:            qgcPal.windowShadeLight
    }

    PipView {
        id:                     _pipView
        anchors.left:           parent.left
        anchors.top:            parent.top
        item1IsFullSettingsKey: "MainFlyWindowIsMap"
        item1:                  mapControl
        item2:                  videoControl
        _pipFullParent:         mapHolder
        show:                   true
        z:                      QGroundControl.zOrderWidgets

        property real leftEdgeBottomInset: visible ? width + anchors.margins : 0
        property real bottomEdgeLeftInset: visible ? height + anchors.margins : 0
    }

    Rectangle {
        id:              itemLoad   // 负载界面
        anchors.left:    leftPanel.left
        anchors.right:   leftPanel.right
        anchors.top:     _pipView.bottom
        anchors.bottom:  leftPanel.bottom
        color:           qgcPal.windowShade

        Item {    // 吊舱的设置界面
            id:                         itemPod
            anchors.top:                parent.top
            anchors.left:               parent.left
            anchors.right:              parent.right
            anchors.margins:            5
            height:                     200
            // color:                      "red"
            QGCLabel {
                id:                     textPodTitle
                text:                   qsTr("吊舱")
                font.pointSize:         14//ScreenTools.smallFontPointSize
                anchors.top:            parent.top
                anchors.horizontalCenter:  parent.horizontalCenter
            }

            RowLayout {   // 吊舱的设置、云台控制、固定视角界面
                anchors.top:            textPodTitle.bottom
                anchors.bottom:         parent.bottom
                anchors.left:           parent.left
                anchors.right:          parent.right
                Column {
                    Layout.fillHeight:  true
                    Layout.fillWidth:   true
                    Layout.preferredWidth:  parent.width / 3
                    QGCLabel {
                        text:     qsTr("型号: ") + qsTr("DC-30-GA")
                        anchors.horizontalCenter: parent.horizontalCenter
                    }

                    Rectangle {
                        color:                  "transparent"
                        radius:                 10
                        border.width:           1
                        border.color:           borderColor
                        anchors.horizontalCenter: parent.horizontalCenter
                        width:                    parent.width
                        height:                   150
                        ColumnLayout {
                            id:                 itemPodSettings
                            anchors.fill:       parent
                            anchors.margins:    5
                            Row {
                                Layout.fillWidth:  true
                                // QGCLabel {
                                //     text:           qsTr("图像: ")
                                //     anchors.verticalCenter: parent.verticalCenter
                                // }

                                QGCRadioButton {
                                    text:           qsTr("可见光")
                                    checked:        true
                                    anchors.verticalCenter: parent.verticalCenter
                                    onClicked: {
                                        _gcu.setVisualLight(true);
                                    }
                                }
                                QGCRadioButton {
                                    text:             qsTr("夜视")
                                    anchors.verticalCenter: parent.verticalCenter
                                    onClicked: {
                                        _gcu.setVisualLight(false);
                                    }
                                }
                            }

                            QGCCheckBoxSlider {
                                id:                 useCheckList
                                Layout.fillWidth:   true
                                text:               qsTr("OSD")
                                visible:            true
                            }
                            QGCButton {
                                text:               qsTr("设置")
                                Layout.fillWidth:   true
                                Layout.leftMargin:  _margins * 2
                                Layout.rightMargin: _margins * 2
                                height:             ScreenTools.defaultFontPixelHeight
                                heightFactor: 0.1
                                // background: Rectangle {
                                //     color:      qgcPal.buttonHighlight
                                //     opacity:    pressed ? 1 : enabled && hovered ? .2 : 0
                                //     radius:     ScreenTools.defaultFontPixelWidth / 2
                                // }
                                onClicked: {
                                    var componentVideoSetting = Qt.createComponent("qrc:/qml/DialogueVideoSettings.qml")
                                    if (componentVideoSetting.status === Component.Ready) {

                                        var objectVideoSetting = componentVideoSetting.createObject(mainWindow)
                                        if (objectVideoSetting !== null)
                                            objectVideoSetting.open()
                                        else {
                                            console.log("Error creating object:", componentVideoSetting.errorString())
                                        }
                                    } else if (componentVideoSetting.status === Component.Error){
                                        console.log("Error loading component:", componentVideoSetting.errorString())
                                    }
                                }
                            }
                        }
                    }
                }

                Column {
                    Layout.fillHeight:  true
                    Layout.fillWidth:   true
                    Layout.preferredWidth:  parent.width * 0.4
                    QGCLabel {
                        text:     qsTr("吊舱控制 ")
                        anchors.horizontalCenter: parent.horizontalCenter
                    }

                    Rectangle {
                        color:                  "transparent"
                        radius:                 10
                        border.width:           1
                        border.color:           borderColor
                        anchors.horizontalCenter: parent.horizontalCenter
                        width:                    parent.width
                        height:                   150
                        Rectangle {       // 外圆
                            id:            outerCircle
                            width:         ScreenTools.implicitIconButtonHeight * 6//parent.width * 0.5
                            height:        width
                            radius:        width / 2
                            color:         "transparent"
                            border.width:           1
                            border.color:           borderColor
                            anchors.left:           parent.left
                            anchors.leftMargin:     _margins
                            anchors.verticalCenter: parent.verticalCenter
                            Rectangle {    // 内圆
                                id:            innerCircle
                                width:         15 //parent.width / 2
                                height:        width
                                radius:        width / 2
                                color:         "transparent"
                                anchors.centerIn: parent
                                border.width:     1
                                border.color:     borderColor
                            }

                            QGCIconButton {
                                id:                       rotateUp
                                anchors.top:              parent.top
                                anchors.horizontalCenter: parent.horizontalCenter
                                iconSource:               "/qmlimages/ArrowUp.svg"
                                highlighted:              hovered
                                width:                    ScreenTools.implicitIconButtonHeight * 2
                                onClicked: {
                                    console.log("pod rotate up")
                                    _gcu.adjustPitch(-1.0)
                                }
                            }
                            QGCIconButton {
                                id:                       rotateDown
                                anchors.bottom:           parent.bottom
                                anchors.horizontalCenter: parent.horizontalCenter
                                iconSource:               "/qmlimages/ArrowUp.svg"
                                highlighted:              hovered
                                width:                    ScreenTools.implicitIconButtonHeight * 2
                                rotation:                 180
                                onClicked: {
                                    console.log("pod rotate down")
                                    _gcu.adjustPitch(1.0)
                                }
                            }
                            QGCIconButton {
                                id:                       rotateLeft
                                anchors.left:             parent.left
                                anchors.verticalCenter:   parent.verticalCenter
                                iconSource:               "/qmlimages/ArrowUp.svg"
                                highlighted:              hovered
                                width:                    ScreenTools.implicitIconButtonHeight * 2
                                rotation:                 -90
                                onClicked: {
                                    console.log("pod rotate left")
                                    _gcu.adjustYaw(-1)
                                }
                            }
                            QGCIconButton {
                                id:                       rotateRight
                                anchors.right:            parent.right
                                anchors.verticalCenter:   parent.verticalCenter
                                iconSource:               "/qmlimages/ArrowUp.svg"
                                highlighted:              hovered
                                width:                    ScreenTools.implicitIconButtonHeight * 2
                                rotation:                 90
                                onClicked: {
                                    console.log("pod rotate right")
                                    _gcu.adjustYaw(1)
                                }
                            }
                            QGCLabel {
                                id:                        labelGimbalYaw
                                anchors.top:               parent.bottom
                                anchors.topMargin:         _margins
                                anchors.horizontalCenter:  parent.horizontalCenter
                                text:                      _gcu.yaw
                            }
                            QGCLabel {
                                id:                        labelGimbalPitch
                                anchors.left:              parent.right
                                anchors.leftMargin:        _margins
                                anchors.verticalCenter:    parent.verticalCenter
                                text:                      _gcu.pitch
                            }
                        }

                        Rectangle {
                            id:     zoomBorder
                            width:  ScreenTools.implicitIconButtonHeight * 1.5//parent.width * 0.2
                            height: outerCircle.height
                            anchors.right: parent.right
                            anchors.rightMargin: 5
                            anchors.verticalCenter: parent.verticalCenter
                            color:                  "transparent"
                            radius:                 width / 2
                            border.width:           1
                            border.color:           borderColor
                            QGCIconButton {
                                id:                       cameraZoomIn
                                anchors.top:              parent.top
                                anchors.horizontalCenter: parent.horizontalCenter
                                iconSource:               "/qmlimages/Plus.svg"
                                highlighted:              hovered
                                width:                    ScreenTools.implicitIconButtonHeight * 2
                                onPressed: {
                                    console.log("zoom in")
                                    _gcu.setZoom(1)
                                }
                                onReleased: {
                                    console.log("zoom in stop")
                                    _gcu.setZoom(0)
                                }
                            }
                            QGCLabel {
                                id:           labelZoom
                                anchors.centerIn: parent
                                text:         _gcu.zoom
                            }

                            QGCIconButton {
                                id:                       cameraZoomOut
                                anchors.bottom:           parent.bottom
                                anchors.horizontalCenter: parent.horizontalCenter
                                iconSource:               "/qmlimages/Minus.svg"
                                highlighted:              hovered
                                width:                    ScreenTools.implicitIconButtonHeight * 2
                                onPressed: {
                                    console.log("zoom out")
                                    _gcu.setZoom(-1)
                                }
                                onReleased: {
                                    console.log("zoom out stop")
                                    _gcu.setZoom(-1)
                                }
                            }
                        }
                    }
                }

                Column {
                    Layout.fillHeight:  true
                    Layout.fillWidth:   true
                    Layout.preferredWidth:  parent.width / 3
                    QGCLabel {
                        text:     qsTr("固定角度")
                        anchors.horizontalCenter: parent.horizontalCenter
                    }

                    Rectangle {
                        color:                  "transparent"
                        radius:                 10
                        border.width:           1
                        border.color:           borderColor
                        anchors.horizontalCenter: parent.horizontalCenter
                        width:                    parent.width
                        height:                   150
                        Row {
                            id:               rowGimbalPitch
                            anchors.centerIn: parent
                            QGCLabel {
                                text:    qsTr("俯仰")
                                anchors.verticalCenter:  parent.verticalCenter
                            }
                            QGCTextField {
                                id:              textFieldPitch
                                width:           40
                                text:            "0"
                                placeholderText: qsTr("俯仰角")
                                anchors.verticalCenter:  parent.verticalCenter
                                onTextChanged: {
                                    console.log("pitch changed: " + text)
                                }
                            }
                        }
                        QGCButton {
                            anchors.bottom:  rowGimbalPitch.top
                            anchors.bottomMargin: _margins
                            anchors.horizontalCenter: parent.horizontalCenter
                            text:      qsTr("前视")
                            heightFactor: 0.1
                            width:        70
                            onClicked: {
                                var pitch = parseFloat(textFieldPitch.text)
                                if (isNaN(pitch))
                                    pitch = 0
                                _gcu.setDirection(0, pitch)
                            }
                        }
                        QGCButton {
                            anchors.top:         rowGimbalPitch.bottom
                            anchors.topMargin:   _margins
                            anchors.horizontalCenter: parent.horizontalCenter
                            text:      qsTr("后视")
                            heightFactor: 0.1
                            width:        70
                            onClicked: {
                                var pitch = parseFloat(textFieldPitch.text)
                                if (isNaN(pitch))
                                    pitch = 0
                                _gcu.setDirection(1, pitch)
                            }
                        }
                        QGCButton {
                            anchors.horizontalCenter: rowGimbalPitch.left
                            anchors.horizontalCenterOffset: -20
                            anchors.verticalCenter: parent.verticalCenter
                            text:      qsTr("左视")
                            rotation:  -90
                            heightFactor: 0.1
                            width:        70
                            onClicked: {
                                var pitch = parseFloat(textFieldPitch.text)
                                if (isNaN(pitch))
                                    pitch = 0
                                _gcu.setDirection(2, pitch)
                            }
                        }
                        QGCButton {
                            anchors.horizontalCenter: rowGimbalPitch.right
                            anchors.horizontalCenterOffset: 20
                            anchors.verticalCenter:  parent.verticalCenter
                            text:      qsTr("右视")
                            rotation:  -90
                            heightFactor: 0.1
                            width:        70
                            onClicked: {
                                var pitch = parseFloat(textFieldPitch.text)
                                if (isNaN(pitch))
                                    pitch = 0
                                _gcu.setDirection(3, pitch)
                            }
                        }
                    }
                }
            }
        }
        Rectangle {
            id:           rectSplitLine1
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top:   itemPod.bottom
            anchors.topMargin: 10
            height:            1
            color:             borderColor // qgcPal.text
        }
        Item {   // 信号灯
            id:                itemSignalLight
            anchors.left:      parent.left
            anchors.right:     parent.right
            anchors.top:       rectSplitLine1.bottom
            anchors.topMargin: 5
            height:            230
            Column {
                anchors.fill: parent
                QGCLabel {
                    anchors.horizontalCenter:    parent.horizontalCenter
                    text:           qsTr("信号灯")
                    font.pointSize: 14
                }
                RowLayout {
                    anchors.left:   parent.left
                    anchors.right:  parent.right
                    anchors.leftMargin:    itemSignalLight.width / 5
                    anchors.rightMargin:   itemSignalLight.width / 5
                    spacing:               itemSignalLight.width / 5
                    height:         200
                    ColumnLayout {
                        Layout.fillHeight:  true
                        Layout.alignment:   Qt.AlignTop

                        QGCLabel {
                            Layout.alignment: Qt.AlignHCenter
                            text: qsTr("倒计时：") + "20s"
                            font.pointSize:   12
                        }
                        Rectangle {
                            id:            signalLightCircle
                            color:         _signalLightColor
                            width:         100
                            height:        width
                            radius:        width / 2
                            Layout.alignment: Qt.AlignHCenter
                        }

                        Row {
                            Layout.fillWidth:  true
                            Layout.alignment: Qt.AlignHCenter
                            QGCRadioButton {
                                text:           qsTr("手动")
                                checked:        true
                                anchors.verticalCenter: parent.verticalCenter

                            }
                            QGCRadioButton {
                                text:             qsTr("自动")
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillHeight:  true
                        Layout.alignment:   Qt.AlignTop
                        QGCLabel {
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignHCenter
                            text:             qsTr("颜色设置")
                            font.pointSize:   12
                        }

                        Item {
                            width: 220
                            height: 150
                            Layout.alignment: Qt.AlignLeft
                            Row {
                                Column {
                                    spacing:    20
                                    QGCRadioButton {
                                        text:             qsTr("红灯")
                                        onClicked: { _signalLightColor = "red" }
                                        // anchors.verticalCenter: parent.verticalCenter
                                    }
                                    QGCRadioButton {
                                        text:             qsTr("绿灯")
                                        checked:          true
                                        onClicked: { _signalLightColor = "green" }
                                        // anchors.verticalCenter: parent.verticalCenter
                                    }
                                    QGCRadioButton {
                                        text:             qsTr("黄灯")
                                        onClicked: { _signalLightColor = "yellow" }
                                        // anchors.verticalCenter: parent.verticalCenter
                                    }
                                }
                                Column {
                                    spacing:    20
                                    QGCTextField {
                                        id:              redLightTime
                                        width:           60
                                        text:            "45"
                                        showUnits:        true
                                        unitsLabel:      qsTr("秒")
                                        onTextChanged: {
                                            console.log("pitch changed: " + text)
                                        }
                                    }
                                    QGCTextField {
                                        id:              greenLightTime
                                        width:           60
                                        text:            "45"
                                        // anchors.verticalCenter:  parent.verticalCenter
                                        showUnits:        true
                                        unitsLabel:      qsTr("秒")
                                        onTextChanged: {
                                            console.log("pitch changed: " + text)
                                        }
                                    }
                                    QGCTextField {
                                        id:              yellowLightTime
                                        width:           60
                                        text:            "3"
                                        // anchors.verticalCenter:  parent.verticalCenter
                                        showUnits:        true
                                        unitsLabel:      qsTr("秒")
                                        onTextChanged: {
                                            console.log("pitch changed: " + text)
                                        }
                                    }
                                }
                            }
                        }

                    }

                }
            }
        }

        Rectangle {
            id:           rectSplitLine2
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top:   itemSignalLight.bottom
            anchors.topMargin: 10
            height:            1
            color:             borderColor // qgcPal.text
        }
        Item  {
            id:                itemSpeaker
            anchors.left:      parent.left
            anchors.right:     parent.right
            anchors.top:       rectSplitLine2.bottom
            anchors.topMargin: 5
            height:            200
            Column {
                anchors.fill: parent
                QGCLabel {
                    anchors.horizontalCenter:    parent.horizontalCenter
                    text:           qsTr("喊话器")
                    font.pointSize: 14
                }
                RowLayout {
                    anchors.left:   parent.left
                    anchors.right:  parent.right
                    anchors.leftMargin:    itemSpeaker.width / 10
                    anchors.rightMargin:   itemSpeaker.width / 10
                    spacing:               itemSpeaker.width / 10
                    height:         200
                    Column {
                        Layout.alignment: Qt.AlignHCenter
                        spacing: 10
                        QGCColoredImage {
                            anchors.horizontalCenter:  parent.horizontalCenter
                            source:     "/qmlimages/Mic.svg"
                            width: 60
                            height: width
                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    parent.checked = !parent.checked
                                }
                            }
                            color: checked ? qgcPal.buttonHighlight : "white"
                        }
                        QGCLabel {
                            anchors.horizontalCenter:  parent.horizontalCenter
                            text: qsTr("按下讲话")
                            font.pointSize: 12
                        }
                    }
                    Column {
                        Layout.alignment: Qt.AlignHCenter
                        spacing: 10
                        QGCColoredImage {
                            anchors.horizontalCenter:  parent.horizontalCenter
                            source:     "/qmlimages/Speaker.svg"
                            width: 60
                            height: width
                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    parent.checked = !parent.checked
                                }
                            }
                            color: checked ? qgcPal.buttonHighlight : "white"
                        }
                        QGCLabel {
                            anchors.horizontalCenter:  parent.horizontalCenter
                            text: qsTr("点击图标播放")
                            font.pointSize: 12
                        }
                    }
                    Column {
                        Layout.alignment: Qt.AlignHCenter
                        spacing:  5
                        QGCLabel {
                            text: qsTr("循环播放")
                            // font.pointSize: 12
                        }
                        ButtonGroup { id:  speakerLoopPlay  }

                        Row {
                            QGCRadioButton {
                                text:             qsTr("播放次数")
                                checked:          true
                                anchors.verticalCenter: parent.verticalCenter
                                ButtonGroup.group: speakerLoopPlay
                            }
                            QGCTextField {
                                width:           60
                                text:            "30"
                                anchors.verticalCenter:  parent.verticalCenter
                                showUnits:        true
                                unitsLabel:      qsTr("次")
                                onTextChanged: {
                                    console.log("循环播放次数: " + text)
                                }
                            }
                        }
                        QGCRadioButton {
                            text:             qsTr("无限循环")
                            // anchors.verticalCenter: parent.verticalCenter
                            ButtonGroup.group: speakerLoopPlay
                        }
                        Rectangle {
                            id:           rectSplitLine3
                            anchors.left:  parent.left
                            anchors.right: parent.right
                            anchors.topMargin: 10
                            height:            1
                            color:             borderColor // qgcPal.text
                        }
                        Row {
                            QGCLabel {
                                text: qsTr("音频:") + _selectedMp3File
                                width:   120
                                // clip:    true
                                elide:   Text.ElideRight
                                anchors.verticalCenter:  parent.verticalCenter
                            }
                            QGCIconButton {
                                // anchors.bottom:           parent.bottom
                                // anchors.horizontalCenter: parent.horizontalCenter
                                iconSource:               "/InstrumentValueIcons/OpenFolder.svg"
                                highlighted:              hovered
                                width:                    ScreenTools.implicitIconButtonHeight
                                anchors.verticalCenter:  parent.verticalCenter
                                onClicked: {
                                    fileDialog.open()
                                }
                            }
                            FileDialog {
                                id: fileDialog
                                nameFilters: ["Mp3 (*.mp3)"]
                                folder: Qt.labs.platform.StandardPaths.standardLocations(Qt.labs.platform.StandardPaths.DocumentsLocation)[0]//StandardPaths.standardLocations(StandardPaths.DocumentsLocation)[0]
                                onAccepted: {
                                    // 查找最后一个路径分隔符的位置
                                    var fileName = file.toString()
                                    _selectedMp3File = fileName.substring(fileName.lastIndexOf("/") + 1)
                                }
                            }
                        }
                        ScrollView {
                            anchors.left:   parent.left
                            anchors.right:  parent.right

                            height:             ScreenTools.defaultFontPixelHeight * 3
                            TextArea {
                                id:                 textToSpeech
                                font.pointSize:     ScreenTools.defaultFontPointSize
                                text:               qsTr("速度提不上来，\n开慢车道上去！")
                                // enabled:            !_disableDataPersistence
                                color:              qgcPal.textFieldText
                                background:         Rectangle { color: qgcPal.textField }
                            }
                        }
                    }
                }
            }
        }
    }

    // Item {
    //     id:                 mapHolder
    //     anchors.top:        parent.top  //toolbar.bottom
    //     anchors.bottom:     parent.bottom
    //     anchors.left:       leftPanel.right
    //     anchors.right:      parent.right
    // }

    Item {
        id:                 mapHolder
        anchors.top:        parent.top  //toolbar.bottom
        anchors.bottom:     parent.bottom
        anchors.left:       leftPanel.right
        anchors.right:      parent.right

        property bool pipView2Clicked: false

        FlyViewMap {
            id:                     mapControl
            planMasterController:   _planController
            rightPanelWidth:        ScreenTools.defaultFontPixelHeight * 9
            pipView:                _pipView
            pipMode:                !_mainWindowIsMap
            toolInsets:             customOverlay.totalToolInsets
            mapName:                "FlightDisplayView"
            enabled:                !viewer3DWindow.isOpen
        }

        FlyViewVideo {
            id:         videoControl
            pipView:    _pipView
            useVideoSource2: false
            HorizontalFactValueGrid {
                id:                     valueArea
                width:                  380
                height:                 50
                anchors.bottom:         parent.bottom
                anchors.bottomMargin:   2
                anchors.horizontalCenter: parent.horizontalCenter
                settingsGroup:          telemetryBarSettingsGroup
                specificVehicleForCard: null
            }
            PhotoVideoControl {
                anchors.verticalCenter:  parent.verticalCenter
                anchors.right:           parent.right
                width:                   50
                z:                       QGroundControl.zOrderWidgets + 1
            }
        }
        // PipView {
        //     id:                     _pipView
        //     anchors.left:           parent.left
        //     anchors.top:            parent.top
        //     item1IsFullSettingsKey: "MainFlyWindowIsMap"
        //     item1:                  mapControl
        //     item2:                  videoControl
        //     _pipFullParent:         mapHolder
        //     show:                   true
        //     z:                      QGroundControl.zOrderWidgets

        //     property real leftEdgeBottomInset: visible ? width + anchors.margins : 0
        //     property real bottomEdgeLeftInset: visible ? height + anchors.margins : 0
        // }

        FlyViewWidgetLayer {
            id:                     widgetLayer
            anchors.top:            parent.top
            anchors.bottom:         parent.bottom
            anchors.left:           parent.left
            anchors.right:          guidedValueSlider.visible ? guidedValueSlider.left : parent.right
            z:                      _fullItemZorder + 2 // we need to add one extra layer for map 3d viewer (normally was 1)
            parentToolInsets:       _toolInsets
            mapControl:             _mapControl
            visible:                !QGroundControl.videoManager.fullScreen
            utmspActTrigger:        utmspSendActTrigger
            isViewer3DOpen:         viewer3DWindow.isOpen
        }
        // Rectangle {
        //     anchors.fill: widgetLayer
        //     border.color: "yellow"
        //     border.width: 2
        // }
        FlyViewCustomLayer {
            id:                 customOverlay
            anchors.fill:       widgetLayer
            z:                  _fullItemZorder + 2
            parentToolInsets:   widgetLayer.totalToolInsets
            mapControl:         _mapControl
            visible:            !QGroundControl.videoManager.fullScreen
        }

        // Development tool for visualizing the insets for a paticular layer, show if needed
        FlyViewInsetViewer {
            id:                     widgetLayerInsetViewer
            anchors.top:            parent.top
            anchors.bottom:         parent.bottom
            anchors.left:           parent.left
            anchors.right:          guidedValueSlider.visible ? guidedValueSlider.left : parent.right
            z:                      widgetLayer.z + 1
            insetsToView:           widgetLayer.totalToolInsets
            visible:                false
        }

        GuidedActionsController {
            id:                 guidedActionsController
            missionController:  _missionController
            guidedValueSlider:     _guidedValueSlider
        }

        //-- Guided value slider (e.g. altitude)
        GuidedValueSlider {
            id:                 guidedValueSlider
            anchors.right:      parent.right
            anchors.top:        parent.top
            anchors.bottom:     parent.bottom
            z:                  QGroundControl.zOrderTopMost
            visible:            false
        }

        Viewer3D{
            id:                     viewer3DWindow
            anchors.fill:           parent
        }
    }
}
