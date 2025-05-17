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
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Palette
import QGroundControl.Controls
import QGroundControl.ScreenTools

Rectangle {
    id:     settingsView
    color:  qgcPal.window
    z:      QGroundControl.zOrderTopMost
    anchors.fill: parent
    readonly property real _defaultTextHeight:  ScreenTools.defaultFontPixelHeight
    readonly property real _defaultTextWidth:   ScreenTools.defaultFontPixelWidth
    readonly property real _horizontalMargin:   _defaultTextWidth / 2
    readonly property real _verticalMargin:     _defaultTextHeight / 2
    readonly property real _largeMargin:        _defaultTextHeight
    readonly property real _largeFontSize:      ScreenTools.defaultFontPointSize * 2
    readonly property real _smallFontSize:      ScreenTools.defaultFontPointSize * 1.2
    readonly property real _buttonHeight:       ScreenTools.isTinyScreen ? ScreenTools.defaultFontPixelHeight * 3 : ScreenTools.defaultFontPixelHeight * 2
    StackLayout {
        id:           mainLayout
        anchors.fill: parent
        currentIndex: 0
        /******************** 用户登录 ********************/
        Rectangle {
            color: "transparent"
            border.width:  1
            border.color:  qgcPal.groupBorder
            implicitWidth:         400
            implicitHeight:        400
            radius:                30
            anchors.centerIn:  parent
            Layout.fillHeight: false
            Layout.fillWidth:  false
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 20
                QGCLabel {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: 20
                    text:     qsTr("用户登录")
                    font.pointSize: ScreenTools.defaultFontPointSize * 2
                    font.bold:      true
                }
                Rectangle {
                    Layout.fillWidth: true
                    height:           1
                    color:            qgcPal.text
                }
                RowLayout {
                    spacing: 20
                    Layout.fillWidth: true
                    QGCLabel {
                        text: qsTr("账号:")
                        font.pointSize: ScreenTools.defaultFontPointSize * 1.5
                        Layout.fillWidth: false
                    }
                    QGCTextField {
                        width: 300
                        placeholderText : qsTr("手机号")
                        Layout.fillWidth: true
                        text:             "15138986689"
                    }
                }
                RowLayout {
                    spacing: 20
                    Layout.fillWidth: true
                    QGCLabel {
                        text: qsTr("密码:")
                        font.pointSize: ScreenTools.defaultFontPointSize * 1.5
                    }
                    QGCTextField {
                        width: 300
                        placeholderText : qsTr("6位以上")
                        Layout.fillWidth: true
                        echoMode: TextInput.Password
                        text:     "123456789"
                    }
                }

                RowLayout {
                    spacing: 20
                    height: 40
                    Layout.fillWidth: true
                    QGCLabel {
                        text: qsTr("验证码:")
                        font.pointSize: ScreenTools.defaultFontPointSize * 1.5
                    }
                    QGCTextField {
                        id:                     vrCodeTextEdit
                        width: 300
                        placeholderText : qsTr("请输入手机验证码")
                        Layout.fillWidth: true
                    }
                    Rectangle {
                        width:     80
                        height:    vrCodeTextEdit.height
                        color:     qgcPal.toolbarBackground
                        QGCLabel {
                            id:            vrCode
                            anchors.centerIn:   parent
                            text:    Math.floor(Math.random() * 10000)
                            font.pointSize: ScreenTools.defaultFontPointSize * 1.5
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                vrCode.text = Math.floor(Math.random() * 10000)
                            }
                        }
                    }
                }
                RowLayout {
                    Layout.alignment:  Qt.AlignHCenter
                    QGCCheckBox {
                        Layout.alignment: Qt.AlignRight
                        text:     qsTr("记住我")
                        checked:  true
                        textFontPointSize: ScreenTools.defaultFontPointSize
                    }
                    QGCLabel {
                        text: qsTr("没有账号？")
                        Layout.leftMargin: 20
                        font.pointSize: ScreenTools.defaultFontPointSize
                    }
                    QGCLabel {
                        text: qsTr("点击注册")
                        color: qgcPal.colorBlue
                        font.pointSize: ScreenTools.defaultFontPointSize
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                mainLayout.currentIndex = 1
                                console.log("clicked")
                            }
                        }
                    }
                }
                QGCButton {
                    // width:  300
                    text:   qsTr("登录")
                    // Layout.alignment: Qt.AlignHCenter
                    Layout.fillWidth: true
                    Layout.leftMargin: 50
                    Layout.rightMargin: 50
                    font.pointSize: ScreenTools.defaultFontPointSize * 1.5
                    onClicked: {
                        mainLayout.currentIndex = 2
                    }
                }
            }
        }
        /******************** 用户注册 ********************/
        Rectangle {
            color: "transparent"
            border.width:  1
            border.color:  qgcPal.groupBorder
            implicitWidth:         400
            implicitHeight:        400
            radius:                30
            anchors.centerIn:  parent
            Layout.fillHeight: false
            Layout.fillWidth:  false
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 20
                QGCLabel {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: 20
                    text:     qsTr("用户注册")
                    font.pointSize: ScreenTools.defaultFontPointSize * 2
                    font.bold:      true
                }
                Rectangle {
                    Layout.fillWidth: true
                    height:           1
                    color:            qgcPal.text
                }
                RowLayout {
                    spacing: 20
                    Layout.fillWidth: true
                    QGCLabel {
                        text: qsTr("账号:")
                        font.pointSize: ScreenTools.defaultFontPointSize * 1.5
                        Layout.fillWidth: false
                    }
                    QGCTextField {
                        width: 300
                        placeholderText : qsTr("手机号")
                        Layout.fillWidth: true
                    }
                }
                RowLayout {
                    spacing: 20
                    Layout.fillWidth: true
                    QGCLabel {
                        text: qsTr("密码:")
                        font.pointSize: ScreenTools.defaultFontPointSize * 1.5
                    }
                    QGCTextField {
                        width: 300
                        placeholderText : qsTr("6位以上")
                        Layout.fillWidth: true
                    }
                }
                RowLayout {
                    spacing: 20
                    Layout.fillWidth: true
                    QGCLabel {
                        text: qsTr("再次输入密码:")
                        font.pointSize: ScreenTools.defaultFontPointSize * 1.5
                    }
                    QGCTextField {
                        width: 300
                        placeholderText : qsTr("6位以上")
                        Layout.fillWidth: true
                    }
                }

                RowLayout {
                    spacing: 20
                    height: 40
                    Layout.fillWidth: true
                    QGCLabel {
                        text: qsTr("验证码:")
                        font.pointSize: ScreenTools.defaultFontPointSize * 1.5
                    }
                    QGCTextField {
                        id:                     vrCodeTextEditRegister
                        width: 300
                        placeholderText : qsTr("请输入手机验证码")
                        Layout.fillWidth: true
                    }
                    Rectangle {
                        width:     80
                        height:    vrCodeTextEditRegister.height
                        color:     qgcPal.toolbarBackground
                        QGCLabel {
                            id:            vrCodeRegister
                            anchors.centerIn:   parent
                            text:    Math.floor(Math.random() * 10000)
                            font.pointSize: ScreenTools.defaultFontPointSize * 1.5
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                vrCodeRegister.text = Math.floor(Math.random() * 10000)
                            }
                        }
                    }
                }

                QGCButton {
                    // width:  300
                    text:   qsTr("注册")
                    // Layout.alignment: Qt.AlignHCenter
                    Layout.fillWidth: true
                    Layout.leftMargin: 50
                    Layout.rightMargin: 50
                    font.pointSize: ScreenTools.defaultFontPointSize * 1.5
                    onClicked: {
                        mainLayout.currentIndex = 0
                    }
                }
            }
        }

        /******************** 用户界面 ********************/
        Item {
            anchors.fill: parent
            Rectangle {
                id:       leftPanel
                anchors.left: parent.left
                anchors.top:  parent.top
                anchors.bottom: parent.bottom
                width:          330
                color:        qgcPal.window
                ColumnLayout {
                    anchors.fill:          parent
                    // anchors.topMargin:     _largeMargin / 2
                    anchors.bottomMargin:  _largeMargin
                    RowLayout {
                        Layout.fillWidth:   true
                        Layout.leftMargin:  _largeMargin
                        Layout.rightMargin: _largeMargin
                        Layout.preferredHeight: _largeMargin * 5
                        spacing:            _largeMargin
                        Image {
                            source: "/res/TrafficPolice.png"
                            width:  50
                            height: width
                        }
                        QGCLabel {
                            text:   qsTr("张晨")
                            font.bold: true
                            font.pointSize: _largeFontSize
                        }
                        Item {
                            Layout.fillWidth: true
                        }

                        QGCLabel {
                            text: qsTr("一级飞手")
                            font.pointSize: _smallFontSize
                        }
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        height:           1
                        color:            qgcPal.groupBorder
                    }
                    RowLayout {
                        Layout.fillWidth:   true
                        Layout.leftMargin:  _largeMargin
                        Layout.rightMargin: _largeMargin
                        spacing:            _largeMargin
                        ColumnLayout {
                            Layout.fillHeight:  true
                            Layout.fillWidth:   true
                            Layout.preferredWidth: parent.width / 3
                            QGCLabel {
                                text: qsTr("飞行时长")
                                font.pointSize: _smallFontSize
                                Layout.alignment: Qt.AlignHCenter
                            }
                            QGCLabel {
                                text: "5:45:23"
                                font.bold:      true
                                font.pointSize: _largeFontSize
                                Layout.alignment: Qt.AlignHCenter
                            }
                            // Rectangle{
                            //     color: "red"
                            //     Layout.alignment: Qt.AlignHCenter
                            //     Layout.fillWidth:   true
                            //     height:          20
                            // }

                        }
                        ColumnLayout {
                            Layout.fillHeight: true
                            Layout.fillWidth:   true
                            Layout.preferredWidth: parent.width / 3
                            QGCLabel {
                                text: qsTr("飞行里程")
                                font.pointSize:  _smallFontSize
                                Layout.alignment: Qt.AlignHCenter
                            }
                            QGCLabel {
                                text: "1233Km"
                                font.bold:      true
                                font.pointSize: _largeFontSize
                                Layout.alignment: Qt.AlignHCenter
                            }
                            // Rectangle{
                            //     color: "red"
                            //     Layout.alignment: Qt.AlignHCenter
                            //     Layout.fillWidth:   true
                            //     height:          20
                            // }
                        }

                        ColumnLayout {
                            Layout.fillHeight:  true
                            Layout.fillWidth:   true
                            Layout.preferredWidth: parent.width / 3
                            QGCLabel {
                                text: qsTr("飞行次数")
                                font.pointSize: _smallFontSize
                                Layout.alignment: Qt.AlignHCenter
                            }
                            QGCLabel {
                                text: "55"
                                font.bold:      true
                                font.pointSize: _largeFontSize
                                Layout.alignment: Qt.AlignHCenter
                            }
                            // Rectangle{
                            //     color: "red"
                            //     Layout.alignment: Qt.AlignHCenter
                            //     Layout.fillWidth:   true
                            //     height:          20
                            // }
                        }
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        height:           1
                        color:            qgcPal.groupBorder
                    }
                    RowLayout {
                        Layout.fillWidth:   true
                        Layout.leftMargin:  _largeMargin
                        Layout.rightMargin: _largeMargin
                        // spacing:            _largeMargin
                        ColumnLayout {
                            Layout.fillHeight: true
                            Layout.fillWidth:   true
                            Layout.preferredWidth: parent.width / 3
                            QGCLabel {
                                text: qsTr("警情处理")
                                font.pointSize: _smallFontSize
                                Layout.alignment: Qt.AlignHCenter
                            }
                            QGCLabel {
                                text: qsTr("34次")
                                font.bold:      true
                                font.pointSize: _largeFontSize
                                Layout.alignment: Qt.AlignHCenter
                            }
                        }
                        ColumnLayout {
                            Layout.fillHeight: true
                            Layout.fillWidth:   true
                            Layout.preferredWidth: parent.width / 3
                            QGCLabel {
                                text: qsTr("拍摄照片")
                                font.pointSize: _smallFontSize
                                Layout.alignment: Qt.AlignHCenter
                            }
                            QGCLabel {
                                text: qsTr("1234张")
                                font.bold:      true
                                font.pointSize: _largeFontSize
                                Layout.alignment: Qt.AlignHCenter
                            }

                        }
                        ColumnLayout {
                            Layout.fillHeight: true
                            Layout.fillWidth:   true
                            Layout.preferredWidth: parent.width / 3
                            QGCLabel {
                                text: qsTr("录像")
                                font.pointSize: _smallFontSize
                                Layout.alignment: Qt.AlignHCenter
                            }
                            QGCLabel {
                                text: qsTr("32段")
                                font.bold:      true
                                font.pointSize: _largeFontSize
                                Layout.alignment: Qt.AlignHCenter
                            }
                        }
                    }
                    Item {
                        Layout.fillHeight: true
                    }
                    ColumnLayout {
                        Layout.fillWidth:   true
                        Layout.leftMargin:  _largeMargin * 2
                        Layout.rightMargin: _largeMargin * 2
                        spacing:            _largeMargin
                        QGCButton {
                            text:   qsTr("当前任务")
                            Layout.alignment: Qt.AlignHCenter
                            Layout.fillWidth: true
                            font.pointSize: ScreenTools.defaultFontPointSize
                            onClicked: stackLayoutMission.currentIndex = 0
                        }
                        QGCButton {
                            text:   qsTr("今日任务")
                            Layout.alignment: Qt.AlignHCenter
                            Layout.fillWidth: true
                            font.pointSize: ScreenTools.defaultFontPointSize
                            onClicked: stackLayoutMission.currentIndex = 1
                        }
                        QGCButton {
                            text:   qsTr("已完成任务")
                            Layout.alignment: Qt.AlignHCenter
                            Layout.fillWidth: true
                            font.pointSize: ScreenTools.defaultFontPointSize
                            onClicked: stackLayoutMission.currentIndex = 2
                        }
                    }
                }
            } // leftPanel
            Rectangle {
                id:           verticleSpliter1
                anchors.left: leftPanel.right
                anchors.top:  parent.top
                anchors.bottom: parent.bottom
                width:          1
                color:          qgcPal.groupBorder
            }
            // 任务面板
            Item {
                id:   missionPanel
                anchors.left:   verticleSpliter1.right
                anchors.top:    parent.top
                anchors.bottom: parent.bottom
                width:          460
                StackLayout {
                    id:             stackLayoutMission
                    anchors.fill:   parent
                    ColumnLayout {
                        anchors.fill: parent
                        // Layout.fillHeight: true
                        // Layout.fillWidth:  true
                        QGCLabel {
                            text: qsTr("当前任务")
                            font.pointSize: _largeFontSize
                            font.bold:      true
                            Layout.alignment: Qt.AlignCenter
                            Layout.preferredHeight: _largeMargin * 5
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment:   Text.AlignVCenter
                        }
                        Rectangle {
                            height:          1
                            color:          qgcPal.groupBorder
                            Layout.fillWidth: true
                        }
                    }


                    ColumnLayout {
                        anchors.fill: parent
                        QGCLabel {
                            text: qsTr("今日任务")
                            font.pointSize: _largeFontSize
                            font.bold:      true
                            Layout.alignment: Qt.AlignCenter
                            Layout.preferredHeight: _largeMargin * 5
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment:   Text.AlignVCenter
                        }
                        Rectangle {
                            height:          1
                            color:          qgcPal.groupBorder
                            Layout.fillWidth: true
                        }
                    }


                    ColumnLayout {
                        anchors.fill: parent
                        QGCLabel {
                            text: qsTr("已完成任务")
                            font.pointSize: _largeFontSize
                            font.bold:      true
                            Layout.alignment: Qt.AlignCenter
                            Layout.preferredHeight: _largeMargin * 5
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment:   Text.AlignVCenter
                        }
                        Rectangle {
                            height:          1
                            color:          qgcPal.groupBorder
                            Layout.fillWidth: true
                        }
                    }

                }

            }// missionPanel任务面板
            Rectangle {
                id:           verticleSpliter2
                anchors.left: missionPanel.right
                anchors.top:  parent.top
                anchors.bottom: parent.bottom
                width:          1
                color:          qgcPal.groupBorder
            }
            // 任务详情面板
            Item {
                id:             detailMissionPanel
                anchors.top:    parent.top
                anchors.bottom: parent.bottom
                anchors.left:   verticleSpliter2.right
                anchors.right:  parent.right

                ColumnLayout {
                    anchors.fill: parent
                    QGCLabel {
                        text: qsTr("任务详情")
                        font.pointSize: _largeFontSize
                        font.bold:      true
                        Layout.alignment: Qt.AlignCenter
                        Layout.preferredHeight: _largeMargin * 5
                        // Layout.fillWidth:       true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment:   Text.AlignVCenter
                    }
                    Rectangle {
                        height:          1
                        color:          qgcPal.groupBorder
                        Layout.fillWidth: true
                    }
                    QGCLabel {
                        text: qsTr("任务描述：")
                        font.pointSize: _largeFontSize
                        font.bold:      true
                        Layout.alignment: Qt.AlignLeft
                        Layout.leftMargin: _largeMargin
                        Layout.rightMargin: _largeMargin
                    }
                    ScrollView {
                        Layout.leftMargin:  _largeMargin
                        Layout.rightMargin: _largeMargin
                        Layout.fillWidth:   true
                        Layout.preferredHeight:  200
                        TextArea {
                            // scrollBarPolicy:    Qt.ScrollBarAsNeeded
                            wrapMode:           Text.WordWrap
                            text: "2024.12.34，我单位指挥中心接到报警，称在 阜新高速有一辆拉蔬菜的运输车发生侧翻。"
                            font.pointSize:     _smallFontSize
                            color:              qgcPal.text
                            background:         Rectangle { color: qgcPal.windowShadeDark }
                        }
                    }
                    Rectangle {
                        height:          1
                        color:          qgcPal.groupBorder
                        Layout.fillWidth: true
                    }
                    QGCLabel {
                        text: qsTr("处理结果：")
                        font.pointSize: _largeFontSize
                        font.bold:      true
                        Layout.alignment: Qt.AlignLeft
                        Layout.leftMargin: _largeMargin
                        Layout.rightMargin: _largeMargin
                    }
                    ScrollView {
                        // Layout.alignment:   Qt.AlignRight
                        Layout.leftMargin:  _largeMargin
                        Layout.rightMargin: _largeMargin
                        Layout.fillWidth:   true
                        Layout.preferredHeight: 200
                        TextArea {
                            wrapMode: Text.WordWrap
                            text: "接警后，我们迅速启动应急处置预案，除了安排警力赶赴现场外，还立即调度无人机前往事故区域。
    无人机率先抵达现场，通过高清摄像头回传的画面，我们清晰看到一辆满载蔬菜的重型货车侧翻在道路右侧，车辆严重受损，车上装载的蔬菜散落一地，占据了部分车道，现场一片狼藉。同时，车流量较大，后方车辆已经开始出现拥堵迹象
    通过无人机的高空视野，我们精准掌握了现场情况，立即通过无人机搭载的高音喇叭进行远程指挥。一方面，告知驾驶员保持冷静，确认其受伤情况，得知驾驶员仅受轻伤后，指导他做好安全防护措施，撤离到安全地带等待救援；另一方面，引导后方车辆有序减速慢行，提前变更车道，避免造成交通堵塞和二次事故。
    在地面警力到达现场后，无人机与现场交警紧密配合，继续发挥作用。无人机在空中实时监测交通状况，为现场交警疏导交通提供全面、准确的信息，确保救援车辆能够快速、顺利抵达现场。同时，利用无人机喊话功能，提醒围观群众不要靠近事故现场，保障自身安全。
    现场救援工作随即展开，我们联系了清障车和吊车对事故车辆进行起吊和拖移。在清理过程中，无人机还协助交警对散落的蔬菜进行整理规划，指挥现场人员有序收集，尽量减少车主的损失。
    经初步调查，事故原因是驾驶员在行驶过程中疲劳驾驶，导致车辆失控发生侧翻。后续我们将进一步对事故进行深入调查，并根据调查结果依法作出处理。"
                            font.pointSize:     _smallFontSize
                            color:              qgcPal.text
                            background:         Rectangle { color: qgcPal.windowShadeDark }
                        }
                    }
                    Rectangle {
                        height:          1
                        color:          qgcPal.groupBorder
                        Layout.fillWidth: true
                    }
                    QGCLabel {
                        text: qsTr("现场记录：")
                        font.pointSize: _largeFontSize
                        font.bold:      true
                        Layout.alignment: Qt.AlignLeft
                        Layout.leftMargin: _largeMargin
                        Layout.rightMargin: _largeMargin
                    }
                    GridLayout {
                        id: tileLayout
                        Layout.leftMargin:  _largeMargin
                        Layout.rightMargin: _largeMargin
                        Layout.fillWidth:   true
                        Layout.preferredHeight: 200
                        columns:            4             // 设置列数为 4
                        rowSpacing:         _largeMargin  // 行间距
                        columnSpacing:      _largeMargin  // 列间距
                        property var mediaList: [
                            "/CapturedPictures/1.jpg",
                            "/CapturedPictures/2.jpg",
                            "/CapturedPictures/5.jpg",
                            "/CapturedPictures/6.jpg",
                            "/CapturedPictures/3.png",
                            "/CapturedPictures/4.png"
                            ]
                        Image {
                            source: "/CapturedPictures/4.png"
                            fillMode: Image.PreserveAspectFit
                        }
                        Image {
                            source: "/CapturedPictures/1.jpg"
                            fillMode: Image.PreserveAspectFit
                        }

                        Repeater {
                            model: mediaList
                            delegate: Item {
                                width: parent.width / tileLayout.columns - tileLayout.columnSpacing
                                height: width
                                Image {
                                    visible: modelData.endsWith(".jpg") || modelData.endsWith(".png")
                                    source:  modelData
                                    fillMode: Image.PreserveAspectFit
                                }
                            }
                        }
                    }
                }
            }

        }
    }
}

