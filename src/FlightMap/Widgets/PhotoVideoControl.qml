/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

import QtQuick
import QtPositioning
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Dialogs

import QGroundControl
import QGroundControl.ScreenTools
import QGroundControl.Controls
import QGroundControl.Palette
import QGroundControl.Vehicle
import QGroundControl.Controllers
import QGroundControl.FactSystem
import QGroundControl.FactControls

Rectangle {
    width:      ScreenTools.defaultFontPixelWidth * 5//mainLayout.width + (_margins * 2)
    height:     ScreenTools.defaultFontPixelWidth * 12//mainLayout.height + (_margins * 2)
    color:      Qt.rgba(qgcPal.window.r, qgcPal.window.g, qgcPal.window.b, 0)
    radius:     _margins
    visible:    true //_camera.capturesVideo || _camera.capturesPhotos

    property real   _margins:                   ScreenTools.defaultFontPixelHeight / 4
    property real   _smallMargins:              ScreenTools.defaultFontPixelWidth / 4
    // property var    _activeVehicle:             globals.activeVehicle
    // property var    _cameraManager:             _activeVehicle.cameraManager
    // property var    _camera:                    _cameraManager.currentCameraInstance
    property bool   _cameraInPhotoMode:         true
    property bool   _cameraInVideoMode:         !_cameraInPhotoMode
    property bool   _videoCaptureIdle:          MavlinkCameraControl.VIDEO_CAPTURE_STATUS_STOPPED
    property bool   _photoCaptureSingleIdle:    MavlinkCameraControl.PHOTO_CAPTURE_IDLE
    property bool   _photoCaptureIdle:          _photoCaptureSingleIdle
    property int    _photoCaptureCount:         0
    property int    _viewRecordSeconds:         0
    property var    _gcu:                       QGroundControl.videoManager.gcu
    QGCPalette { id: qgcPal; colorGroupEnabled: enabled }

    DeadMouseArea { anchors.fill: parent }

    RowLayout {
        id:                 mainLayout
        anchors.margins:    _margins
        anchors.top:        parent.top
        anchors.right:      parent.right
        spacing:            _margins

        ColumnLayout {
            Layout.fillHeight:  true
            spacing:            _margins
            visible:            true//_camera.hasZoom

            // QGCLabel {
            //     Layout.alignment:   Qt.AlignHCenter
            //     text:               qsTr("Zoom")
            //     font.pointSize:     ScreenTools.smallFontPointSize
            // }

            // QGCSlider {
            //     Layout.alignment:   Qt.AlignHCenter
            //     Layout.fillHeight:  true
            //     orientation:        Qt.Vertical
            //     to:                 100
            //     from:               0
            //     value:              _camera.zoomLevel
            //     live:               true
            //     onValueChanged:     _camera.zoomLevel = value
            //     _barHeight:         Math.round(ScreenTools.defaultFontPixelHeight / 4)
            // }
        }
        
        ColumnLayout {
            spacing: _margins * 2

            ColumnLayout {
                spacing: _margins

                // Camera name
                // QGCLabel {
                //     Layout.alignment:   Qt.AlignHCenter
                //     text:               _camera.modelName
                //     visible:            _cameraManager.cameras.length > 1
                // }

                // Photo/Video Mode Selector
                QGCColoredImage {
                    height:             ScreenTools.defaultFontPixelWidth * 3
                    width:              height
                    Layout.alignment:   Qt.AlignHCenter
                    source:             _cameraInPhotoMode ? "/qmlimages/camera_photo.svg" : "/qmlimages/camera_video.svg"
                    fillMode:           Image.PreserveAspectFit
                    sourceSize.height:  height
                    // color:              _cameraInVideoMode ? qgcPal.colorGreen : qgcPal.text

                    MouseArea {
                        anchors.fill:   parent
                        // enabled:        _cameraInPhotoMode ? _photoCaptureIdle : true
                        onClicked:      {//_camera.setCameraModeVideo()
                            _cameraInPhotoMode = !_cameraInPhotoMode
                            console.log("_cameraInPhotoMode:", _cameraInPhotoMode)
                        }
                    }
                }

                // Take Photo, Start/Stop Video button
                Rectangle {
                    Layout.alignment:   Qt.AlignHCenter
                    color:              Qt.rgba(0,0,0,0)
                    width:              ScreenTools.defaultFontPixelWidth * 4
                    height:             width
                    radius:             width * 0.5
                    border.color:       qgcPal.buttonText
                    border.width:       3

                    Rectangle {
                        id:                 rectShoot
                        anchors.centerIn:   parent
                        width:              parent.width * (_isShootingInCurrentMode ? (_cameraInVideoMode ? 0.5 : 0.65) : 0.75)
                        height:             width
                        radius:             _isShootingInCurrentMode ? (_cameraInVideoMode ? 0 : width * 0.5): width * 0.5
                        color:              _cameraInPhotoMode ? qgcPal.colorGrey : qgcPal.colorRed

                        property bool _isShootingInPhotoMode:  false //_cameraInPhotoMode && _camera.photoCaptureStatus === MavlinkCameraControl.PHOTO_CAPTURE_IN_PROGRESS
                        property bool _isShootingInVideoMode:  false//(!_cameraInPhotoMode && _camera.videoCaptureStatus === MavlinkCameraControl.VIDEO_CAPTURE_STATUS_RUNNING)
                        property bool _isShootingInCurrentMode: _cameraInPhotoMode ? _isShootingInPhotoMode : _isShootingInVideoMode
                        property bool _isShootingInOtherMode:   _cameraInPhotoMode ? _isShootingInVideoMode : _isShootingInPhotoMode
                        // property bool _canShootInCurrentMode:   _isShootingInOtherMode ?
                        //                                             (_cameraInPhotoMode ? _camera.photosInVideoMode : _camera.videoInPhotoMode) :
                        //                                             true
                    }
                    Timer {
                        id: timerShooting
                        interval: 500
                        repeat: false
                        onTriggered: {
                            rectShoot._isShootingInPhotoMode = false
                        }
                    }
                    Timer {
                        id: timerVideoRecord
                        interval: 1000
                        repeat: true
                        onTriggered: {
                            _viewRecordSeconds++
                            var hours = Math.floor(_viewRecordSeconds / 3600)
                            var minutes = Math.floor((_viewRecordSeconds % 3600) / 60)
                            var seconds = Math.floor(_viewRecordSeconds % 60)
                            videoRecordTime.text = hours.toString().padStart(2, '0') + ":" + minutes.toString().padStart(2, '0') + ":" + seconds.toString().padStart(2, '0')
                            console.log("videoRecordTime.text: ", videoRecordTime.text, _viewRecordSeconds)
                        }
                    }
                    MouseArea {
                        anchors.fill:   parent
                        onClicked:      toggleShooting()

                        function toggleShooting() {
                            console.log("toggleShooting(), _cameraInPhotoMode: ", _cameraInPhotoMode, rectShoot._isShootingInVideoMode)
                            if (_cameraInPhotoMode) {
                                rectShoot._isShootingInPhotoMode = true
                                _photoCaptureCount++
                                timerShooting.start()
                                _gcu.takePhoto()
                            } else {
                                if (rectShoot._isShootingInVideoMode){
                                    rectShoot._isShootingInVideoMode = false
                                    timerVideoRecord.stop()
                                    videoRecordTime.text = "00:00:00"
                                    _viewRecordSeconds = 0
                                    _gcu.startRecording(false)
                                } else {
                                    rectShoot._isShootingInVideoMode = true
                                    _viewRecordSeconds = 0
                                    timerVideoRecord.start()
                                    _gcu.startRecording(true)
                                }
                            }
                        }
                    }
                }

                // Record time / Capture count
                Rectangle {
                    Layout.alignment:       Qt.AlignHCenter
                    color:                  "transparent"//!_videoCaptureIdle && !_photoCaptureIdle ? "transparent" : qgcPal.colorRed
                    Layout.preferredWidth:  videoRecordTime.width//(_cameraInVideoMode ? videoRecordTime.width : photoCaptureCount.width) + (_smallMargins * 2)
                    Layout.preferredHeight: videoRecordTime.height//(_cameraInVideoMode ? videoRecordTime.height : photoCaptureCount.height)
                    radius:                 _margins / 2

                    // Video record time
                    QGCLabel {
                        id:                 videoRecordTime
                        anchors.leftMargin: _smallMargins
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top:        parent.top
                        text:               "00:00:00"
                        font.pointSize:     ScreenTools.largeFontPointSize * 0.7
                        visible:            _cameraInVideoMode
                    }

                    // Photo capture count
                    QGCLabel {
                        id:                 photoCaptureCount
                        anchors.leftMargin: _smallMargins
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top:        parent.top
                        text:               ('00000' + _photoCaptureCount).slice(-5)
                        font.pointSize:     ScreenTools.largeFontPointSize * 0.7
                        visible:            _cameraInPhotoMode
                    }
                }

                //-- Status Information
                // ColumnLayout {
                //     Layout.alignment:   Qt.AlignHCenter
                //     spacing:            0

                //     QGCLabel {
                //         Layout.alignment:   Qt.AlignHCenter
                //         text:               qsTr("Free Space: ") + _camera.storageFreeStr
                //         font.pointSize:     ScreenTools.defaultFontPointSize
                //         visible:            _camera.storageStatus === MavlinkCameraControl.STORAGE_READY
                //     }

                //     QGCLabel {
                //         Layout.alignment:   Qt.AlignHCenter
                //         text:               qsTr("Battery: ") + _camera.batteryRemainingStr
                //         font.pointSize:     ScreenTools.defaultFontPointSize
                //         visible:            _camera.batteryRemaining >= 0
                //     }
                // }
            }

            /*
            ColumnLayout {
                id:                 trackingControls
                Layout.alignment:   Qt.AlignHCenter
                spacing:            _margins
                visible:            true//_camera && _camera.hasTracking

                Rectangle {
                    Layout.alignment:       Qt.AlignHCenter
                    color:                  _camera.trackingEnabled ? qgcPal.colorRed : qgcPal.windowShadeLight
                    Layout.preferredWidth:  ScreenTools.defaultFontPixelWidth * 6
                    Layout.preferredHeight: Layout.preferredWidth
                    border.color:           qgcPal.buttonText
                    border.width:           3
                    
                    QGCColoredImage {
                        height:             parent.height * 0.5
                        width:              height
                        anchors.centerIn:   parent
                        source:             "/qmlimages/TrackingIcon.svg"
                        fillMode:           Image.PreserveAspectFit
                        sourceSize.height:  height
                        color:              qgcPal.text

                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                _camera.trackingEnabled = !_camera.trackingEnabled;
                                if (!_camera.trackingEnabled) {
                                    !camera.stopTracking()
                                }
                            }
                        }
                    }
                }

                // QGCLabel {
                //     Layout.alignment:   Qt.AlignHCenter
                //     text:               qsTr("Camera Tracking")
                //     font.pointSize:     ScreenTools.defaultFontPointSize
                //     visible:            _camera && _camera.hasTracking
                // }
            }
            */

            // QGCColoredImage {
            //     Layout.alignment:       Qt.AlignHCenter
            //     source:                 "/res/gear-black.svg"
            //     mipmap:                 true
            //     Layout.preferredHeight: ScreenTools.defaultFontPixelHeight * 1.5
            //     Layout.preferredWidth:  Layout.preferredHeight
            //     sourceSize.height:      Layout.preferredHeight
            //     color:                  qgcPal.text
            //     fillMode:               Image.PreserveAspectFit

            //     QGCMouseArea {
            //         fillItem:   parent
            //         onClicked:  settingsDialogComponent.createObject(mainWindow).open()
            //     }
            // }
        }

        Component {
            id: settingsDialogComponent

            QGCPopupDialog {
                title:      qsTr("Settings")
                buttons:    Dialog.Close

                property bool _multipleMavlinkCameras:          _cameraManager.cameras.count > 1
                property bool _multipleMavlinkCameraStreams:    _camera.streamLabels.length > 1
                property bool _cameraStorageSupported:          _camera.storageStatus !== MavlinkCameraControl.STORAGE_NOT_SUPPORTED
                property var  _videoSettings:                   QGroundControl.settingsManager.videoSettings

                ColumnLayout {
                    spacing: _margins

                    GridLayout {
                        id:     gridLayout
                        flow:   GridLayout.TopToBottom
                        rows:   dynamicRows + _camera.activeSettings.length

                        property int dynamicRows: 10

                        // First column
                        QGCLabel {
                            text:               qsTr("Camera")
                            visible:            _multipleMavlinkCameras
                            onVisibleChanged:   gridLayout.dynamicRows += visible ? 1 : -1
                        }

                        QGCLabel {
                            text:               qsTr("Video Stream")
                            visible:            _multipleMavlinkCameraStreams
                            onVisibleChanged:   gridLayout.dynamicRows += visible ? 1 : -1
                        }

                        QGCLabel {
                            text:               qsTr("Thermal View Mode")
                            visible:            _camera.thermalStreamInstance
                            onVisibleChanged:   gridLayout.dynamicRows += visible ? 1 : -1
                        }

                        QGCLabel {
                            text:               qsTr("Blend Opacity")
                            visible:            _camera.thermalStreamInstance && _camera.thermalMode === MavlinkCameraControl.THERMAL_BLEND
                            onVisibleChanged:   gridLayout.dynamicRows += visible ? 1 : -1
                        }

                        // Mavlink Camera Protocol active settings
                        Repeater {
                            model: _camera.activeSettings

                            QGCLabel {
                                text: _camera.getFact(modelData).shortDescription
                            }
                        }

                        QGCLabel {
                            text:               qsTr("Photo Mode")
                            visible:            _camera.capturesPhotos
                            onVisibleChanged:   gridLayout.dynamicRows += visible ? 1 : -1
                        }

                        QGCLabel {
                            text:               qsTr("Photo Interval (seconds)")
                            visible:            _camera.capturesPhotos && _camera.photoCaptureMode === MavlinkCameraControl.PHOTO_CAPTURE_TIMELAPSE
                            onVisibleChanged:   gridLayout.dynamicRows += visible ? 1 : -1
                        }

                        QGCLabel {
                            text:               qsTr("Video Grid Lines")
                            visible:            _camera.hasVideoStream
                            onVisibleChanged:   gridLayout.dynamicRows += visible ? 1 : -1
                        }

                        QGCLabel {
                            text:               qsTr("Video Screen Fit")
                            visible:            _camera.hasVideoStream
                            onVisibleChanged:   gridLayout.dynamicRows += visible ? 1 : -1
                        }

                        QGCLabel {
                            text:               qsTr("Reset Camera Defaults")
                            onVisibleChanged:   gridLayout.dynamicRows += visible ? 1 : -1
                        }

                        QGCLabel {
                            text:               qsTr("Storage")
                            visible:            _cameraStorageSupported
                            onVisibleChanged:   gridLayout.dynamicRows += visible ? 1 : -1
                        }

                        // Second column
                        QGCComboBox {
                            Layout.fillWidth:   true
                            sizeToContents:     true
                            model:              _cameraManager.cameraLabels
                            currentIndex:       _cameraManager.currentCamera
                            visible:            _multipleMavlinkCameras
                            onActivated:        (index) => { _cameraManager.currentCamera = index }
                        }

                        QGCComboBox {
                            Layout.fillWidth:   true
                            sizeToContents:     true
                            model:              _camera.streamLabels
                            currentIndex:       _camera.currentStream
                            visible:            _multipleMavlinkCameraStreams
                            onActivated:        (index) => { _camera.currentStream = index }
                        }

                        QGCComboBox {
                            Layout.fillWidth:   true
                            sizeToContents:     true
                            model:              [ qsTr("Off"), qsTr("Blend"), qsTr("Full"), qsTr("Picture In Picture") ]
                            currentIndex:       _camera.thermalMode
                            visible:            _camera.thermalStreamInstance
                            onActivated:        (index) => { _camera.thermalMode = index }
                        }

                        QGCSlider {
                            Layout.fillWidth:   true
                            to:                 100
                            from:               0
                            value:              _camera.thermalOpacity
                            live:               true
                            visible:            _camera.thermalStreamInstance && _camera.thermalMode === MavlinkCameraControl.THERMAL_BLEND
                            onValueChanged:     _camera.thermalOpacity = value
                        }

                        // Mavlink Camera Protocol active settings
                        Repeater {
                            model: _camera.activeSettings

                            RowLayout {
                                Layout.fillWidth:   true
                                spacing:            ScreenTools.defaultFontPixelWidth

                                property var    _fact:      _camera.getFact(modelData)
                                property bool   _isBool:    _fact.typeIsBool
                                property bool   _isCombo:   !_isBool && _fact.enumStrings.length > 0
                                property bool   _isSlider:  _fact && !isNaN(_fact.increment)
                                property bool   _isEdit:    !_isBool && !_isSlider && _fact.enumStrings.length < 1

                                FactComboBox {
                                    Layout.fillWidth:   true
                                    sizeToContents:     true
                                    fact:               parent._fact
                                    indexModel:         false
                                    visible:            parent._isCombo
                                }
                                FactTextField {
                                    Layout.fillWidth:   true
                                    fact:               parent._fact
                                    visible:            parent._isEdit
                                }
                                QGCSlider {
                                    Layout.fillWidth:           true
                                    to:               parent._fact.max
                                    from:               parent._fact.min
                                    stepSize:                   parent._fact.increment
                                    visible:                    parent._isSlider
                                    live:   false
                                    property bool initialized:  false

                                    onValueChanged: {
                                        if (!initialized) {
                                            return
                                        }
                                        parent._fact.value = value
                                    }

                                    Component.onCompleted: {
                                        value = parent._fact.value
                                        initialized = true
                                    }
                                }
                                QGCSwitch {
                                    checked:    parent._fact ? parent._fact.value : false
                                    visible:    parent._isBool
                                    onClicked:  parent._fact.value = checked ? 1 : 0
                                }
                            }
                        }

                        QGCComboBox {
                            Layout.fillWidth:   true
                            sizeToContents:     true
                            model:              [ qsTr("Single"), qsTr("Time Lapse") ]
                            currentIndex:       _camera.photoCaptureMode
                            visible:            _camera.capturesPhotos
                            onActivated:        (index) => { _camera.photoCaptureMode = index }
                        }

                        QGCSlider {
                            Layout.fillWidth:   true
                            to:                 60
                            from:               1
                            stepSize:           1
                            value:              _camera.photoLapse
                            displayValue:       true
                            live:               true
                            visible:            _camera.capturesPhotos && _camera.photoCaptureMode === MavlinkCameraControl.PHOTO_CAPTURE_TIMELAPSE
                            onValueChanged:     _camera.photoLapse = value
                        }

                        QGCSwitch {
                            checked:    _videoSettings.gridLines.rawValue
                            visible:    _camera.hasVideoStream
                            onClicked:  _videoSettings.gridLines.rawValue = checked ? 1 : 0
                        }

                        FactComboBox {
                            Layout.fillWidth:   true
                            sizeToContents:     true
                            fact:               _videoSettings.videoFit
                            indexModel:         false
                            visible:            _camera.hasVideoStream
                        }

                        QGCButton {
                            Layout.fillWidth:   true
                            text:               qsTr("Reset")
                            onClicked:          resetPrompt.open()
                            MessageDialog {
                                id:                 resetPrompt
                                title:              qsTr("Reset Camera to Factory Settings")
                                text:               qsTr("Confirm resetting all settings?")
                                buttons:            MessageDialog.Yes | MessageDialog.No

                                onButtonClicked: function (button, role) {
                                    switch (button) {
                                    case MessageDialog.Yes:
                                        _camera.resetSettings()
                                        resetPrompt.close()
                                        break;
                                    case MessageDialog.No:
                                        resetPrompt.close()
                                        break;
                                    }
                                }
                            }
                        }

                        QGCButton {
                            Layout.fillWidth:   true
                            text:               qsTr("Format")
                            visible:            _cameraStorageSupported
                            onClicked:          formatPrompt.open()
                            MessageDialog {
                                id:                 formatPrompt
                                title:              qsTr("Format Camera Storage")
                                text:               qsTr("Confirm erasing all files?")
                                buttons:            MessageDialog.Yes | MessageDialog.No

                                onButtonClicked: function (button, role) {
                                    switch (button) {
                                    case MessageDialog.Yes:
                                        _camera.formatCard()
                                        formatPrompt.close()
                                        break;
                                    case MessageDialog.No:
                                        formatPrompt.close()
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
