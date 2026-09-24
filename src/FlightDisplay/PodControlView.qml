import QtLocation
import QtPositioning
import QtQuick.Window
import QtQml.Models
import Qt.labs.platform 1.1
import QtQuick.Layouts

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

Item {    // 吊舱的设置界面
    height:                     270
    // property var    _gcu:              QGroundControl.videoManager.gcu
    property var    _inyyoA102Pro:  QGroundControl.videoManager.inyyoA102Pro
    property bool   pointTrackEnabled:  pointTrackCheckBoxSlider.checked
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

        ColumnLayout {
            Layout.fillHeight:  true
            Layout.fillWidth:   true
            Layout.preferredWidth:  parent.width / 2

            QGCLabel {
                text:     qsTr("吊舱控制 ")
                Layout.alignment:  Qt.AlignHCenter
            }

            Rectangle {
                color:                  "transparent"
                radius:                 10
                border.width:           1
                border.color:           borderColor
                Layout.fillWidth:         true
                Layout.fillHeight:        true
                // Rectangle {
                //     anchors.fill: gridPodControl
                //     color: "red"
                // }

                RowLayout {
                    id: rowAngles
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 30
                    anchors.margins: 10
                    // spacing: 5
                    QGCLabel {
                        Layout.fillWidth: true
                        Layout.preferredWidth: 65
                        text: qsTr("横滚:") + _inyyoA102Pro.podRoll.toFixed(1)
                    }
                    QGCLabel {
                        Layout.fillWidth: true
                        Layout.preferredWidth: 65
                        text: qsTr("俯仰:") + _inyyoA102Pro.podPitch.toFixed(1)
                    }
                    QGCLabel {
                        Layout.fillWidth: true
                        Layout.preferredWidth: 65
                        text: qsTr("偏航:") + _inyyoA102Pro.podYaw.toFixed(1)
                    }
                }

                GridLayout {
                    id:                gridPodControl
                    anchors.top:       rowAngles.bottom
                    // anchors.bottom:    parent.bottom
                    anchors.left:      parent.left
                    anchors.right:     parent.right
                    anchors.margins:   10//_margins * 2
                    columns:            3
                    rowSpacing:         16
                    columnSpacing:      5


                    // 第一行
                    QGCLabel {
                        Layout.alignment: Qt.AlignLeft
                        text: qsTr("偏航:")
                    }

                    QGCSlider {
                        id: sliderYawRate
                        Layout.fillWidth: true
                        from: -0xff
                        to:   0xff
                        stepSize: 1
                        onValueChanged: {
                            if (pressed) {
                                // _gcu.rotatePod(0, value)
                                _inyyoA102Pro.yawRotate(value < 0, Math.abs(value))
                                console.log("Yaw value:", value, value < 0)
                            }
                        }
                        PropertyAnimation {
                            id: centerAnimYaw
                            target: sliderYawRate
                            property: "value"
                            to: (sliderYawRate.from + sliderYawRate.to) / 2
                            duration:  100
                            easing.type: Easing.OutQuart
                        }
                        onPressedChanged: {
                            if (!pressed && sliderYawRate.value !== (sliderYawRate.from + sliderYawRate.to) / 2) {
                                // _gcu.rotatePod(0, 0)
                                _inyyoA102Pro.stopRoate()
                                centerAnimYaw.start()
                            }
                        }
                    }
                    QGCLabel {
                        Layout.preferredWidth: 20
                        Layout.alignment: Qt.AlignRight
                        text: Math.round(sliderYawRate.value)
                    }

                    // 第二行
                    QGCLabel {
                        Layout.alignment: Qt.AlignLeft
                        text: qsTr("俯仰:")
                    }

                    QGCSlider {
                        id: sliderPitchRate
                        Layout.fillWidth: true
                        from: -150
                        to:   150
                        stepSize: 1
                        onValueChanged: {
                            if (pressed) {
                                // _gcu.rotatePod(1, value)
                                _inyyoA102Pro.pitchRotate(value < 0, Math.abs(value))
                            }
                        }

                        PropertyAnimation {
                            id: centerAnimPitch
                            target: sliderPitchRate
                            property: "value"
                            to: (sliderPitchRate.from + sliderPitchRate.to) / 2
                            duration:  100
                            easing.type: Easing.OutQuart
                        }
                        onPressedChanged: {
                            if (!pressed && sliderPitchRate.value !== (sliderPitchRate.from + sliderPitchRate.to) / 2) {
                                // _gcu.rotatePod(1, 0)
                                _inyyoA102Pro.stopRoate()
                                centerAnimPitch.start()
                            }
                        }
                    }
                    QGCLabel {
                        Layout.preferredWidth: 20
                        Layout.alignment: Qt.AlignRight
                        text: Math.round(sliderPitchRate.value)
                    }

                    // 第三行
                    QGCLabel {
                        Layout.alignment: Qt.AlignLeft
                        text: qsTr("可见光:")
                    }

                    QGCSlider {
                        id: sliderVisualZoom
                        Layout.fillWidth: true
                        from: -8
                        to:   8
                        stepSize: 1
                        onValueChanged: {
                            if (pressed) {
                                // _gcu.rotatePod(1, value)
                                _inyyoA102Pro.zoomChange(value > 0, Math.abs(value))
                            }
                        }

                        PropertyAnimation {
                            id: centerAnimVisualZoom
                            target: sliderVisualZoom
                            property: "value"
                            to: (sliderVisualZoom.from + sliderVisualZoom.to) / 2
                            duration:  100
                            easing.type: Easing.OutQuart
                        }
                        onPressedChanged: {
                            if (!pressed && sliderVisualZoom.value !== (sliderVisualZoom.from + sliderVisualZoom.to) / 2) {
                                // _gcu.rotatePod(1, 0)
                                _inyyoA102Pro.zoomStop()
                                centerAnimVisualZoom.start()
                            }
                        }
                    }
                    QGCLabel {
                        Layout.preferredWidth: 20
                        Layout.alignment: Qt.AlignRight
                        text: Math.round(sliderVisualZoom.value)
                    }

                    // 第四行
                    QGCLabel {
                        Layout.alignment: Qt.AlignLeft
                        text: qsTr("红   外:")
                    }

                    QGCSlider {
                        id: sliderInfraredZoom
                        Layout.fillWidth: true
                        from: 1
                        to:   8
                        stepSize: 1
                        onValueChanged: {
                            if (pressed) {
                                // _gcu.rotatePod(1, value)
                                _inyyoA102Pro.thermalZoom(value - 1)
                            }
                        }
                    }
                    QGCLabel {
                        Layout.preferredWidth: 20
                        Layout.alignment: Qt.AlignRight
                        text: Math.round(sliderInfraredZoom.value)
                    }
                }

                RowLayout {
                    anchors.top: gridPodControl.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 30
                    anchors.margins: 10
                    // spacing: 5
                    QGCButton{
                        Layout.fillWidth:   true
                        text: qsTr("一键向下")
                        height:             ScreenTools.defaultFontPixelHeight
                        heightFactor: 0.1
                        onClicked: {
                            _inyyoA102Pro.lookDown()
                        }
                    }
                    QGCButton{
                        Layout.fillWidth:   true
                        text: qsTr("一键回中")
                        height:             ScreenTools.defaultFontPixelHeight
                        heightFactor: 0.1
                        onClicked: {
                            _inyyoA102Pro.lookForward()
                        }
                    }
                }


                // Rectangle {
//     anchors.fill: columnPodRate
//     color: "red"
// }

//                 StackLayout {
//                     id: stackLayoutPodControl
//                     anchors.left:  parent.left
//                     anchors.right: parent.right
//                     anchors.top:   rowAngles.bottom
//                     anchors.bottom: parent.bottom
//                     anchors.margins: 5
//                     anchors.topMargin: 10
//                     //角度模式:0x10, 欧拉角模式:0x14,FPV模式:0x1c
//                     //指向锁定:0x11,指向跟随:0x12,俯拍模式:0x13,凝视模式:0x16, 跟踪模式:0x17,
//                     property bool angleMode: _gcu.podMode === 0x10 ||  _gcu.podMode === 0x14 ||  _gcu.podMode === 0x1c
//                     currentIndex: angleMode ? 1 : 0



// /*
//                     ColumnLayout {
//                         id: columnPodAngle
//                         anchors.fill: parent
//                         anchors.margins: 5

//                         RowLayout {
//                             Layout.fillHeight: true
//                             Layout.fillWidth: true
//                             QGCLabel {
//                                 text: qsTr("偏航:")
//                             }
//                             QGCTextField {
//                                 id: textFieldYawAngle
//                                 Layout.fillWidth: true
//                                 placeholderText: "-180~180"
//                                 validator: DoubleValidator {
//                                                         bottom: -180        // 最小值
//                                                         top: 180           // 最大值
//                                                         decimals: 1        // 最多1位小数
//                                                         locale: Qt.locale("C")
//                                                     }
//                                 onAccepted: {
//                                     _gcu.setPodAngle(0, parseFloat(text))
//                                     focus = false
//                                 }
//                                 Connections {
//                                     id: connectionsYawAngle
//                                     target: _gcu
//                                     enabled: !textFieldYawAngle.focus
//                                     onYawChanged: {
//                                         textFieldYawAngle.text = _gcu.yaw.toFixed(1)
//                                     }
//                                 }
//                             }
//                         }
//                         RowLayout {
//                             Layout.fillHeight: true
//                             Layout.fillWidth: true
//                             QGCLabel {
//                                 text: qsTr("俯仰:")
//                             }
//                             QGCTextField {
//                                 id: textFieldPitchAngle
//                                 Layout.fillWidth: true
//                                 placeholderText: "-90~90"
//                                 validator: DoubleValidator {
//                                     bottom: -90        // 最小值
//                                     top: 90            // 最大值
//                                     decimals: 1        // 最多1位小数
//                                     locale: Qt.locale("C")
//                                 }
//                                 onAccepted: {
//                                     _gcu.setPodAngle(1, parseFloat(text))
//                                     focus = false
//                                 }
//                                 Connections {
//                                     id: connectionsPitchAngle
//                                     target: _gcu
//                                     enabled: !textFieldPitchAngle.focus
//                                     onPitchChanged: {
//                                         textFieldPitchAngle.text = _gcu.pitch.toFixed(1)
//                                     }
//                                 }
//                             }
//                         }
//                         RowLayout {
//                             Layout.fillHeight: true
//                             Layout.fillWidth: true
//                             QGCLabel {
//                                 text: qsTr("横滚:")
//                             }
//                             QGCTextField {
//                                 id: textFieldRollAngle
//                                 Layout.fillWidth: true
//                                 placeholderText: "-90~90"
//                                 validator: DoubleValidator {
//                                     bottom: -90        // 最小值
//                                     top: 90            // 最大值
//                                     decimals: 1        // 最多1位小数
//                                     locale: Qt.locale("C")
//                                 }
//                                 onAccepted: {
//                                     _gcu.setPodAngle(2, parseFloat(text))
//                                     focus = false
//                                 }
//                                 Connections {
//                                     id: connectionsRollAngle
//                                     enabled: !textFieldRollAngle.focus
//                                     target: _gcu
//                                     onRollChanged: {
//                                         textFieldRollAngle.text = _gcu.roll.toFixed(1)
//                                     }
//                                 }
//                             }
//                         }

//                     }
// */
//                 }



/*
                Rectangle {       // 外圆
                    id:            outerCircle
                    width:         ScreenTools.implicitIconButtonHeight * 6//parent.width * 0.5
                    height:        width
                    radius:        width / 2
                    color:         "transparent"
                    border.width:           1
                    border.color:           borderColor
                    anchors.left:           parent.left
                    anchors.leftMargin:     _margins * 2
                    anchors.top:            parent.top
                    anchors.topMargin:      _margins * 2
                    // anchors.verticalCenter: parent.verticalCenter
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
                        onPressed: {
                            console.log("pod rotate up")
                            _gcu.rotatePod(1, -1)
                        }
                        onReleased: {
                            _gcu.rotatePod(1, 0)
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
                        onPressed: {
                            console.log("pod rotate down")
                            _gcu.rotatePod(1, 1)
                        }
                        onReleased: {
                            _gcu.rotatePod(1, 0)
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
                        onPressed: {
                            console.log("pod rotate left")
                            _gcu.rotatePod(0, -1)
                        }
                        onReleased: {
                            _gcu.rotatePod(0, 0)
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
                        onPressed: {
                            console.log("pod rotate right")
                            _gcu.rotatePod(0, 1)
                        }
                        onReleased: {
                            _gcu.rotatePod(0, 0)
                        }
                    }
                    QGCLabel {
                        id:                        labelGimbalPitch
                        anchors.top:               parent.bottom
                        anchors.topMargin:         _margins
                        anchors.horizontalCenter:  parent.horizontalCenter
                        text:                      _gcu.pitch.toFixed(1)
                    }
                    QGCLabel {
                        id:                        labelGimbalyaw
                        anchors.left:              parent.right
                        anchors.leftMargin:        _margins
                        anchors.verticalCenter:    parent.verticalCenter
                        text:                      _gcu.yaw.toFixed(1)
                    }
                }

                Rectangle {
                    id:     zoomBorder
                    width:  ScreenTools.implicitIconButtonHeight * 1.5//parent.width * 0.2
                    height: outerCircle.height
                    anchors.right: parent.right
                    anchors.rightMargin: _margins * 2
                    // anchors.verticalCenter: parent.verticalCenter
                    anchors.top:            parent.top
                    anchors.topMargin:      _margins * 2
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
                            _gcu.continuousZoom(1)
                        }
                        onReleased: {
                            console.log("zoom in stop")
                            _gcu.continuousZoom(0)
                        }
                    }
                    QGCLabel {
                        id:           labelZoom
                        anchors.centerIn: parent
                        text:         _gcu.zoom.toFixed(1)
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
                            _gcu.continuousZoom(-1)
                        }
                        onReleased: {
                            console.log("zoom out stop")
                            _gcu.continuousZoom(0)
                        }
                    }
                }
*/

            }

        }

        ColumnLayout {
            Layout.fillHeight:  true
            Layout.fillWidth:   true
            Layout.preferredWidth:  parent.width / 2
            QGCLabel {
                text:     qsTr("型号: ")// + _gcu.cameraName
                Layout.alignment:  Qt.AlignHCenter
            }

            Rectangle {
                color:                    "transparent"
                radius:                   10
                border.width:             1
                border.color:             borderColor
                width:                    parent.width
                Layout.fillWidth:         true
                Layout.fillHeight:        true
                GridLayout {
                    id:                 itemPodSettingsGridLayout
                    anchors.top:        parent.top
                    anchors.left:       parent.left
                    anchors.right:      parent.right
                    anchors.margins:    10
                    anchors.topMargin:  16
                    columns:            2
                    rowSpacing:         16
                    columnSpacing:      20
                    QGCCheckBoxSlider {
                        id:                 osdFollowVehicleSlider
                        Layout.fillWidth:   true
                        text:               qsTr("跟随载具")
                        visible:            true
                        checked:            true
                        onCheckedChanged: {
                            _inyyoA102Pro.followPlatform(checked)
                            console.log("跟随载具:", checked)
                        }
                    }

                    QGCCheckBoxSlider {
                        id:                 osdCheckBoxSlider
                        Layout.fillWidth:   true
                        text:               qsTr("OSD")
                        visible:            true
                        checked:            true
                        onCheckedChanged: {
                            _inyyoA102Pro.osd(checked)
                            console.log("osd chedked:", checked)
                        }
                    }

                    // false默认为指点平移，true为指点跟踪
                    QGCCheckBoxSlider {
                        id:                 pointTrackCheckBoxSlider
                        Layout.fillWidth:   true
                        text:               qsTr("指点跟踪")
                        visible:            true
                        onCheckedChanged: {
                            // _gcu.setOsd(checked)
                            if (!checked) {
                                _inyyoA102Pro.stopTrackToPoint()
                            }
                            console.log("指点跟踪 chedked:", checked)
                        }
                    }

                    QGCCheckBoxSlider {
                        id:                 targetDetectCheckBoxSlider
                        Layout.fillWidth:   true
                        text:               qsTr("目标检测")
                        visible:            true
                        onCheckedChanged: {
                            _inyyoA102Pro.targetDetect(checked)
                            console.log("targetDetect chedked:", checked)
                        }
                    }

                }

                GridLayout{
                    anchors.top:        itemPodSettingsGridLayout.bottom
                    anchors.left:       parent.left
                    anchors.right:      parent.right
                    // anchors.margins:    _margins * 2
                    anchors.margins:    10
                    anchors.topMargin:  16
                    columns:            2
                    rowSpacing:         16
                    columnSpacing:      20

                    QGCLabel {
                        text: qsTr("画中画")
                    }

                    QGCComboBox {
                        Layout.fillWidth:   true
                        // text:               qsTr("画中画")
                        // Layout.columnSpan:  2
                        height:             ScreenTools.defaultFontPixelHeight
                        model:  [qsTr("可见光包含热成像"), qsTr("热成像包含可见光"),qsTr("仅可见光"),qsTr("仅热成像")]
                        // sizeToContents: true
                        onCurrentIndexChanged: {
                            _inyyoA102Pro.pictureInPictureSwitch(currentIndex)
                            console.log("画中画", currentIndex)
                        }
                    }

                    QGCLabel {
                        text: qsTr("热成像")
                    }

                    QGCComboBox {
                        Layout.fillWidth:   true
                        // text:               qsTr("热成像")
                        // Layout.columnSpan:  2
                        height:             ScreenTools.defaultFontPixelHeight
                        model:  [qsTr("黑热"), qsTr("白热"),qsTr("彩色")]
                        // sizeToContents: true
                        onCurrentIndexChanged: {
                            _inyyoA102Pro.thermalModeSwitch(currentIndex)
                            console.log("热成像", currentIndex)
                        }
                    }

                    QGCButton {
                        Layout.fillWidth:   true
                        text:               qsTr("设置")
                        Layout.columnSpan:  2
                        height:             ScreenTools.defaultFontPixelHeight
                        heightFactor: 0.1

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
        /*
        ColumnLayout {
            Layout.fillHeight:  true
            Layout.fillWidth:   true
            Layout.preferredWidth:  parent.width / 3
            QGCLabel {
                text:     qsTr("相机")
                Layout.alignment:  Qt.AlignHCenter
            }

            Rectangle {
                color:                  "transparent"
                radius:                 10
                border.width:           1
                border.color:           borderColor
                Layout.fillWidth:         true
                Layout.fillHeight:        true
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
                    anchors.bottomMargin: _margins * 2
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
                    anchors.topMargin:   _margins * 2
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
                    anchors.horizontalCenterOffset: -20 -_margins * 2
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
                    anchors.horizontalCenterOffset: _margins * 2 + 20
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
    */
        }
    }
}
