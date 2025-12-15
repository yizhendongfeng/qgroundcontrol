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
import QGroundControl.FactSystem
import QGroundControl.FactControls
import QGroundControl.Controls
import QGroundControl.ScreenTools
import QGroundControl.GCU

QGCPopupDialog {
    id:             root
    title:          qsTr("吊舱设置")
    buttons:        Dialog.Close
    // width:          500
    // height:         500
    property var    _settingsManager:           QGroundControl.settingsManager
    property var    _videoManager:              QGroundControl.videoManager
    property var    _videoSettings:             _settingsManager.videoSettings
    property var    _gcu:                       _videoManager.gcu

    property string _videoSource:               _videoSettings.videoSource.rawValue
    property bool   _isGST:                     _videoManager.gstreamerEnabled
    property bool   _isStreamSource:            _videoManager.isStreamSource
    property bool   _isUDP264:                  _isStreamSource && (_videoSource === _videoSettings.udp264VideoSource)
    property bool   _isUDP265:                  _isStreamSource && (_videoSource === _videoSettings.udp265VideoSource)
    property bool   _isRTSP:                    _isStreamSource && (_videoSource === _videoSettings.rtspVideoSource)
    property bool   _isTCP:                     _isStreamSource && (_videoSource === _videoSettings.tcpVideoSource)
    property bool   _isMPEGTS:                  _isStreamSource && (_videoSource === _videoSettings.mpegtsVideoSource)
    property bool   _videoAutoStreamConfig:     _videoManager.autoStreamConfigured
    property real   _urlFieldWidth:             ScreenTools.defaultFontPixelWidth * 25
    property bool   _requiresUDPPort:           _isUDP264 || _isUDP265 || _isMPEGTS
    readonly property real  _margin:            ScreenTools.defaultFontPixelWidth / 2
    ColumnLayout {
        spacing: _margin
        SettingsGroupLayout {
            Layout.fillWidth:   true
            heading:            qsTr("Video Source")
            headingDescription: _videoAutoStreamConfig ? qsTr("Mavlink camera stream is automatically configured") : ""
            enabled:            !_videoAutoStreamConfig

            LabelledFactComboBox {
                Layout.fillWidth:   true
                label:              qsTr("Source")
                indexModel:         false
                fact:               _videoSettings.videoSource
                visible:            fact.visible
            }


            SettingsGroupLayout {
                Layout.fillWidth:   true
                heading:            qsTr("Connection")
                visible:            !_videoAutoStreamConfig && (_isTCP || _isRTSP | _requiresUDPPort)

                LabelledFactTextField {
                    Layout.fillWidth:           true
                    textFieldPreferredWidth:    _urlFieldWidth
                    label:                      qsTr("RTSP URL")
                    fact:                       _videoSettings.rtspUrl
                    visible:                    _isRTSP && _videoSettings.rtspUrl.visible
                }

                LabelledFactTextField {
                    Layout.fillWidth:           true
                    label:                      qsTr("TCP URL")
                    textFieldPreferredWidth:    _urlFieldWidth
                    fact:                       _videoSettings.tcpUrl
                    visible:                    _isTCP && _videoSettings.tcpUrl.visible
                }
                LabelledFactTextField {
                    Layout.fillWidth:   true
                    label:              qsTr("UDP Port")
                    fact:               _videoSettings.udpPort
                    visible:            _requiresUDPPort && _videoSettings.udpPort.visible
                }
            }

            SettingsGroupLayout {
                Layout.fillWidth:   true
                heading:            qsTr("吊舱控制")
                // 吊舱控制
                LabelledFactTextField {
                    Layout.fillWidth:           true
                    label:                      qsTr("吊舱IP: ")
                    textFieldPreferredWidth:    _urlFieldWidth
                    fact:                       _videoSettings.podIp
                    visible:                    true
                }

                LabelledFactTextField {
                    Layout.fillWidth:   true
                    label:              qsTr("吊舱端口：")
                    fact:               _videoSettings.podPort
                    visible:            true
                }

                RowLayout {
                    Layout.fillWidth:  true
                    // Layout.leftMargin:  parent.width * 0.2
                    // Layout.rightMargin: parent.width * 0.2
                    spacing: 30
                    QGCButton {
                        Layout.alignment: Qt.AlignLeft
                        Layout.fillWidth: true
                        text:               _gcu.connected ? qsTr("断开") : qsTr("连接")
                        onClicked: {
                            console.log("连接吊舱控制端口！")
                            _gcu.connectToGcu(!_gcu.connected);
                        }
                    }


                    FactCheckBoxSlider {
                        Layout.alignment:  Qt.AlignRight
                        text: qsTr("自动连接")
                        fact: _videoSettings.autoConnectToPod
                    }
                }
            }
        }



        SettingsGroupLayout {
            Layout.fillWidth:   true
            heading:            qsTr("Settings")

            LabelledFactTextField {
                Layout.fillWidth:   true
                label:              qsTr("Aspect Ratio")
                fact:               _videoSettings.aspectRatio
                visible:            !_videoAutoStreamConfig && _isStreamSource && _videoSettings.aspectRatio.visible
            }

            FactCheckBoxSlider {
                Layout.fillWidth:   true
                text:               qsTr("Stop recording when disarmed")
                fact:               _videoSettings.disableWhenDisarmed
                visible:            !_videoAutoStreamConfig && _isStreamSource && fact.visible
            }

            FactCheckBoxSlider {
                Layout.fillWidth:   true
                text:               qsTr("Low Latency Mode")
                fact:               _videoSettings.lowLatencyMode
                visible:            !_videoAutoStreamConfig && _isStreamSource && fact.visible && _isGST
            }

            LabelledFactComboBox {
                Layout.fillWidth:   true
                label:              qsTr("Video decode priority")
                fact:               _videoSettings.forceVideoDecoder
                visible:            fact.visible
                indexModel:         false
            }
        }

        SettingsGroupLayout {
            Layout.fillWidth: true
            heading:            qsTr("Local Video Storage")

            LabelledFactComboBox {
                Layout.fillWidth:   true
                label:              qsTr("Record File Format")
                fact:               _videoSettings.recordingFormat
                visible:            _videoSettings.recordingFormat.visible
            }

            FactCheckBoxSlider {
                Layout.fillWidth:   true
                text:               qsTr("Auto-Delete Saved Recordings")
                fact:               _videoSettings.enableStorageLimit
                visible:            fact.visible
            }

            LabelledFactTextField {
                Layout.fillWidth:   true
                label:              qsTr("Max Storage Usage")
                fact:               _videoSettings.maxVideoSize
                visible:            fact.visible
                enabled:            _videoSettings.enableStorageLimit.rawValue
            }
        }
    }
}
