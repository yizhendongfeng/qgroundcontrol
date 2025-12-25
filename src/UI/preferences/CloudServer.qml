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
}
