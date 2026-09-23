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

import QGroundControl
import QGroundControl.FactSystem
import QGroundControl.FactControls
import QGroundControl.Controls
import QGroundControl.ScreenTools
import QGroundControl.MultiVehicleManager
import QGroundControl.Palette

SettingsPage {
    property var    _settingsManager:           QGroundControl.settingsManager
    property var    _cloudServerSettings:       _settingsManager.cloudServerSettings
    property var    _multivehicleManager:       QGroundControl.multiVehicleManager

    // SettingsGroupLayout {
    //     Layout.fillWidth:   true
    //     heading:            qsTr("CloudServer")
    //     RowLayout {
    //         spacing: ScreenTools.defaultFontPixelWidth

    //         LabelledFactTextField {
    //             id:                         _serverUrl
    //             Layout.fillWidth:           true
    //             textFieldPreferredWidth:    ScreenTools.defaultFontPixelWidth * 20
    //             label:                      qsTr("URL")
    //             fact:                       _cloudServerSettings.serverUrl
    //             visible:                    fact.visible
    //         }

    //         QGCButton {
    //             text:       qsTr("Open")
    //             onClicked: {
    //                 // _multivehicleManager.connectToMqttHost()
    //             }
    //         }
    //     }
    // }

    // Dio

    SettingsGroupLayout {
        Layout.fillWidth:   true
        heading:            qsTr("Mqtt")



        RowLayout {
            spacing: ScreenTools.defaultFontPixelWidth

            LabelledFactTextField {
                id:                         _mqttHost
                Layout.fillWidth:           true
                textFieldPreferredWidth:    ScreenTools.defaultFontPixelWidth * 20
                label:                      qsTr("Host name")
                fact:                       _cloudServerSettings.mqttHost
                visible:                    fact.visible
            }

            QGCButton {
                text:       _multivehicleManager.mqttConnected ? qsTr("Disconnect") : qsTr("Connect")
                onClicked: {
                    _multivehicleManager.connectToMqttHost()
                }
            }
        }
        LabelledFactTextField {
            Layout.fillWidth:           true
            textFieldPreferredWidth:    ScreenTools.defaultFontPixelWidth * 20
            label:                      qsTr("gcs sn")
            fact:                       _cloudServerSettings.gcsSn
            visible:                    fact.visible
        }
        LabelledFactTextField {
            Layout.fillWidth:           true
            textFieldPreferredWidth:    ScreenTools.defaultFontPixelWidth * 20
            label:                      qsTr("drone sn")
            fact:                       _cloudServerSettings.droneSn
            visible:                    fact.visible
        }
    }

    SettingsGroupLayout {
        Layout.fillWidth:   true
        heading:            qsTr("Cloud Api")

        LabelledFactTextField {
            Layout.fillWidth:           true
            textFieldPreferredWidth:    ScreenTools.defaultFontPixelWidth * 20
            label:                      qsTr("appId")
            fact:                       _cloudServerSettings.appId
            visible:                    fact.visible
        }
        LabelledFactTextField {
            Layout.fillWidth:           true
            textFieldPreferredWidth:    ScreenTools.defaultFontPixelWidth * 20
            label:                      qsTr("appKey")
            fact:                       _cloudServerSettings.appKey
            visible:                    fact.visible
        }
        LabelledFactTextField {
            Layout.fillWidth:           true
            textFieldPreferredWidth:    ScreenTools.defaultFontPixelWidth * 20
            label:                      qsTr("appLicense")
            fact:                       _cloudServerSettings.appLicense
            visible:                    fact.visible
        }
    }

    SettingsGroupLayout {
        Layout.fillWidth:   true
        heading:            qsTr("Cloud Map")

        FactCheckBox {
            Layout.fillWidth:   true
            text:               qsTr("Coordinate correction (GCJ-02)")
            fact:               _cloudServerSettings.coordinateTransform
            visible:            fact.visible
        }

        QGCLabel {
            Layout.fillWidth:   true
            Layout.maximumWidth: ScreenTools.defaultFontPixelWidth * 40
            wrapMode:           Text.WordWrap
            font.pointSize:     ScreenTools.smallFontPointSize
            color:              qgcPal.colorGrey
            text:               qsTr("Leave this OFF. It is only for backends that store shapes in GCJ-02 (AMap/Gaode) coordinates: turning it on converts incoming shapes to WGS84 and outgoing ones back to GCJ-02. Our backend stores plain WGS84, so enabling it shifts every cloud shape by 300-600m.")
        }
    }

    // 指令飞行 / 远程控制（DRC）。运行期的状态与控制权在飞行视图右侧的 DRC 面板上，
    // 这里只放"得先定下来、之后不该随手改"的几项。
    SettingsGroupLayout {
        Layout.fillWidth:   true
        heading:            qsTr("DRC (Direct Remote Control)")

        FactCheckBox {
            Layout.fillWidth:   true
            text:               qsTr("Require local operator consent before the cloud takes control")
            fact:               _cloudServerSettings.drcRequireLocalConsent
            visible:            fact.visible
        }

        QGCLabel {
            Layout.fillWidth:   true
            Layout.maximumWidth: ScreenTools.defaultFontPixelWidth * 40
            wrapMode:           Text.WordWrap
            font.pointSize:     ScreenTools.smallFontPointSize
            color:              qgcPal.colorGrey
            text:               qsTr("On by default: handing the sticks to somebody else's web panel is exactly the moment a local operator should get a say. The official web console enters DRC and grabs flight authority without ever sending a control request, so with this on that flow stops until someone here clicks Allow. Turn it off for unattended operation, where nobody is at the ground station to answer the dialog.")
        }

        FactCheckBox {
            Layout.fillWidth:   true
            text:               qsTr("Allow the cloud to trigger an emergency stop")
            fact:               _cloudServerSettings.drcEmergencyStopEnabled
            visible:            fact.visible
        }

        QGCLabel {
            Layout.fillWidth:   true
            Layout.maximumWidth: ScreenTools.defaultFontPixelWidth * 40
            wrapMode:           Text.WordWrap
            font.pointSize:     ScreenTools.smallFontPointSize
            color:              qgcPal.colorRed
            text:               qsTr("An emergency stop maps to MAV_CMD_DO_FLIGHTTERMINATION: it cuts the motors in flight and cannot be undone. Even with this on, every request still has to be confirmed in a dialog on this ground station.")
        }

        QGCLabel {
            Layout.fillWidth:   true
            Layout.maximumWidth: ScreenTools.defaultFontPixelWidth * 40
            wrapMode:           Text.WordWrap
            font.pointSize:     ScreenTools.smallFontPointSize
            color:              qgcPal.colorGrey
            text:               qsTr("The flight controller ignores stick input outside a handful of modes, so a takeover while the aircraft sits in Hold leaves the cloud holding authority that does nothing. This ground station does not switch the flight mode on its own - set it by hand before handing control over. The cloud control indicator on the toolbar shows a red \"mode rejected\" state while authority is held in a mode that ignores sticks.")
        }

        LabelledFactTextField {
            Layout.fillWidth:           true
            textFieldPreferredWidth:    ScreenTools.defaultFontPixelWidth * 12
            label:                      qsTr("Camera horizontal FOV (deg)")
            fact:                       _cloudServerSettings.drcCameraHFov
            visible:                    fact.visible
        }

        QGCLabel {
            Layout.fillWidth:   true
            Layout.maximumWidth: ScreenTools.defaultFontPixelWidth * 40
            wrapMode:           Text.WordWrap
            font.pointSize:     ScreenTools.smallFontPointSize
            color:              qgcPal.colorGrey
            text:               qsTr("Only used by the cloud's \"aim camera here\" command, which sends a point in the video frame and expects the gimbal to centre on it. QGC cannot read the lens field of view, so set it per lens: wide angle, zoom and IR all differ.")
        }

        // 三个轴向取反。协议里 x/y/w 的正方向在不同后端上说法不一，
        // 台架打单轴实测才知道，所以就放在这儿给实测用。
        QGCLabel {
            Layout.fillWidth:   true
            font.pointSize:     ScreenTools.smallFontPointSize
            color:              qgcPal.text
            text:               qsTr("Axis calibration — flip an axis if the aircraft moves the wrong way (bench test with the debug stick in the DRC panel):")
        }

        FactCheckBox {
            Layout.fillWidth:   true
            text:               qsTr("Invert x — left/right")
            fact:               _cloudServerSettings.drcInvertX
            visible:            fact.visible
        }

        FactCheckBox {
            Layout.fillWidth:   true
            text:               qsTr("Invert y — forward/back")
            fact:               _cloudServerSettings.drcInvertY
            visible:            fact.visible
        }

        FactCheckBox {
            Layout.fillWidth:   true
            text:               qsTr("Invert w — yaw")
            fact:               _cloudServerSettings.drcInvertW
            visible:            fact.visible
        }
    }
}
