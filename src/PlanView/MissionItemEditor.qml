import QtQuick                      2.11
import QtQuick.Controls             2.4
import QtQuick.Controls.Styles      1.4
import QtQuick.Dialogs              1.2
import QtQml                        2.2
import QtQuick.Layouts              1.11

import QGroundControl               1.0
import QGroundControl.ScreenTools   1.0
import QGroundControl.Vehicle       1.0
import QGroundControl.Controls      1.0
import QGroundControl.FactControls  1.0
import QGroundControl.Palette       1.0


/// Mission item edit control
QGCViewDialog {
    id:             _root
//    color:          qgcPal.windowShade
//    radius:         _radius
    DeadMouseArea{
        anchors.fill: parent
    }

    property var    map                 ///< Map control
    property var    masterController
    property var    missionItem         ///< MissionItem associated with this editor
    property bool   readOnly            ///< true: read only view, false: full editing view

    signal clicked
    signal remove
    signal selectNextNotReadyItem

    property var    itemInfoSlot
    property var    _masterController:          masterController
    property var    _missionController:         _masterController.missionController
    property real   _sectionSpacer:             ScreenTools.defaultFontPixelWidth / 2  // spacing between section headings
    property bool   _singleComplexItem:         _missionController.complexMissionItemNames.length === 1

    readonly property real  _editFieldWidth:    Math.min(width - _innerMargin * 2, ScreenTools.defaultFontPixelWidth * 12)
    readonly property real  _margin:            ScreenTools.defaultFontPixelWidth / 2
    readonly property real  _innerMargin:       2
    readonly property real  _radius:            ScreenTools.defaultFontPixelWidth / 2
    readonly property real  _hamburgerSize:     ScreenTools.implicitButtonHeight * 0.75
    readonly property real  _trashSize:         ScreenTools.implicitButtonHeight * 0.75
    readonly property real  _iconSize:          ScreenTools.implicitButtonHeight
    readonly property bool  _waypointsOnlyMode: QGroundControl.corePlugin.options.missionWaypointsOnly

    QGCPalette {
        id: qgcPal
        colorGroupEnabled: enabled
    }
    ColumnLayout {
        anchors.fill: parent
        spacing: 5

        QGCFlickable {
            Layout.fillHeight: true
            Layout.fillWidth: true
            contentHeight: gridLayoutInfoSlot.height

            GridLayout {
                id: gridLayoutInfoSlot
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 5
                columns: 2

                /********** infoslot类型 **********/
                QGCLabel {
                    text: qsTr("infoslot_type")
                    Layout.columnSpan: 2
                }
                // 大类编号
                QGCLabel {
                    text: qsTr("infoslot")
                    Layout.fillWidth:   true
                }
                FactComboBox {
                    fact:               itemInfoSlot.majorType
                    Layout.fillWidth:   true
                }
                // 小类编号
                QGCLabel {
                    text: qsTr("infoslot_type")
                    Layout.fillWidth:   true
                }
                FactComboBox {
                    fact:               itemInfoSlot.minorType
                    Layout.fillWidth:   true
                }

                /********** 经度 **********/
                QGCLabel {
                    text: qsTr("Longitude")
                }
                FactTextField {
                    fact:               itemInfoSlot.lon
                    Layout.fillWidth:   true
                }
                /********** 纬度 **********/
                QGCLabel {
                    text: qsTr("Latitude")
                }
                FactTextField {
                    fact:               itemInfoSlot.lat
                    Layout.fillWidth:   true
                }
                /********** 高度 **********/
                QGCLabel {
                    text: qsTr("Altitude")
                }
                FactTextField {
                    fact:               itemInfoSlot.alt
                    Layout.fillWidth:   true
                }

                Rectangle {
                    height: 1
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                    color: _splitLineColorLighter
                }

                /********** 特征圆半径 **********/
                QGCLabel {
                    text: qsTr("radiusCh")
                }
                FactTextField {
                    fact:               itemInfoSlot.radiusCh
                    Layout.fillWidth:   true
                }

                Rectangle {
                    height: 1
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                    color: _splitLineColorLighter
                }

                /********** 参考飞行速度 **********/
                QGCLabel {
                    text: qsTr("rVGnd")
                }
                FactTextField {
                    fact:               itemInfoSlot.rVGnd
                    Layout.fillWidth:   true
                }
                /********** 参考爬升速度或梯度 **********/
                QGCLabel {
                    text: qsTr("rRocDoc")
                }
                FactTextField {
                    fact:               itemInfoSlot.rRocDoc
                    Layout.fillWidth:   true
                }

                Rectangle {
                    height: 1
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                    color: _splitLineColorLighter
                }

                /********** 切换条件延迟时长 **********/
                QGCLabel {
                    text: qsTr("swTd")
                }
                FactTextField {
                    fact:               itemInfoSlot.swTd
                    Layout.fillWidth:   true
                }
                /********** 切换条件速度阈值 **********/
                QGCLabel {
                    text: qsTr("swVGnd")
                }
                FactTextField {
                    fact:               itemInfoSlot.swVGnd
                    Layout.fillWidth:   true
                }
                /********** 切换方位角条件阈值 **********/
                QGCLabel {
                    text: qsTr("rHdgDeg")
                }
                FactTextField {
                    fact:               itemInfoSlot.rHdgDeg
                    Layout.fillWidth:   true
                }

                Rectangle {
                    height: 1
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                    color: _splitLineColorLighter
                }

                /********** 离开动作字段 **********/
                QGCLabel {
                    text: qsTr("actDept")
                    Layout.columnSpan: 2
                }
                // 主引擎操作
                QGCLabel {
                    text: qsTr("actDeptMainEngine")
                    Layout.fillWidth:   true
                }
                FactComboBox {
                    fact:               itemInfoSlot.actDeptMainEngine
                    Layout.fillWidth:   true
                }
                // 减速机构
                QGCLabel {
                    text: qsTr("actDeptDecelerate")
                    Layout.fillWidth:   true
                }
                FactComboBox {
                    fact:               itemInfoSlot.actDeptDecelerate
                    Layout.fillWidth:   true
                }
                // 起落装置
                QGCLabel {
                    text: qsTr("actDeptLandingGear")
                    Layout.fillWidth:   true
                }
                FactComboBox {
                    fact:               itemInfoSlot.actDeptLandingGear
                    Layout.fillWidth:   true
                }
                // 默认载荷操作
                QGCLabel {
                    text: qsTr("actDeptPayload")
                    Layout.fillWidth:   true
                }
                FactComboBox {
                    fact:               itemInfoSlot.actDeptPayload
                    Layout.fillWidth:   true
                }
                // 载具操作
                QGCLabel {
                    text: qsTr("actDeptVehicle")
                    Layout.fillWidth:   true
                }
                FactComboBox {
                    fact:               itemInfoSlot.actDeptVehicle
                    Layout.fillWidth:   true
                }
                // 机动动作
                QGCLabel {
                    text: qsTr("actDeptMnvr")
                    Layout.fillWidth:   true
                }
                FactComboBox {
                    fact:               itemInfoSlot.actDeptMnvr
                    Layout.fillWidth:   true
                }

                Rectangle {
                    height: 1
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                    color: _splitLineColorLighter
                }

                /********** 到达动作字段 **********/
                QGCLabel {
                    text: qsTr("act arrvl")
                    Layout.columnSpan: 2
                }
                // 主引擎操作
                QGCLabel {
                    text: qsTr("actArrvlMainEngine")
                    Layout.fillWidth:   true
                }
                FactComboBox {
                    fact:               itemInfoSlot.actArrvlMainEngine
                    Layout.fillWidth:   true
                }
                // 减速机构
                QGCLabel {
                    text: qsTr("actArrvlDecelerate")
                    Layout.fillWidth:   true
                }
                FactComboBox {
                    fact:               itemInfoSlot.actArrvlDecelerate
                    Layout.fillWidth:   true
                }
                // 起落装置
                QGCLabel {
                    text: qsTr("actArrvlLandingGear")
                    Layout.fillWidth:   true
                }
                FactComboBox {
                    fact:               itemInfoSlot.actArrvlLandingGear
                    Layout.fillWidth:   true
                }
                // 默认载荷操作
                QGCLabel {
                    text: qsTr("actArrvlPayload")
                    Layout.fillWidth:   true
                }
                FactComboBox {
                    fact:               itemInfoSlot.actArrvlPayload
                    Layout.fillWidth:   true
                }
                // 载具操作
                QGCLabel {
                    text: qsTr("actArrvlVehicle")
                    Layout.fillWidth:   true
                }
                FactComboBox {
                    fact:               itemInfoSlot.actArrvlVehicle
                    Layout.fillWidth:   true
                }
                // 机动动作
                QGCLabel {
                    text: qsTr("actArrvlMnvr")
                    Layout.fillWidth:   true
                }
                FactComboBox {
                    fact:               itemInfoSlot.actArrvlMnvr
                    Layout.fillWidth:   true
                }

                Rectangle {
                    height: 1
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                    color: _splitLineColorLighter
                }

                /********** 切换条件设置 **********/
                QGCLabel {
                    text: qsTr("sw cond set")
                    Layout.columnSpan: 2
                }
                // 自动切换开关
                QGCLabel {
                    text: qsTr("swCondAuto")
                    Layout.fillWidth:   true
                }
                FactComboBox {
                    fact:               itemInfoSlot.swCondAuto
                    Layout.fillWidth:   true
                }
                // 速度信息源选择
                QGCLabel {
                    text: qsTr("swCondSpeedSource")
                    Layout.fillWidth:   true
                }
                FactComboBox {
                    fact:               itemInfoSlot.swCondSpeedSource
                    Layout.fillWidth:   true
                }
                // 平面误差条件
                QGCLabel {
                    text: qsTr("swCondHorizontalError")
                    Layout.fillWidth:   true
                }
                FactComboBox {
                    fact:               itemInfoSlot.swCondHorizontalError
                    Layout.fillWidth:   true
                }
                // 高度条件
                QGCLabel {
                    text: qsTr("swCondHeight")
                    Layout.fillWidth:   true
                }
                FactComboBox {
                    fact:               itemInfoSlot.swCondHeight
                    Layout.fillWidth:   true
                }
                // 速度条件
                QGCLabel {
                    text: qsTr("swCondSpeed")
                    Layout.fillWidth:   true
                }
                FactComboBox {
                    fact:               itemInfoSlot.swCondSpeed
                    Layout.fillWidth:   true
                }
                // 纵向运动条件
                QGCLabel {
                    text: qsTr("swCondLongitudinalMotion")
                    Layout.fillWidth:   true
                }
                FactComboBox {
                    fact:               itemInfoSlot.swCondLongitudinalMotion
                    Layout.fillWidth:   true
                }
                // 保留字段
                QGCLabel {
                    text: qsTr("swCondReserved")
                    Layout.fillWidth:   true
                }
                FactComboBox {
                    fact:               itemInfoSlot.swCondReserved
                    Layout.fillWidth:   true
                }

                Rectangle {
                    height: 1
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                    color: _splitLineColorLighter
                }
                /********** 飞行中动作字段 **********/
                QGCLabel {
                    text: qsTr("actInFlt")
                    Layout.fillWidth:   true
                }
                FactTextField {
                    fact:               itemInfoSlot.actInFlt
                    Layout.fillWidth:   true
                }

                Rectangle {
                    height: 1
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                    color: _splitLineColorLighter
                }

                /********** 高度和航向控制模式 **********/
                QGCLabel {
                    text: qsTr("hgtHdgCtrlMode")
                    Layout.columnSpan: 2
                }
                // 高度控制模式
                QGCLabel {
                    text: qsTr("hgtCtrlMode")
                    Layout.fillWidth:   true
                }
                FactComboBox {
                    fact:               itemInfoSlot.hgtCtrlMode
                    Layout.fillWidth:   true
                }
                // 航向控制模式
                QGCLabel {
                    text: qsTr("hdgCtrlMode")
                    Layout.fillWidth:   true
                }
                FactComboBox {
                    fact:               itemInfoSlot.hdgCtrlMode
                    Layout.fillWidth:   true
                }

                Rectangle {
                    height: 1
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                    color: _splitLineColorLighter
                }

                /********** 盘旋方位角切换 **********/
                QGCLabel {
                    text: qsTr("swAngDeg")
                    Layout.fillWidth:   true
                }
                FactTextField {
                    fact:               itemInfoSlot.swAngDeg
                    Layout.fillWidth:   true
                }

                /********** 盘旋切换圈数 **********/
                QGCLabel {
                    text: qsTr("swTurns")
                    Layout.fillWidth:   true
                }
                FactTextField {
                    fact:               itemInfoSlot.swTurns
                    Layout.fillWidth:   true
                }

                Rectangle {
                    height: 1
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                    color: _splitLineColorLighter
                }

                /********** 制导模式 **********/
                QGCLabel {
                    text: qsTr("gdMode")
                    Layout.fillWidth:   true
                }
                FactComboBox {
                    fact:               itemInfoSlot.gdMode
                    Layout.fillWidth:   true
                }

                Rectangle {
                    height: 1
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                    color: _splitLineColorLighter
                }

                /********** 机动风格 **********/
                QGCLabel {
                    text: qsTr("mnvrSty")
                    Layout.columnSpan: 2
                }
                // 结束模式
                QGCLabel {
                    text: qsTr("mnvrStyEndMode")
                    Layout.fillWidth:   true
                }
                FactComboBox {
                    fact:               itemInfoSlot.mnvrStyEndMode
                    Layout.fillWidth:   true
                }
                // 转弯方式
                QGCLabel {
                    text: qsTr("mnvrStyTurn")
                    Layout.fillWidth:   true
                }
                FactComboBox {
                    fact:               itemInfoSlot.mnvrStyTurn
                    Layout.fillWidth:   true
                }
                // 保留
                QGCLabel {
                    text: qsTr("mnvrStyReserved")
                    Layout.fillWidth:   true
                }
                FactTextField {
                    fact:               itemInfoSlot.mnvrStyReserved
                    Layout.fillWidth:   true
                }

                Rectangle {
                    height: 1
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                    color: _splitLineColorLighter
                }

                /********** 过渡风格 **********/
                // 过渡风格
                QGCLabel {
                    text: qsTr("transSty")
                    Layout.fillWidth:   true
                }
                FactComboBox {
                    fact:               itemInfoSlot.transSty
                    Layout.fillWidth:   true
                }

                Rectangle {
                    height: 1
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                    color: _splitLineColorLighter
                }

                /********** reserved0 **********/
                // 保留变量
                QGCLabel {
                    text: qsTr("reserved0")
                    Layout.fillWidth:   true
                }
                FactTextField {
                    fact:               itemInfoSlot.reserved0
                    Layout.fillWidth:   true
                }

                /********** reserved1 **********/
                // 保留变量
                QGCLabel {
                    text: qsTr("reserved1")
                    Layout.fillWidth:   true
                }
                FactTextField {
                    fact:               itemInfoSlot.reserved1
                    Layout.fillWidth:   true
                }
            }
        }
    }
    Component.onCompleted: {
        console.log("itemInfoSlot id", itemInfoSlot.idInfoSlot, "idwaypoint:", itemInfoSlot.idWaypoint, "lon:", itemInfoSlot.lon.value, "_root:", _root)
    }
    Component.onDestruction: {
        console.log("itemInfoSlot onDestruction id", itemInfoSlot.idInfoSlot)
    }
    Timer {
        id: timer
        interval: 1000
        repeat: true
        running: true
        onTriggered: console.log("item editor:", _root, "idInfoSlot:", itemInfoSlot.idInfoSlot)
    }
    onHideDialog: {
        itemInfoSlot.updateSlotInfoData()
    }
} // Rectangle
