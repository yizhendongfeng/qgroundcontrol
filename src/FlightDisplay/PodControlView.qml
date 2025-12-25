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
    height:                     250
    property var    _gcu:              QGroundControl.videoManager.gcu
    property bool   pointMoveEnabled:  pointMoveCheckBoxSlider.checked
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
                        text: qsTr("横滚:") + _gcu.roll.toFixed(1)
                    }
                    QGCLabel {
                        Layout.fillWidth: true
                        Layout.preferredWidth: 65
                        text: qsTr("俯仰:") + _gcu.pitch.toFixed(1)
                    }
                    QGCLabel {
                        Layout.fillWidth: true
                        Layout.preferredWidth: 65
                        text: qsTr("偏航:") + _gcu.yaw.toFixed(1)
                    }
                }
// Rectangle {
//     anchors.fill: columnPodRate
//     color: "red"
// }

                StackLayout {
                    id: stackLayoutPodControl
                    anchors.left:  parent.left
                    anchors.right: parent.right
                    anchors.top:   rowAngles.botton
                    anchors.bottom: rowLayoutZoom.top
                    anchors.margins: 5
                    //角度模式:0x10, 欧拉角模式:0x14,FPV模式:0x1c
                    //指向锁定:0x11,指向跟随:0x12,俯拍模式:0x13,凝视模式:0x16, 跟踪模式:0x17,
                    property bool angleMode: _gcu.podMode === 0x10 ||  _gcu.podMode === 0x14 ||  _gcu.podMode === 0x1c
                    currentIndex: angleMode ? 1 : 0


                    ColumnLayout {
                        id: columnPodRate
                        anchors.fill: parent
                        anchors.margins: 5
                        anchors.topMargin: 30
                        spacing: 10
                        // Rectangle {
                        //     Layout.fillHeight: true
                        //     Layout.fillWidth: true
                        //     color: "red"
                        // }

                        // Item {
                        //     Layout.fillHeight: true
                        //     Layout.fillWidth: true
                        // }
                        RowLayout {
                            Layout.fillHeight: true
                            Layout.fillWidth:  true
                            Layout.minimumHeight: 30
                            // Layout.preferredHeight: 1
                            QGCLabel {
                                Layout.alignment: Qt.AlignLeft
                                text: qsTr("偏航:")
                            }

                            QGCSlider {
                                id: sliderYawRate
                                Layout.fillWidth: true
                                from: -150
                                to:   150
                                onValueChanged: {
                                    if (pressed) {
                                        _gcu.rotatePod(0, value)
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
                                        _gcu.rotatePod(0, 0)
                                        centerAnimYaw.start()
                                    }
                                }
                            }
                            QGCLabel {
                                Layout.preferredWidth: 30
                                Layout.alignment: Qt.AlignRight
                                text: Math.round(sliderYawRate.value)
                            }
                        }
                        RowLayout {
                            Layout.fillHeight: true
                            Layout.fillWidth:  true
                            Layout.minimumHeight: 30
                            // Layout.preferredHeight: 1
                            QGCLabel {
                                Layout.alignment: Qt.AlignLeft
                                text: qsTr("俯仰:")
                            }

                            QGCSlider {
                                id: sliderPitchRate
                                Layout.fillWidth: true
                                from: -150
                                to:   150
                                onValueChanged: {
                                    if (pressed) {
                                        _gcu.rotatePod(1, value)
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
                                        _gcu.rotatePod(1, 0)
                                        centerAnimPitch.start()
                                    }
                                }
                            }
                            QGCLabel {
                                Layout.preferredWidth: 30
                                Layout.alignment: Qt.AlignRight
                                text: Math.round(sliderPitchRate.value)
                            }
                        }

                    }

                    ColumnLayout {
                        id: columnPodAngle
                        anchors.fill: parent
                        anchors.margins: 5

                        RowLayout {
                            Layout.fillHeight: true
                            Layout.fillWidth: true
                            QGCLabel {
                                text: qsTr("偏航:")
                            }
                            QGCTextField {
                                id: textFieldYawAngle
                                Layout.fillWidth: true
                                placeholderText: "-180~180"
                                validator: DoubleValidator {
                                                        bottom: -180        // 最小值
                                                        top: 180           // 最大值
                                                        decimals: 1        // 最多1位小数
                                                        locale: Qt.locale("C")
                                                    }
                                onAccepted: {
                                    _gcu.setPodAngle(0, parseFloat(text))
                                    focus = false
                                }
                                Connections {
                                    id: connectionsYawAngle
                                    target: _gcu
                                    enabled: !textFieldYawAngle.focus
                                    onYawChanged: {
                                        textFieldYawAngle.text = _gcu.yaw.toFixed(1)
                                    }
                                }
                            }
                        }
                        RowLayout {
                            Layout.fillHeight: true
                            Layout.fillWidth: true
                            QGCLabel {
                                text: qsTr("俯仰:")
                            }
                            QGCTextField {
                                id: textFieldPitchAngle
                                Layout.fillWidth: true
                                placeholderText: "-90~90"
                                validator: DoubleValidator {
                                    bottom: -90        // 最小值
                                    top: 90            // 最大值
                                    decimals: 1        // 最多1位小数
                                    locale: Qt.locale("C")
                                }
                                onAccepted: {
                                    _gcu.setPodAngle(1, parseFloat(text))
                                    focus = false
                                }
                                Connections {
                                    id: connectionsPitchAngle
                                    target: _gcu
                                    enabled: !textFieldPitchAngle.focus
                                    onPitchChanged: {
                                        textFieldPitchAngle.text = _gcu.pitch.toFixed(1)
                                    }
                                }
                            }
                        }
                        RowLayout {
                            Layout.fillHeight: true
                            Layout.fillWidth: true
                            QGCLabel {
                                text: qsTr("横滚:")
                            }
                            QGCTextField {
                                id: textFieldRollAngle
                                Layout.fillWidth: true
                                placeholderText: "-90~90"
                                validator: DoubleValidator {
                                    bottom: -90        // 最小值
                                    top: 90            // 最大值
                                    decimals: 1        // 最多1位小数
                                    locale: Qt.locale("C")
                                }
                                onAccepted: {
                                    _gcu.setPodAngle(2, parseFloat(text))
                                    focus = false
                                }
                                Connections {
                                    id: connectionsRollAngle
                                    enabled: !textFieldRollAngle.focus
                                    target: _gcu
                                    onRollChanged: {
                                        textFieldRollAngle.text = _gcu.roll.toFixed(1)
                                    }
                                }
                            }
                        }

                    }

                    // Item {
                    //     Layout.fillWidth: true
                    //     Layout.fillHeight: true
                    //     id: podAngleControl

                    // }
                }

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

                RowLayout {
                    id: rowLayoutZoom
                    anchors.bottom: rowLayoutMode.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: _margins * 2
                    height: 20

                    QGCLabel {
                        Layout.alignment: Qt.AlignLeft
                        text: qsTr("缩放:")
                    }

                    QGCIconButton {
                        id:                       cameraZoomOut
                        // anchors.bottom:           parent.bottom
                        // anchors.horizontalCenter: parent.horizontalCenter
                        Layout.preferredHeight: ScreenTools.implicitIconButtonHeight
                        Layout.preferredWidth:  ScreenTools.implicitIconButtonHeight
                        iconSource:               "/qmlimages/Minus.svg"
                        highlighted:              hovered
                        width:                    ScreenTools.implicitIconButtonHeight * 2
                        onPressed: {
                            textFieldConnections.enabled = true
                            sliderConnections.enabled = true
                            console.log("zoom out")
                            _gcu.continuousZoom(-1)
                        }
                        onReleased: {
                            console.log("zoom out stop")
                            _gcu.continuousZoom(0)
                        }
                    }
                    QGCSlider {
                        id: sliderZoom
                        Layout.fillWidth: true
                        from: 1
                        to:   40
                        // value: _gcu.zoom * -10
                        onPressedChanged: {
                            if (!pressed) {
                                sliderConnections.enabled = false
                                textFieldConnections.enabled = true
                                _gcu.setZoom(value)
                            } else {
                                sliderConnections.enabled = false
                            }
                        }
                        Connections {
                            id: sliderConnections
                            // enabled: false
                            target: _gcu
                            onZoomChanged: {
                                sliderZoom.value = _gcu.zoom
                            }
                        }
                    }
                    QGCIconButton {
                        id:                       cameraZoomIn
                        // anchors.top:              parent.top
                        // anchors.horizontalCenter: parent.horizontalCenter
                        Layout.preferredHeight: ScreenTools.implicitIconButtonHeight
                        Layout.preferredWidth:  ScreenTools.implicitIconButtonHeight
                        iconSource:               "/qmlimages/Plus.svg"
                        highlighted:              hovered
                        width:                    ScreenTools.implicitIconButtonHeight * 2
                        onPressed: {
                            console.log("zoom in")
                            textFieldConnections.enabled = true
                            sliderConnections.enabled = true
                            _gcu.continuousZoom(1)
                        }
                        onReleased: {
                            console.log("zoom in stop")
                            _gcu.continuousZoom(0)
                        }
                    }
                    QGCTextField {
                        id:           textFieldZoomValue
                        Layout.preferredWidth: 35
                        Layout.preferredHeight: 25
                        validator: DoubleValidator {
                            bottom: 1.0        // 最小值0
                            top: 40.0          // 最大值100
                            decimals: 1        // 最多1位小数
                            locale: Qt.locale("C")
                        }
                        onAccepted: {
                            textFieldConnections.enabled = false
                            sliderConnections.enabled = true
                            _gcu.setZoom(parseFloat(text))
                        }
                        onFocusChanged: {
                            if (focus)
                                textFieldConnections.enabled = false
                        }

                        Connections {
                            id: textFieldConnections
                            // enabled: false
                            target: _gcu
                            onZoomChanged: {
                                textFieldZoomValue.text = _gcu.zoom.toFixed(1)
                            }
                        }
                    }
                    // Connections {
                    //     target: _gcu
                    //     onZoomChanged: {
                    //         enabled = false
                    //         textFieldConnections.enabled = false
                    //         sliderConnections.enabled = false
                    //     }
                    // }
                }

                RowLayout {
                    id: rowLayoutMode
                    // width: parent.width
                    anchors.bottom: parent.bottom
                    // anchors.horizontalCenter: parent.horizontalCenter
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: _margins * 2
                    spacing: _margins * 2
                    QGCLabel {
                        text: qsTr("模式:")
                    }
                    QGCComboBox {
                        id: podMode
                        textRole: "text"
                        valueRole: "value"
                        currentIndex: 2
                        // sizeToContents: true
                        height:        ScreenTools.defaultFontPixelHeight
                        Layout.fillWidth: true
                        model: [
                            { value: 0x10, text: qsTr("角度模式")},
                            { value: 0x11, text: qsTr("指向锁定")},
                            { value: 0x12, text: qsTr("指向跟随")},
                            { value: 0x13, text: qsTr("俯拍模式")},
                            { value: 0x14, text: qsTr("欧拉角模式")},
                            // { value: 0x16, text: qsTr("凝视模式")},
                            { value: 0x17, text: qsTr("跟踪模式")},
                            { value: 0x1c, text: qsTr("FPV模式")}
                        ]
                        onActivated:{
                            _gcu.setPodHeadMode(currentValue)
                            connectionMode.enabled = true
                            console.log("new pod mode:", index, currentValue)
                        }
                        Connections {
                            id: connectionMode
                            target: _gcu
                            onPodModeChanged: {
                                podMode.currentIndex = podMode.indexOfValue(_gcu.podMode)
                            }
                        }
                        onPressedChanged: {
                            console.log("mode pressed:", pressed)
                            connectionMode.enabled = false
                        }
                    }

                    // QGCRadioButton {
                    //     id: podFollow
                    //     text: qsTr("跟随")
                    //     Layout.alignment: Qt.AlignLeft
                    //     checked: _gcu.podMode === 0x12
                    //     onCheckedChanged: {
                    //         if (checked)
                    //             _gcu.setPodHeadMode(0x12)
                    //     }
                    // }
                    // QGCRadioButton {
                    //     id: podLock
                    //     text: qsTr("锁定")
                    //     Layout.alignment: Qt.AlignHCenter
                    //     checked: _gcu.podMode === 0x11
                    //     onCheckedChanged: {
                    //         if (checked)
                    //             _gcu.setPodHeadMode(0x11)
                    //     }
                    // }
                    // QGCRadioButton {
                    //     id: podAngle
                    //     text: qsTr("角度")
                    //     Layout.alignment: Qt.AlignRight
                    //     checked: _gcu.podMode === 0x10
                    //     onCheckedChanged: {
                    //         if (checked)
                    //             _gcu.setPodHeadMode(0x10)
                    //     }
                    // }

                }
            }

        }

        ColumnLayout {
            Layout.fillHeight:  true
            Layout.fillWidth:   true
            Layout.preferredWidth:  parent.width / 2
            QGCLabel {
                text:     qsTr("型号: ") + _gcu.cameraName
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
                ColumnLayout {
                    id:                 itemPodSettings
                    anchors.fill:       parent
                    anchors.margins:    _margins
                    RowLayout {
                        Layout.fillWidth:   true
                        QGCRadioButton {
                            text:           qsTr("可见光")
                            Layout.fillWidth:   true
                            Layout.leftMargin:  _margins
                            Layout.rightMargin: _margins
                            checked:        true
                            onClicked: {
                                _gcu.setVisualLight(true);
                            }
                        }
                        QGCRadioButton {
                            text:             qsTr("夜视")
                            Layout.fillWidth:   true
                            Layout.leftMargin:  _margins
                            Layout.rightMargin: _margins
                            checked:           _gcu.nightVision
                            onClicked: {
                                _gcu.setVisualLight(false);
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth:   true
                        QGCCheckBoxSlider {
                            id:                 useCheckList
                            Layout.fillWidth:   true
                            Layout.alignment:    Qt.AlignLeft
                            Layout.leftMargin:  _margins
                            Layout.rightMargin: _margins
                            text:               qsTr("补光")
                            visible:            true
                            checked:           _gcu.fillLight
                            onCheckedChanged: {
                                if (checked) {
                                    mainWindow.showMessageDialog(qsTr("警告"), qsTr("吊舱所搭载激光照明模块属于 Class 3B 类非可见光激光器，在照明模块开启状态下，严禁直接目视（≤ 12m）或使用光学仪器直接观察激光光束，照明模块前方 20cm 内严禁放置易燃物体。确定开启？"),
                                                                    MessageDialog.Yes | MessageDialog.Cancel,
                                                                    function() { _gcu.turnOnFillLight(255)},
                                                                    function() {checked = false})
                                } else {
                                    _gcu.turnOnFillLight(0)
                                }
                            }
                        }
                        QGCCheckBoxSlider {
                            id:                 osdCheckBoxSlider
                            Layout.fillWidth:   true
                            Layout.alignment:    Qt.AlignRight
                            // Layout.fillWidth:   true
                            Layout.leftMargin:  _margins
                            Layout.rightMargin: _margins
                            text:               qsTr("OSD")
                            visible:            true
                            onCheckedChanged: {
                                _gcu.setOsd(checked)
                                console.log("osd chedked:", checked)
                            }
                        }

                    }
                    QGCCheckBoxSlider {
                        id:                 pointMoveCheckBoxSlider
                        // Layout.fillWidth:   true
                        Layout.alignment:   Qt.AlignLeft
                        Layout.leftMargin:  _margins
                        Layout.rightMargin: _margins
                        Layout.preferredWidth: parent.height / 2
                        text:               qsTr("指点平移")
                        visible:            true
                        // onCheckedChanged: {
                        //     _gcu.setOsd(checked)
                        //     console.log("osd chedked:", checked)
                        // }
                    }


                    RowLayout {
                        Layout.fillWidth:   true
                        Layout.alignment: Qt.AlignBottom
                        Layout.bottomMargin: 5
                        QGCButton{
                            text: qsTr("校准")
                            Layout.fillWidth:   true
                            Layout.leftMargin:  _margins
                            Layout.rightMargin: _margins
                            height:             ScreenTools.defaultFontPixelHeight
                            heightFactor: 0.1
                            onClicked: {
                                mainWindow.showMessageDialog(qsTr("警告"), qsTr("校准过程中请保持静止，约15秒完成"),
                                                                MessageDialog.Ok,
                                                                function(){
                                                                    console.log("校准")
                                                                    _gcu.calibratePod()
                                                                })

                            }
                        }
                        QGCButton{
                            text: qsTr("回中")
                            Layout.fillWidth:   true
                            Layout.leftMargin:  _margins
                            Layout.rightMargin: _margins
                            height:             ScreenTools.defaultFontPixelHeight
                            heightFactor: 0.1
                            onClicked: {
                                _gcu.podCentering()
                            }
                        }


                    }
                    QGCButton {
                        text:               qsTr("设置")
                        Layout.fillWidth:   true
                        Layout.leftMargin:  _margins
                        Layout.rightMargin: _margins
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
