/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

import QtQuick                  2.4
import QtPositioning            5.2
import QtQuick.Layouts          1.2
import QtQuick.Controls         1.4
import QtQuick.Dialogs          1.2
import QtGraphicalEffects       1.0

import QGroundControl                   1.0
import QGroundControl.ScreenTools       1.0
import QGroundControl.Controls          1.0
import QGroundControl.Palette           1.0
import QGroundControl.Vehicle           1.0
import QGroundControl.Controllers       1.0
import QGroundControl.FactSystem        1.0
import QGroundControl.FactControls      1.0

Rectangle {
    id: _root
    height:     mainLayout.height
    color:      Qt.rgba(0,0,0,0)
//    radius:     _margins
    width:      parent.width / 15 > _maxWidth ? _maxWidth : parent.width / 15
    visible:    true//(_mavlinkCamera || _videoStreamAvailable || _simpleCameraAvailable) && multiVehiclePanelSelector.showSingleVehiclePanel
    property var    _maxWidth: 40
    property var    _minWidth: 10

    property real   _margins:                                   ScreenTools.defaultFontPixelHeight / 2
    property var    _activeVehicle:                             QGroundControl.multiVehicleManager.activeVehicle

    // The following properties relate to a simple camera
    property var    _flyViewSettings:                           QGroundControl.settingsManager.flyViewSettings
    property bool   _simpleCameraAvailable:                     _activeVehicle && _flyViewSettings.showSimpleCameraControl.rawValue
    property bool   _onlySimpleCameraAvailable:                 !_anyVideoStreamAvailable && _simpleCameraAvailable
    property bool   _simpleCameraIsShootingInCurrentMode:       _onlySimpleCameraAvailable && !_simplePhotoCaptureIsIdle

    property real   _comboFieldWidth:           ScreenTools.defaultFontPixelWidth * 30

    property var    _videoSettings:             QGroundControl.settingsManager.videoSettings
    property string _videoSource:               _videoSettings.videoSource.rawValue
    property bool   _isGst:                     QGroundControl.videoManager.isGStreamer
    property bool   _isUDP264:                  _isGst && _videoSource === _videoSettings.udp264VideoSource
    property bool   _isUDP265:                  _isGst && _videoSource === _videoSettings.udp265VideoSource
    property bool   _isRTSP:                    _isGst && _videoSource === _videoSettings.rtspVideoSource
    property bool   _isTCP:                     _isGst && _videoSource === _videoSettings.tcpVideoSource
    property bool   _isMPEGTS:                  _isGst && _videoSource === _videoSettings.mpegtsVideoSource
    property bool   _videoAutoStreamConfig:     QGroundControl.videoManager.autoStreamConfigured
    property bool   _showSaveVideoSettings:     _isGst || _videoAutoStreamConfig
    property bool   _disableAllDataPersistence: QGroundControl.settingsManager.appSettings.disableAllPersistence.rawValue

    // The following properties relate to a simple video stream
    property bool   _videoStreamAvailable:                      _videoStreamManager.hasVideo
    property var    _videoStreamSettings:                       QGroundControl.settingsManager.videoSettings
    property var    _videoStreamManager:                        QGroundControl.videoManager
    property bool   _videoStreamAllowsPhotoWhileRecording:      true
    property bool   _videoStreamIsStreaming:                    _videoStreamManager.streaming
    property bool   _simplePhotoCaptureIsIdle:                  true
    property bool   _videoStreamRecording:                      _videoStreamManager.recording
    property bool   _videoStreamCanShoot:                       _videoStreamIsStreaming
    property bool   _videoStreamIsShootingInCurrentMode:        _videoStreamInPhotoMode ? !_simplePhotoCaptureIsIdle : _videoStreamRecording
    property bool   _videoStreamInPhotoMode:                    false

    // The following settings and functions unify between a mavlink camera and a simple video stream for simple access

    property bool   _anyVideoStreamAvailable:                   _videoStreamManager.hasVideo
    property string _cameraName:                                ""
    property bool   _modeIndicatorPhotoMode:                    _videoStreamInPhotoMode || _onlySimpleCameraAvailable
    property bool   _allowsPhotoWhileRecording:                 _videoStreamAllowsPhotoWhileRecording
    property bool   _videoIsRecording:                          _videoStreamRecording
    property bool   _canShootInCurrentMode:                     _videoStreamCanShoot || _simpleCameraAvailable
    property bool   _isShootingInCurrentMode:                   _videoStreamIsShootingInCurrentMode || _simpleCameraIsShootingInCurrentMode
    property var    _fontSize: width / 2 < ScreenTools.defaultFontPixelHeight ? width / 2 : ScreenTools.defaultFontPixelHeight


    Component.onCompleted: console.log("PhotoVideoControl _fontSize:", _fontSize, "ScreenTools.smallFontPointSize:", ScreenTools.smallFontPointSize)
    function setCameraMode(photoMode) {
        _videoStreamInPhotoMode = photoMode
    }

    function toggleShooting() {
        console.log("toggleShooting", _modeIndicatorPhotoMode, "_videoStreamManager.recording", _videoStreamManager.recording)
        if (_modeIndicatorPhotoMode) {
            _simplePhotoCaptureIsIdle = false
            _videoStreamManager.grabImage()
            simplePhotoCaptureTimer.start()
        } else {
            if (_videoStreamManager.recording) {
                _videoStreamManager.stopRecording()
            } else {
                _videoStreamManager.startRecording()
            }
        }
    }

    Timer {
        id:             simplePhotoCaptureTimer
        interval:       100
        onTriggered:    _simplePhotoCaptureIsIdle = true
    }

    QGCPalette { id: qgcPal; colorGroupEnabled: enabled }

    ColumnLayout {
        id:                         mainLayout
        anchors.top:                parent.top
        anchors.horizontalCenter:   parent.horizontalCenter
        anchors.left:               parent.left
        spacing:                    _root.width / 4

        // Photo/Video Mode Selector
        QGCColoredImage {
            height:             parent.height * 0.6
            width:              height
            source:             _modeIndicatorPhotoMode ? "/qmlimages/camera_photo.svg" : "/qmlimages/camera_video.svg"
            mipmap:             true
            Layout.alignment:          Qt.AlignHCenter
            Layout.preferredWidth:     _root.width * 0.8
            Layout.minimumWidth:       _minWidth
            Layout.maximumWidth:       _maxWidth
            Layout.minimumHeight:      Layout.minimumWidth
            Layout.maximumHeight:      Layout.maximumWidth
            Layout.preferredHeight:    Layout.preferredWidth
            fillMode:           Image.PreserveAspectFit
            sourceSize.height:  height
            color:              qgcPal.text   //_modeIndicatorPhotoMode ? qgcPal.text : qgcPal.colorGreen
            MouseArea {
                anchors.fill:   parent
                onClicked:      {
                    if (_modeIndicatorPhotoMode)
                        setCameraMode(false)
                    else
                        setCameraMode(true)
                }
                hoverEnabled: true
            }
        }
        // Take Photo, Start/Stop Video button
        Rectangle {
            Layout.alignment:   Qt.AlignHCenter
            color:              Qt.rgba(0,0,0,0)

            Layout.preferredWidth:     _root.width
            Layout.minimumWidth:       _minWidth
            Layout.maximumWidth:       _maxWidth
            Layout.minimumHeight:      Layout.minimumWidth
            Layout.maximumHeight:      Layout.maximumWidth
            Layout.preferredHeight:    Layout.preferredWidth

            radius:             width * 0.5
            border.color:       qgcPal.buttonText
            border.width:       3

            Rectangle {
                anchors.centerIn:   parent
                width:              parent.width * (_isShootingInCurrentMode ? 0.6 : 0.75)
                height:             width
                radius:             _modeIndicatorPhotoMode ? width * 0.5 : (_videoStreamManager.recording ? 0 : width * 0.5)
                color:              _modeIndicatorPhotoMode ? qgcPal.colorGrey : qgcPal.colorRed
            }

            MouseArea {
                anchors.fill:   parent
                enabled:        true//_canShootInCurrentMode
                onClicked:      toggleShooting()
            }
            Component.onCompleted: console.log("ScreenTools.defaultFontPixelWidth:", ScreenTools.defaultFontPixelWidth, "_fontSize:", _fontSize)
        }

        QGCColoredImage {
            Layout.alignment:   Qt.AlignHCenter
            source:             "/res/gear-black.svg"
            mipmap:             true
            Layout.preferredWidth:     _root.width * 0.8
            Layout.minimumWidth:       _minWidth
            Layout.maximumWidth:       _maxWidth
            Layout.minimumHeight:      Layout.minimumWidth
            Layout.maximumHeight:      Layout.maximumWidth
            Layout.preferredHeight:    Layout.preferredWidth
            sourceSize.height:  height
            color:              qgcPal.text
            fillMode:           Image.PreserveAspectFit
            visible:            true //!_onlySimpleCameraAvailable

            QGCMouseArea {
                fillItem:   parent
                onClicked:  mainWindow.showPopupDialogFromComponent(settingsDialogComponent)
            }
        }
    }

    //-- Status Information
    ColumnLayout {
//        Layout.alignment:   Qt.AlignHCenter
        anchors.top:        mainLayout.bottom
        anchors.topMargin:  _fontSize
        anchors.horizontalCenter: _root.horizontalCenter
        spacing:            0

        QGCLabel {
            Layout.alignment:   Qt.AlignHCenter
            text:               _cameraName
            visible:            _cameraName !== ""
            font.pointSize:     ScreenTools.defaultFontPointSize    //_fontSize
            Component.onCompleted: console.log("_cameraName:", _cameraName, "_fontSize:", font.pointSize)
        }
        QGCLabel {
            Layout.alignment:   Qt.AlignHCenter
            text:               _activeVehicle ? ('00000' + _activeVehicle.cameraTriggerPoints.count).slice(-5) : "00000"
            visible:            _modeIndicatorPhotoMode
            font.pointSize:     ScreenTools.defaultFontPointSize    //_fontSize
        }
    }

    Component {
        id: settingsDialogComponent

        QGCPopupDialog {
            title:      qsTr("Settings")
            buttons:    StandardButton.Close

            ColumnLayout {
                spacing: _margins
                GridLayout {
                    id:         videoGrid
                    columns:    2
                    visible:    _videoSettings.visible

                    QGCLabel {
                        text:               qsTr("Video Settings")
                        Layout.columnSpan:  2
                        Layout.alignment:   Qt.AlignHCenter
                    }

                    QGCLabel {
                        id:         videoSourceLabel
                        text:       qsTr("Source")
                        visible:    !_videoAutoStreamConfig && _videoSettings.videoSource.visible
                    }
                    FactComboBox {
                        id:                     videoSource
                        Layout.preferredWidth:  _comboFieldWidth
                        indexModel:             false
                        fact:                   _videoSettings.videoSource
                        visible:                videoSourceLabel.visible
                    }

                    QGCLabel {
                        id:         udpPortLabel
                        text:       qsTr("UDP Port")
                        visible:    !_videoAutoStreamConfig && (_isUDP264 || _isUDP265 || _isMPEGTS) && _videoSettings.udpPort.visible
                    }
                    FactTextField {
                        Layout.preferredWidth:  _comboFieldWidth
                        fact:                   _videoSettings.udpPort
                        visible:                udpPortLabel.visible
                    }

                    QGCLabel {
                        id:         rtspUrlLabel
                        text:       qsTr("RTSP URL")
                        visible:    !_videoAutoStreamConfig && _isRTSP && _videoSettings.rtspUrl.visible
                    }
                    FactTextField {
                        Layout.preferredWidth:  _comboFieldWidth
                        fact:                   _videoSettings.rtspUrl
                        visible:                rtspUrlLabel.visible
                    }

                    QGCLabel {
                        id:         tcpUrlLabel
                        text:       qsTr("TCP URL")
                        visible:    !_videoAutoStreamConfig && _isTCP && _videoSettings.tcpUrl.visible
                    }
                    FactTextField {
                        Layout.preferredWidth:  _comboFieldWidth
                        fact:                   _videoSettings.tcpUrl
                        visible:                tcpUrlLabel.visible
                    }

                    QGCLabel {
                        text:                   qsTr("Aspect Ratio")
                        visible:                !_videoAutoStreamConfig && _isGst && _videoSettings.aspectRatio.visible
                    }
                    FactTextField {
                        Layout.preferredWidth:  _comboFieldWidth
                        fact:                   _videoSettings.aspectRatio
                        visible:                !_videoAutoStreamConfig && _isGst && _videoSettings.aspectRatio.visible
                    }

                    QGCLabel {
                        id:         videoFileFormatLabel
                        text:       qsTr("File Format")
                        visible:    _showSaveVideoSettings && _videoSettings.recordingFormat.visible
                    }
                    FactComboBox {
                        Layout.preferredWidth:  _comboFieldWidth
                        fact:                   _videoSettings.recordingFormat
                        visible:                videoFileFormatLabel.visible
                    }

                    QGCLabel {
                        id:         maxSavedVideoStorageLabel
                        text:       qsTr("Max Storage Usage")
                        visible:    _showSaveVideoSettings && _videoSettings.maxVideoSize.visible && _videoSettings.enableStorageLimit.value
                    }
                    FactTextField {
                        Layout.preferredWidth:  _comboFieldWidth
                        fact:                   _videoSettings.maxVideoSize
                        visible:                _showSaveVideoSettings && _videoSettings.enableStorageLimit.value && maxSavedVideoStorageLabel.visible
                    }

                    QGCLabel {
                        id:         videoDecodeLabel
                        text:       qsTr("Video decode priority")
                        visible:    forceVideoDecoderComboBox.visible
                    }
                    FactComboBox {
                        id:                     forceVideoDecoderComboBox
                        Layout.preferredWidth:  _comboFieldWidth
                        fact:                   _videoSettings.forceVideoDecoder
                        visible:                fact.visible
                        indexModel:             false
                    }

                    Item { width: 1; height: 1}
                    FactCheckBox {
                        text:       qsTr("Disable When Disarmed")
                        fact:       _videoSettings.disableWhenDisarmed
                        visible:    !_videoAutoStreamConfig && _isGst && fact.visible
                    }

                    Item { width: 1; height: 1}
                    FactCheckBox {
                        text:       qsTr("Low Latency Mode")
                        fact:       _videoSettings.lowLatencyMode
                        visible:    !_videoAutoStreamConfig && _isGst && fact.visible
                    }

                    Item { width: 1; height: 1}
                    FactCheckBox {
                        text:       qsTr("Auto-Delete Saved Recordings")
                        fact:       _videoSettings.enableStorageLimit
                        visible:    _showSaveVideoSettings && fact.visible
                    }
                }


//                GridLayout {
//                    id:     gridLayout
//                    flow:   GridLayout.TopToBottom
//                    rows:   dynamicRows + (_mavlinkCamera ? _mavlinkCamera.activeSettings.length : 0)

//                    property int dynamicRows: 10

//                    // First column
//                    QGCLabel {
//                        text:               qsTr("Camera")
//                        visible:            _multipleMavlinkCameras
//                        onVisibleChanged:   gridLayout.dynamicRows += visible ? 1 : -1
//                    }

//                    QGCLabel {
//                        text:               qsTr("Video Stream")
//                        visible:            _multipleMavlinkCameraStreams
//                        onVisibleChanged:   gridLayout.dynamicRows += visible ? 1 : -1
//                    }

//                    QGCLabel {
//                        text:               qsTr("Thermal View Mode")
//                        visible:            _mavlinkCameraHasThermalVideoStream
//                        onVisibleChanged:   gridLayout.dynamicRows += visible ? 1 : -1
//                    }

//                    QGCLabel {
//                        text:               qsTr("Blend Opacity")
//                        visible:            _mavlinkCameraHasThermalVideoStream && _mavlinkCamera.thermalMode === QGCCameraControl.THERMAL_BLEND
//                        onVisibleChanged:   gridLayout.dynamicRows += visible ? 1 : -1
//                    }

//                    // Mavlink Camera Protocol active settings
//                    Repeater {
//                        model: _mavlinkCamera ? _mavlinkCamera.activeSettings : []

//                        QGCLabel {
//                            text: _mavlinkCamera.getFact(modelData).shortDescription
//                        }
//                    }

//                    QGCLabel {
//                        text:               qsTr("Photo Mode")
//                        visible:            _mavlinkCameraHasModes
//                        onVisibleChanged:   gridLayout.dynamicRows += visible ? 1 : -1
//                    }

//                    QGCLabel {
//                        text:               qsTr("Photo Interval (seconds)")
//                        visible:            _mavlinkCameraInPhotoMode && _mavlinkCamera.photoMode === QGCCameraControl.PHOTO_CAPTURE_TIMELAPSE
//                        onVisibleChanged:   gridLayout.dynamicRows += visible ? 1 : -1
//                    }

//                    QGCLabel {
//                        text:               qsTr("Video Grid Lines")
//                        visible:            _anyVideoStreamAvailable
//                        onVisibleChanged:   gridLayout.dynamicRows += visible ? 1 : -1
//                    }

//                    QGCLabel {
//                        text:               qsTr("Video Screen Fit")
//                        visible:            _anyVideoStreamAvailable
//                        onVisibleChanged:   gridLayout.dynamicRows += visible ? 1 : -1
//                    }

//                    QGCLabel {
//                        text:               qsTr("Reset Camera Defaults")
//                        visible:            _mavlinkCamera
//                        onVisibleChanged:   gridLayout.dynamicRows += visible ? 1 : -1
//                    }

//                    QGCLabel {
//                        text:               qsTr("Storage")
//                        visible:            _mavlinkCameraStorageSupported
//                        onVisibleChanged:   gridLayout.dynamicRows += visible ? 1 : -1
//                    }

//                    // Second column
//                    QGCComboBox {
//                        Layout.fillWidth:   true
//                        sizeToContents:     true
//                        model:              _mavlinkCameraManager ? _mavlinkCameraManager.cameraLabels : []
//                        currentIndex:       _mavlinkCameraManagerCurCameraIndex
//                        visible:            _multipleMavlinkCameras
//                        onActivated:        _mavlinkCameraManager.currentCamera = index
//                    }

//                    QGCComboBox {
//                        Layout.fillWidth:   true
//                        sizeToContents:     true
//                        model:              _mavlinkCamera ? _mavlinkCamera.streamLabels : []
//                        currentIndex:       _mavlinCameraCurStreamIndex
//                        visible:            _multipleMavlinkCameraStreams
//                        onActivated:        _mavlinkCamera.currentStream = index
//                    }

//                    QGCComboBox {
//                        Layout.fillWidth:   true
//                        sizeToContents:     true
//                        model:              [ qsTr("Off"), qsTr("Blend"), qsTr("Full"), qsTr("Picture In Picture") ]
//                        currentIndex:       _mavlinkCamera ? _mavlinkCamera.thermalMode : -1
//                        visible:            _mavlinkCameraHasThermalVideoStream
//                        onActivated:        _mavlinkCamera.thermalMode = index
//                    }

//                    QGCSlider {
//                        Layout.fillWidth:           true
//                        maximumValue:               100
//                        minimumValue:               0
//                        value:                      _mavlinkCamera ? _mavlinkCamera.thermalOpacity : 0
//                        updateValueWhileDragging:   true
//                        visible:                    _mavlinkCameraHasThermalVideoStream && _mavlinkCamera.thermalMode === QGCCameraControl.THERMAL_BLEND
//                        onValueChanged:             _mavlinkCamera.thermalOpacity = value
//                    }

//                    // Mavlink Camera Protocol active settings
//                    Repeater {
//                        model: _mavlinkCamera ? _mavlinkCamera.activeSettings : []

//                        RowLayout {
//                            Layout.fillWidth:   true
//                            spacing:            ScreenTools.defaultFontPixelWidth

//                            property var    _fact:      _mavlinkCamera.getFact(modelData)
//                            property bool   _isBool:    _fact.typeIsBool
//                            property bool   _isCombo:   !_isBool && _fact.enumStrings.length > 0
//                            property bool   _isSlider:  _fact && !isNaN(_fact.increment)
//                            property bool   _isEdit:    !_isBool && !_isSlider && _fact.enumStrings.length < 1

//                            FactComboBox {
//                                Layout.fillWidth:   true
//                                sizeToContents:     true
//                                fact:               parent._fact
//                                indexModel:         false
//                                visible:            parent._isCombo
//                            }
//                            FactTextField {
//                                Layout.fillWidth:   true
//                                fact:               parent._fact
//                                visible:            parent._isEdit
//                            }
//                            QGCSlider {
//                                Layout.fillWidth:           true
//                                maximumValue:               parent._fact.max
//                                minimumValue:               parent._fact.min
//                                stepSize:                   parent._fact.increment
//                                visible:                    parent._isSlider
//                                updateValueWhileDragging:   false
//                                onValueChanged:             parent._fact.value = value
//                                Component.onCompleted:      value = parent._fact.value
//                            }
//                            QGCSwitch {
//                                checked:        parent._fact ? parent._fact.value : false
//                                visible:        parent._isBool
//                                onClicked:      parent._fact.value = checked ? 1 : 0
//                            }
//                        }
//                    }

//                    QGCComboBox {
//                        Layout.fillWidth:   true
//                        sizeToContents:     true
//                        model:              [ qsTr("Single"), qsTr("Time Lapse") ]
//                        currentIndex:       _mavlinkCamera ? _mavlinkCamera.photoMode : 0
//                        visible:            _mavlinkCameraHasModes
//                        onActivated:        _mavlinkCamera.photoMode = index
//                    }

//                    QGCSlider {
//                        Layout.fillWidth:           true
//                        maximumValue:               60
//                        minimumValue:               1
//                        stepSize:                   1
//                        value:                      _mavlinkCamera ? _mavlinkCamera.photoLapse : 5
//                        displayValue:               true
//                        updateValueWhileDragging:   true
//                        visible:                    _mavlinkCameraInPhotoMode && _mavlinkCamera.photoMode === QGCCameraControl.PHOTO_CAPTURE_TIMELAPSE
//                        onValueChanged: {
//                            if (_mavlinkCamera) {
//                                _mavlinkCamera.photoLapse = value
//                            }
//                        }
//                    }

//                    QGCSwitch {
//                        checked:            _videoStreamSettings.gridLines.rawValue
//                        visible:            _anyVideoStreamAvailable
//                        onClicked:          _videoStreamSettings.gridLines.rawValue = checked ? 1 : 0
//                    }

//                    FactComboBox {
//                        Layout.fillWidth:   true
//                        sizeToContents:     true
//                        fact:               _videoStreamSettings.videoFit
//                        indexModel:         false
//                        visible:            _anyVideoStreamAvailable
//                    }

//                    QGCButton {
//                        Layout.fillWidth:   true
//                        text:               qsTr("Reset")
//                        visible:            _mavlinkCamera
//                        onClicked:          resetPrompt.open()
//                        MessageDialog {
//                            id:                 resetPrompt
//                            title:              qsTr("Reset Camera to Factory Settings")
//                            text:               qsTr("Confirm resetting all settings?")
//                            standardButtons:    StandardButton.Yes | StandardButton.No
//                            onNo: resetPrompt.close()
//                            onYes: {
//                                _mavlinkCamera.resetSettings()
//                                resetPrompt.close()
//                            }
//                        }
//                    }

//                    QGCButton {
//                        Layout.fillWidth:   true
//                        text:               qsTr("Format")
//                        visible:            _mavlinkCameraStorageSupported
//                        onClicked:          formatPrompt.open()
//                        MessageDialog {
//                            id:                 formatPrompt
//                            title:              qsTr("Format Camera Storage")
//                            text:               qsTr("Confirm erasing all files?")
//                            standardButtons:    StandardButton.Yes | StandardButton.No
//                            onNo: formatPrompt.close()
//                            onYes: {
//                                _mavlinkCamera.formatCard()
//                                formatPrompt.close()
//                            }
//                        }
//                    }
//                }

            }
        }
    }
}
