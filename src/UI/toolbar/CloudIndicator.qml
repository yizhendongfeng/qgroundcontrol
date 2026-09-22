/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import QGroundControl
import QGroundControl.Controls
import QGroundControl.ScreenTools
import QGroundControl.Palette

// 上云 WebSocket（ws 组件）状态指示器
//   图标 + 在线设备数 / 告警数角标，点击弹出抽屉展示云平台推送的四类消息：
//   设备上下线、HMS 健康告警、任务/上传进度、原始消息日志。
Item {
    id:             control
    width:          cloudIndicatorRow.width
    anchors.top:    parent.top
    anchors.bottom: parent.bottom

    // StackLayout 的 implicitWidth/implicitHeight 取所有子页的最大值，
    // 抽屉宽度必须显式给定，否则切到内容最宽的 tab 时抽屉会被撑宽、切 tab 时跳动。
    readonly property real _drawerWidth:    Math.min(ScreenTools.defaultFontPixelWidth * 46, mainWindow.contentItem.width * 0.7)
    readonly property real _listHeight:     ScreenTools.defaultFontPixelHeight * 16
    readonly property real _rowMargin:      ScreenTools.defaultFontPixelHeight / 8
    // 工具栏有 3 倍字体高，云图标是实心块，铺满整条会显得过大
    readonly property real _iconSize:       ScreenTools.defaultFontPixelHeight * 1.5

    QGCPalette { id: qgcPal }

    Row {
        id:             cloudIndicatorRow
        anchors.top:    parent.top
        anchors.bottom: parent.bottom
        spacing:        ScreenTools.defaultFontPixelWidth / 2

        QGCColoredImage {
            id:                     cloudIcon
            width:                  control._iconSize
            height:                 control._iconSize
            anchors.verticalCenter: parent.verticalCenter
            source:                 "/InstrumentValueIcons/cloud.svg"
            fillMode:               Image.PreserveAspectFit
            sourceSize.height:      height
            // 未连接时压暗，一眼能看出掉线
            color:                  djiBridgeServer.cloudWsConnected ? qgcPal.buttonText : qgcPal.colorGrey
        }

        Column {
            anchors.verticalCenter: parent.verticalCenter
            spacing:                0

            QGCLabel {
                color:          qgcPal.buttonText
                font.pointSize: ScreenTools.smallFontPointSize
                text:           djiBridgeServer.cloudOnlineCount + "/" + djiBridgeServer.cloudDeviceCount
            }

            QGCLabel {
                color:          qgcPal.colorYellow
                font.pointSize: ScreenTools.smallFontPointSize
                visible:        djiBridgeServer.cloudHmsCount > 0
                text:           qsTr("HMS") + " " + djiBridgeServer.cloudHmsCount
            }
        }
    }

    MouseArea {
        anchors.fill:   parent
        onClicked:      mainWindow.showIndicatorDrawer(cloudDrawerComponent, control)
    }

    /// 地图元素列表的一行：色块 + 名称 + 类型/顶点数 + 「编辑」
    /// 线与面共用这一份，靠 object.type 区分文案
    Component {
        id: mapElementRowComponent

        Rectangle {
            id:             elementRowRect
            required property var object
            required property int index

            width:          parent ? parent.width : 0
            implicitHeight: elementRow.implicitHeight + (control._rowMargin * 2)
            color:          index % 2 ? qgcPal.windowShade : qgcPal.windowShadeLight

            RowLayout {
                id:                     elementRow
                anchors.left:           parent.left
                anchors.right:          parent.right
                anchors.top:            parent.top
                anchors.topMargin:      control._rowMargin
                spacing:                ScreenTools.defaultFontPixelWidth / 2

                Rectangle {
                    Layout.preferredWidth:  ScreenTools.defaultFontPixelHeight
                    Layout.preferredHeight: ScreenTools.defaultFontPixelHeight
                    radius:                 2
                    color:                  elementRowRect.object.color
                    border.width:           1
                    border.color:           qgcPal.text
                }

                QGCLabel {
                    Layout.fillWidth:   true
                    elide:              Text.ElideRight
                    font.pointSize:     ScreenTools.mediumFontPointSize
                    color:              qgcPal.text
                    text:               elementRowRect.object.name
                }

                QGCLabel {
                    font.pointSize:     ScreenTools.smallFontPointSize
                    color:              qgcPal.colorGrey
                    text: {
                        var element = elementRowRect.object
                        return (element.type === 2 ? qsTr("区域") : qsTr("线段")) + " · " + element.vertexCount + qsTr("点")
                    }
                }

                QGCButton {
                    _horizontalPadding: 0
                    text:               qsTr("编辑")
                    onClicked: {
                        // 先关抽屉再进编辑态：抽屉是 modal，标绘时点地图会先把它关掉
                        mainWindow.closeIndicatorDrawer()
                        djiBridgeServer.cloudMapBeginEdit(elementRowRect.object.id)
                    }
                }
            }
        }
    }

    Component {
        id: cloudDrawerComponent

        // 抽屉根必须是 ToolIndicatorPage：MainRootWindow 会往它上面绑定 expanded / 读取 showExpand
        // showExpand 保持默认 false —— 没有 expandedComponent，展开箭头会是个点不动的死按钮
        ToolIndicatorPage {
            contentComponent: Component {
                ColumnLayout {
                    spacing:                ScreenTools.defaultFontPixelHeight / 2
                    Layout.preferredWidth:  control._drawerWidth

                    LabelledLabel {
                        label:      qsTr("State")
                        labelText:  djiBridgeServer.cloudWsConnected ? qsTr("Connected") : qsTr("Disconnected")
                    }

                    LabelledLabel {
                        label:      qsTr("Endpoint")
                        labelText:  djiBridgeServer.cloudWsUrl.length > 0 ? djiBridgeServer.cloudWsUrl : qsTr("N/A")
                    }

                    LabelledLabel {
                        label:      qsTr("Devices Online")
                        labelText:  djiBridgeServer.cloudOnlineCount + " / " + djiBridgeServer.cloudDeviceCount
                    }

                    LabelledLabel {
                        label:      qsTr("Health Alerts")
                        labelText:  djiBridgeServer.cloudHmsCount
                    }

                    QGCTabBar {
                        id:                     cloudTabBar
                        Layout.alignment:       Qt.AlignHCenter
                        Layout.topMargin:       ScreenTools.defaultFontPixelHeight / 2

                        Repeater {
                            model: [ qsTr("Devices"), qsTr("Health"), qsTr("Progress"), qsTr("Messages"), qsTr("元素") ]

                            QGCTabButton {
                                text: modelData
                            }
                        }
                    }

                    StackLayout {
                        Layout.preferredWidth:  control._drawerWidth
                        Layout.preferredHeight: control._listHeight
                        currentIndex:           cloudTabBar.currentIndex

                        // ---------------- 设备 ----------------
                        Item {
                            ListView {
                                id:                 devicesList
                                anchors.fill:       parent
                                clip:               true
                                spacing:            1
                                boundsBehavior:     Flickable.StopAtBounds
                                model:              djiBridgeServer.cloudDevices

                                delegate: Rectangle {
                                    required property var object
                                    required property int index

                                    width:          devicesList.width
                                    implicitHeight: deviceColumn.implicitHeight + (control._rowMargin * 2)
                                    color:          index % 2 ? qgcPal.windowShade : qgcPal.windowShadeLight

                                    ColumnLayout {
                                        id:                     deviceColumn
                                        anchors.left:           parent.left
                                        anchors.right:          parent.right
                                        anchors.top:            parent.top
                                        anchors.topMargin:      control._rowMargin
                                        spacing:                0

                                        RowLayout {
                                            Layout.fillWidth:   true
                                            spacing:            ScreenTools.defaultFontPixelWidth / 2

                                            QGCLabel {
                                                font.pointSize: ScreenTools.smallFontPointSize
                                                color:          object.online ? qgcPal.colorGreen : qgcPal.colorGrey
                                                text:           object.online ? "●" : "○"
                                            }

                                            QGCLabel {
                                                Layout.fillWidth:   true
                                                elide:              Text.ElideMiddle
                                                font.pointSize:     ScreenTools.mediumFontPointSize
                                                color:              qgcPal.text
                                                text:               object.sn
                                            }

                                            QGCLabel {
                                                font.pointSize: ScreenTools.smallFontPointSize
                                                color:          qgcPal.colorGrey
                                                visible:        object.isLocal
                                                text:           qsTr("Local")
                                            }
                                        }

                                        RowLayout {
                                            Layout.fillWidth:   true
                                            spacing:            ScreenTools.defaultFontPixelWidth

                                            QGCLabel {
                                                Layout.fillWidth:   true
                                                elide:              Text.ElideRight
                                                font.pointSize:     ScreenTools.smallFontPointSize
                                                color:              qgcPal.text
                                                text: {
                                                    var info = object.info ? object.info : ({})
                                                    var deviceModel = info.model ? info.model : ""
                                                    var callsign = info.callsign ? info.callsign : ""
                                                    return (deviceModel + " " + callsign).trim()
                                                }
                                            }

                                            QGCLabel {
                                                font.pointSize: ScreenTools.smallFontPointSize
                                                color:          qgcPal.text
                                                text:           object.osd.batteryPercent !== undefined ? object.osd.batteryPercent + "%" : ""
                                            }

                                            QGCLabel {
                                                font.pointSize: ScreenTools.smallFontPointSize
                                                color:          qgcPal.text
                                                text:           object.osd.height !== undefined ? object.osd.height.toFixed(1) + " m" : ""
                                            }

                                            QGCLabel {
                                                font.pointSize: ScreenTools.smallFontPointSize
                                                color:          qgcPal.text
                                                text: {
                                                    if (object.osd.latitude === undefined || object.osd.longitude === undefined) {
                                                        return ""
                                                    }
                                                    return object.osd.latitude.toFixed(5) + ", " + object.osd.longitude.toFixed(5)
                                                }
                                            }

                                            QGCLabel {
                                                font.pointSize: ScreenTools.smallFontPointSize
                                                color:          qgcPal.colorGrey
                                                text:           object.lastSeen
                                            }
                                        }
                                    }
                                }
                            }

                            QGCLabel {
                                anchors.centerIn:   parent
                                color:              qgcPal.text
                                visible:            devicesList.count === 0
                                text:               qsTr("No device reported yet")
                            }
                        }

                        // ---------------- 健康告警 ----------------
                        ColumnLayout {
                            spacing: 0

                            QGCButton {
                                Layout.fillWidth:   true
                                text:               qsTr("Clear Health Alerts")
                                enabled:            djiBridgeServer.cloudHmsCount > 0
                                onClicked:          djiBridgeServer.clearCloudHms()
                            }

                            Item {
                                Layout.fillWidth:   true
                                Layout.fillHeight:  true

                                ListView {
                                    id:                 hmsList
                                    anchors.fill:       parent
                                    clip:               true
                                    spacing:            1
                                    boundsBehavior:     Flickable.StopAtBounds
                                    model:              djiBridgeServer.cloudHms

                                    delegate: Rectangle {
                                        required property var object
                                        required property int index

                                        width:          hmsList.width
                                        implicitHeight: hmsColumn.implicitHeight + (control._rowMargin * 2)
                                        color:          index % 2 ? qgcPal.windowShade : qgcPal.windowShadeLight

                                        ColumnLayout {
                                            id:                     hmsColumn
                                            anchors.left:           parent.left
                                            anchors.right:          parent.right
                                            anchors.top:            parent.top
                                            anchors.topMargin:      control._rowMargin
                                            spacing:                0

                                            RowLayout {
                                                Layout.fillWidth: true

                                                QGCLabel {
                                                    font.pointSize: ScreenTools.smallFontPointSize
                                                    color:          object.level >= 2 ? qgcPal.colorRed
                                                                    : (object.level === 1 ? qgcPal.colorOrange : qgcPal.colorYellow)
                                                    text:           object.level >= 2 ? qsTr("WARN")
                                                                    : (object.level === 1 ? qsTr("CAUTION") : qsTr("NOTICE"))
                                                }

                                                QGCLabel {
                                                    Layout.fillWidth:   true
                                                    elide:              Text.ElideMiddle
                                                    font.pointSize:     ScreenTools.smallFontPointSize
                                                    color:              qgcPal.text
                                                    text:               object.sn
                                                }

                                                QGCLabel {
                                                    font.pointSize: ScreenTools.smallFontPointSize
                                                    color:          qgcPal.colorGrey
                                                    text:           object.time
                                                }
                                            }

                                            QGCLabel {
                                                Layout.fillWidth:   true
                                                elide:              Text.ElideRight
                                                color:              qgcPal.text
                                                text:               object.text
                                            }

                                            QGCLabel {
                                                Layout.fillWidth:   true
                                                elide:              Text.ElideRight
                                                font.pointSize:     ScreenTools.smallFontPointSize
                                                color:              qgcPal.colorGrey
                                                visible:            object.code.length > 0
                                                text:               object.code
                                            }
                                        }
                                    }
                                }

                                QGCLabel {
                                    anchors.centerIn:   parent
                                    color:              qgcPal.text
                                    visible:            hmsList.count === 0
                                    text:               qsTr("No health alert")
                                }
                            }
                        }

                        // ---------------- 任务/上传进度 ----------------
                        Item {
                            ListView {
                                id:                 progressList
                                anchors.fill:       parent
                                clip:               true
                                spacing:            ScreenTools.defaultFontPixelHeight / 2
                                boundsBehavior:     Flickable.StopAtBounds
                                model:              djiBridgeServer.cloudProgress

                                delegate: Item {
                                    required property var object
                                    required property int index

                                    width:          progressList.width
                                    implicitHeight: progressColumn.implicitHeight

                                    ColumnLayout {
                                        id:             progressColumn
                                        anchors.left:   parent.left
                                        anchors.right:  parent.right
                                        anchors.top:    parent.top
                                        spacing:        control._rowMargin

                                        RowLayout {
                                            Layout.fillWidth: true
                                            spacing:          ScreenTools.defaultFontPixelWidth

                                            QGCLabel {
                                                Layout.fillWidth:   true
                                                elide:              Text.ElideRight
                                                color:              qgcPal.text
                                                text:               object.title
                                            }

                                            QGCLabel {
                                                color:  qgcPal.text
                                                text:   object.statusText
                                            }

                                            QGCLabel {
                                                color:  qgcPal.text
                                                text:   object.percent + "%"
                                            }
                                        }

                                        Rectangle {
                                            Layout.fillWidth:       true
                                            Layout.preferredHeight: ScreenTools.defaultFontPixelHeight / 2
                                            color:                  qgcPal.windowShade

                                            Rectangle {
                                                width:  parent.width * (object.percent / 100)
                                                height: parent.height
                                                color:  object.kind === "upload" ? qgcPal.colorBlue : qgcPal.colorGreen
                                            }
                                        }
                                    }
                                }
                            }

                            QGCLabel {
                                anchors.centerIn:   parent
                                color:              qgcPal.text
                                visible:            progressList.count === 0
                                text:               qsTr("No task or upload in progress")
                            }
                        }

                        // ---------------- 原始消息 ----------------
                        Item {
                            ListView {
                                id:                 messagesList
                                anchors.fill:       parent
                                clip:               true
                                spacing:            1
                                boundsBehavior:     Flickable.StopAtBounds
                                model:              djiBridgeServer.cloudMessages

                                delegate: Rectangle {
                                    required property var object
                                    required property int index

                                    width:          messagesList.width
                                    implicitHeight: messageRow.implicitHeight + (control._rowMargin * 2)
                                    color:          index % 2 ? qgcPal.windowShade : qgcPal.windowShadeLight

                                    RowLayout {
                                        id:                     messageRow
                                        anchors.left:           parent.left
                                        anchors.right:          parent.right
                                        anchors.top:            parent.top
                                        anchors.topMargin:      control._rowMargin
                                        spacing:                ScreenTools.defaultFontPixelWidth / 2

                                        QGCLabel {
                                            font.pointSize: ScreenTools.smallFontPointSize
                                            color:          qgcPal.colorGrey
                                            text:           object.time
                                        }

                                        QGCLabel {
                                            Layout.fillWidth:   true
                                            elide:              Text.ElideRight
                                            font.pointSize:     ScreenTools.smallFontPointSize
                                            color:              qgcPal.text
                                            text:               object.summary
                                        }
                                    }
                                }
                            }

                            QGCLabel {
                                anchors.centerIn:   parent
                                color:              qgcPal.text
                                visible:            messagesList.count === 0
                                text:               qsTr("No message received yet")
                            }
                        }

                        // ---------------- 地图元素 ----------------
                        // 这里只给列表和「打开编辑器」，不放绘制控件：
                        // 抽屉是 modal + CloseOnPressOutside，点地图加点会先把它关掉。
                        ColumnLayout {
                            spacing: 0

                            QGCLabel {
                                Layout.fillWidth:   true
                                wrapMode:           Text.WordWrap
                                font.pointSize:     ScreenTools.smallFontPointSize
                                color:              qgcPal.colorGrey
                                text:               qsTr("绘制请用飞行视图的「云元素」面板，或规划视图的「云元素」图层。")
                            }

                            QGCLabel {
                                Layout.fillWidth:   true
                                Layout.topMargin:   control._rowMargin
                                font.pointSize:     ScreenTools.smallFontPointSize
                                color:              qgcPal.text
                                text:               qsTr("线段") + " (" + djiBridgeServer.cloudMapLines.count + ")"
                            }

                            ListView {
                                id:                     linesElementList
                                Layout.fillWidth:       true
                                Layout.preferredHeight: Math.max(contentHeight, ScreenTools.defaultFontPixelHeight)
                                interactive:            false
                                clip:                   true
                                spacing:                1
                                boundsBehavior:         Flickable.StopAtBounds
                                model:                  djiBridgeServer.cloudMapLines
                                delegate:               mapElementRowComponent

                                QGCLabel {
                                    anchors.centerIn:   parent
                                    color:              qgcPal.colorGrey
                                    font.pointSize:     ScreenTools.smallFontPointSize
                                    visible:            linesElementList.count === 0
                                    text:               qsTr("暂无线段")
                                }
                            }

                            QGCLabel {
                                Layout.fillWidth:   true
                                Layout.topMargin:   control._rowMargin
                                font.pointSize:     ScreenTools.smallFontPointSize
                                color:              qgcPal.text
                                text:               qsTr("区域") + " (" + djiBridgeServer.cloudMapAreas.count + ")"
                            }

                            ListView {
                                id:                     areasElementList
                                Layout.fillWidth:       true
                                Layout.preferredHeight: Math.max(contentHeight, ScreenTools.defaultFontPixelHeight)
                                interactive:            false
                                clip:                   true
                                spacing:                1
                                boundsBehavior:         Flickable.StopAtBounds
                                model:                  djiBridgeServer.cloudMapAreas
                                delegate:               mapElementRowComponent

                                QGCLabel {
                                    anchors.centerIn:   parent
                                    color:              qgcPal.colorGrey
                                    font.pointSize:     ScreenTools.smallFontPointSize
                                    visible:            areasElementList.count === 0
                                    text:               qsTr("暂无区域")
                                }
                            }

                            Item { Layout.fillHeight: true }
                        }
                    }
                }
            }
        }
    }
}
