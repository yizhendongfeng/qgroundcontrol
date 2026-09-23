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
import QtLocation
import QtPositioning
import QtQuick.Layouts
import QtQuick.Window

import QGroundControl
import QGroundControl.FlightMap
import QGroundControl.ScreenTools
import QGroundControl.Controls
import QGroundControl.FactSystem
import QGroundControl.FactControls
import QGroundControl.Palette
import QGroundControl.Controllers
import QGroundControl.ShapeFileHelper
import QGroundControl.FlightDisplay
import QGroundControl.Controllers       1.0
// import QGroundControl.UTMSP


Item {
    id: _root

    property bool planControlColapsed: false

    readonly property int   _decimalPlaces:             8
    readonly property real  _margin:                    ScreenTools.defaultFontPixelHeight * 0.5
    readonly property real  _toolsMargin:               ScreenTools.defaultFontPixelWidth * 0.75
    readonly property real  _radius:                    ScreenTools.defaultFontPixelWidth  * 0.5
    readonly property real  _rightPanelWidth:           300//Math.min(width / 3, ScreenTools.defaultFontPixelWidth * 30)
    readonly property var   _defaultVehicleCoordinate:  QtPositioning.coordinate(37.803784, -122.462276)
    readonly property bool  _waypointsOnlyMode:         QGroundControl.corePlugin.options.missionWaypointsOnly

    property var    _planMasterController:              planMasterController
    property var    _missionController:                 _planMasterController.missionController
    property var    _geoFenceController:                _planMasterController.geoFenceController
    property var    _rallyPointController:              _planMasterController.rallyPointController
    property var    _visualItems:                       _missionController.visualItems
    property bool   _lightWidgetBorders:                editorMap.isSatelliteMap
    property bool   _addROIOnClick:                     false
    property bool   _singleComplexItem:                 _missionController.complexMissionItemNames.length === 1
     property int    _editingLayer:                      layerTabBar.currentIndex ? _layers[layerTabBar.currentIndex] : _layerMission // _layerMission//
    property int    _toolStripBottom:                   toolStrip.height + toolStrip.y
    property var    _appSettings:                       QGroundControl.settingsManager.appSettings
    property var    _planViewSettings:                  QGroundControl.settingsManager.planViewSettings
    property bool   _promptForPlanUsageShowing:         false
    // property bool   _utmspEnabled:                      QGroundControl.utmspSupported
    property bool   _resetGeofencePolygon:              false   //Reset the Geofence Polygon
    property var    _vehicleID
    property bool   _triggerSubmit
    property bool   _resetRegisterFlightPlan
    property string _currentPlanFileDir:                    _appSettings.missionSavePath
    property string _currentPlanFileName:                   ""
    readonly property var       _layers:                    [_layerMission, _layerGeoFence, _layerRallyPoints, _layerCloudElements]
    // readonly property var       _layersUTMSP:               [_layerMission, _layerRallyPoints, _layerUTMSP] //Adds additional UTMSP layer
    property var                _itemCurrentWaypoint:      0//_itemCurrentBank ? _itemCurrentBank.itemCurrentWaypoint : null
    readonly property int       _layerMission:              1
    readonly property int       _layerGeoFence:             2
    readonly property int       _layerRallyPoints:          3
    readonly property int       _layerCloudElements:        4
    // readonly property int       _layerUTMSP:                4 // Additional Tab button when UTMSP is enabled
    readonly property string    _armedVehicleUploadPrompt:  qsTr("Vehicle is currently armed. Do you want to upload the mission to the vehicle?")
    property bool   _modePlanEdit:              false
    property real   _dataFontSize:              ScreenTools.defaultFontPointSize
    property real   _largeValueWidth:           ScreenTools.defaultFontPixelWidth * 8
    property real   _mediumValueWidth:          ScreenTools.defaultFontPixelWidth * 4
    property real   _smallValueWidth:           ScreenTools.defaultFontPixelWidth * 3
    property real   _labelToValueSpacing:       ScreenTools.defaultFontPixelWidth

    // 航线参数
    property bool   _controllerValid:           _planMasterController !== undefined && _planMasterController !== null
    property var    missionItems:               _controllerValid ? _missionController.visualItems : undefined
    property real   missionPlanedDistance:      _controllerValid ? _missionController.missionPlannedDistance : NaN
    property real   missionTotalDistance:       _controllerValid ? _missionController.missionTotalDistance : NaN

    property real   missionTime:                _controllerValid ? _missionController.missionTime : 0
    property real   missionMaxTelemetry:        _controllerValid ? _missionController.missionMaxTelemetry : NaN
    property real   _missionMaxTelemetry:       _missionValid ? missionMaxTelemetry : NaN
    property string _missionMaxTelemetryText:       isNaN(_missionMaxTelemetry) ?       "-.-" : QGroundControl.unitsConversion.metersToAppSettingsHorizontalDistanceUnits(_missionMaxTelemetry).toFixed(1) + " " + QGroundControl.unitsConversion.appSettingsHorizontalDistanceUnitsString

    property bool   missionDirty:               _controllerValid ? _planMasterController.missionController.dirty : false
    property string _missionPlannedDistanceText: isNaN(_missionPlanedDistance) ? "-.-" : QGroundControl.unitsConversion.metersToAppSettingsHorizontalDistanceUnits(_missionPlanedDistance).toFixed(1) + " " + QGroundControl.unitsConversion.appSettingsHorizontalDistanceUnitsString
    property string _missionTotalDistanceText: isNaN(_missionTotalDistance) ? "-.-" : QGroundControl.unitsConversion.metersToAppSettingsHorizontalDistanceUnits(_missionTotalDistance).toFixed(1) + " " + QGroundControl.unitsConversion.appSettingsHorizontalDistanceUnitsString
    property bool   _missionValid:              missionItems !== undefined
    property real   _missionPlanedDistance: _missionValid ? missionPlanedDistance : NaN
    property real   _missionTotalDistance: _missionValid ? missionTotalDistance : NaN
    property real   _missionTime:               _missionValid ? missionTime : 0

    function mapCenter() {
        var coordinate = editorMap.center
        coordinate.latitude  = coordinate.latitude.toFixed(_decimalPlaces)
        coordinate.longitude = coordinate.longitude.toFixed(_decimalPlaces)
        coordinate.altitude  = coordinate.altitude.toFixed(_decimalPlaces)
        return coordinate
    }
    function getMissionTime() {
        if (!_missionTime) {
            return "00:00:00"
        }
        var t = new Date(2021, 0, 0, 0, 0, Number(_missionTime))
        var days = Qt.formatDateTime(t, 'dd')
        var complete

        if (days == 31) {
            days = '0'
            complete = Qt.formatTime(t, 'hh:mm:ss')
        } else {
            complete = days + " days " + Qt.formatTime(t, 'hh:mm:ss')
        }
        return complete
    }
    property bool _firstMissionLoadComplete:    false
    property bool _firstFenceLoadComplete:      false
    property bool _firstRallyLoadComplete:      false
    property bool _firstLoadComplete:           false

    MapFitFunctions {
        id:                         mapFitFunctions  // The name for this id cannot be changed without breaking references outside of this code. Beware!
        map:                        editorMap
        usePlannedHomePosition:     true
        planMasterController:       _planMasterController
    }

    onVisibleChanged: {
        if(visible) {
            editorMap.zoomLevel = QGroundControl.flightMapZoom
            editorMap.center    = QGroundControl.flightMapPosition
            if (!_planMasterController.containsItems) {
                toolStrip.simulateClick(toolStrip.fileButtonIndex)
            }
        }
    }

    Connections {
        target: _appSettings ? _appSettings.defaultMissionItemAltitude : null
        function onRawValueChanged() {
            if (_visualItems.count > 1) {
                mainWindow.showMessageDialog(qsTr("Apply new altitude"),
                                             qsTr("You have changed the default altitude for mission items. Would you like to apply that altitude to all the items in the current mission?"),
                                             Dialog.Yes | Dialog.No,
                                             function() { _missionController.applyDefaultMissionAltitude() })
            }
        }
    }

    Component {
        id: promptForPlanUsageOnVehicleChangePopupComponent
        QGCPopupDialog {
            title:      _planMasterController.managerVehicle.isOfflineEditingVehicle ? qsTr("Plan View - Vehicle Disconnected") : qsTr("Plan View - Vehicle Changed")
            buttons:    Dialog.NoButton

            ColumnLayout {
                QGCLabel {
                    Layout.maximumWidth:    parent.width
                    wrapMode:               QGCLabel.WordWrap
                    text:                   _planMasterController.managerVehicle.isOfflineEditingVehicle ?
                                                qsTr("The vehicle associated with the plan in the Plan View is no longer available. What would you like to do with that plan?") :
                                                qsTr("The plan being worked on in the Plan View is not from the current vehicle. What would you like to do with that plan?")
                }

                QGCButton {
                    Layout.fillWidth:   true
                    text:               _planMasterController.dirty ?
                                            (_planMasterController.managerVehicle.isOfflineEditingVehicle ?
                                                 qsTr("Discard Unsaved Changes") :
                                                 qsTr("Discard Unsaved Changes, Load New Plan From Vehicle")) :
                                            qsTr("Load New Plan From Vehicle")
                    onClicked: {
                        _planMasterController.showPlanFromManagerVehicle()
                        _promptForPlanUsageShowing = false
                        close();
                    }
                }

                QGCButton {
                    Layout.fillWidth:   true
                    text:               _planMasterController.managerVehicle.isOfflineEditingVehicle ?
                                            qsTr("Keep Current Plan") :
                                            qsTr("Keep Current Plan, Don't Update From Vehicle")
                    onClicked: {
                        if (!_planMasterController.managerVehicle.isOfflineEditingVehicle) {
                            _planMasterController.dirty = true
                        }
                        _promptForPlanUsageShowing = false
                        close()
                    }
                }
            }
        }
    }

    PlanMasterController {
        id:         planMasterController
        flyView:    false

        Component.onCompleted: {
            _planMasterController.start()
            _missionController.setCurrentPlanViewSeqNum(0, true)
        }

        onPromptForPlanUsageOnVehicleChange: {
            if (!_promptForPlanUsageShowing) {
                _promptForPlanUsageShowing = true
                promptForPlanUsageOnVehicleChangePopupComponent.createObject(mainWindow).open()
            }
        }

        function waitingOnIncompleteDataMessage(save) {
            var saveOrUpload = save ? qsTr("Save") : qsTr("Upload")
            mainWindow.showMessageDialog(qsTr("Unable to %1").arg(saveOrUpload), qsTr("Plan has incomplete items. Complete all items and %1 again.").arg(saveOrUpload))
        }

        function waitingOnTerrainDataMessage(save) {
            var saveOrUpload = save ? qsTr("Save") : qsTr("Upload")
            mainWindow.showMessageDialog(qsTr("Unable to %1").arg(saveOrUpload), qsTr("Plan is waiting on terrain data from server for correct altitude values."))
        }

        function checkReadyForSaveUpload(save) {
            if (readyForSaveState() == VisualMissionItem.NotReadyForSaveData) {
                waitingOnIncompleteDataMessage(save)
                return false
            } else if (readyForSaveState() == VisualMissionItem.NotReadyForSaveTerrain) {
                waitingOnTerrainDataMessage(save)
                return false
            }
            return true
        }

        function upload() {
            if (!checkReadyForSaveUpload(false /* save */)) {
                return
            }
            switch (_missionController.sendToVehiclePreCheck()) {
                case MissionController.SendToVehiclePreCheckStateOk:
                    sendToVehicle()
                    break
                case MissionController.SendToVehiclePreCheckStateActiveMission:
                    mainWindow.showMessageDialog(qsTr("Send To Vehicle"), qsTr("Current mission must be paused prior to uploading a new Plan"))
                    break
                case MissionController.SendToVehiclePreCheckStateFirwmareVehicleMismatch:
                    mainWindow.showMessageDialog(qsTr("Plan Upload"),
                                                 qsTr("This Plan was created for a different firmware or vehicle type than the firmware/vehicle type of vehicle you are uploading to. " +
                                                      "This can lead to errors or incorrect behavior. " +
                                                      "It is recommended to recreate the Plan for the correct firmware/vehicle type.\n\n" +
                                                      "Click 'Ok' to upload the Plan anyway."),
                                                 Dialog.Ok | Dialog.Cancel,
                                                 function() { _planMasterController.sendToVehicle() })
                    break
            }
        }

        function loadFromSelectedFile() {
            fileDialog.title =          qsTr("Select Plan File")
            fileDialog.planFiles =      true
            fileDialog.nameFilters =    _planMasterController.loadNameFilters
            fileDialog.openForLoad()
        }

        function saveToSelectedFile() {
            if (!checkReadyForSaveUpload(true /* save */)) {
                return
            }
            fileDialog.title =          qsTr("Save Plan")
            fileDialog.planFiles =      true
            fileDialog.nameFilters =    _planMasterController.saveNameFilters
            fileDialog.openForSave()
        }

        function fitViewportToItems() {
            mapFitFunctions.fitMapViewportToMissionItems()
        }

        function saveKmlToSelectedFile() {
            if (!checkReadyForSaveUpload(true /* save */)) {
                return
            }
            fileDialog.title =          qsTr("Save KML")
            fileDialog.planFiles =      false
            fileDialog.nameFilters =    ShapeFileHelper.fileDialogKMLFilters
            fileDialog.openForSave()
        }
    }

    Connections {
        target: _missionController

        function onNewItemsFromVehicle() {
            if (_visualItems && _visualItems.count !== 1) {
                mapFitFunctions.fitMapViewportToMissionItems()
            }
            _missionController.setCurrentPlanViewSeqNum(0, true)
        }
    }

    function insertSimpleItemAfterCurrent(coordinate) {
        var nextIndex = _missionController.currentPlanViewVIIndex + 1
        _missionController.insertSimpleMissionItem(coordinate, nextIndex, true /* makeCurrentItem */)
    }

    function insertROIAfterCurrent(coordinate) {
        var nextIndex = _missionController.currentPlanViewVIIndex + 1
        _missionController.insertROIMissionItem(coordinate, nextIndex, true /* makeCurrentItem */)
    }

    function insertCancelROIAfterCurrent() {
        var nextIndex = _missionController.currentPlanViewVIIndex + 1
        _missionController.insertCancelROIMissionItem(nextIndex, true /* makeCurrentItem */)
    }

    function insertComplexItemAfterCurrent(complexItemName) {
        var nextIndex = _missionController.currentPlanViewVIIndex + 1
        _missionController.insertComplexMissionItem(complexItemName, mapCenter(), nextIndex, true /* makeCurrentItem */)
    }

    function insertTakeItemAfterCurrent() {
        var nextIndex = _missionController.currentPlanViewVIIndex + 1
        _missionController.insertTakeoffItem(mapCenter(), nextIndex, true /* makeCurrentItem */)
    }

    function insertLandItemAfterCurrent() {
        var nextIndex = _missionController.currentPlanViewVIIndex + 1
        _missionController.insertLandItem(mapCenter(), nextIndex, true /* makeCurrentItem */)
    }


    function selectNextNotReady() {
        var foundCurrent = false
        for (var i=0; i<_missionController.visualItems.count; i++) {
            var vmi = _missionController.visualItems.get(i)
            if (vmi.readyForSaveState === VisualMissionItem.NotReadyForSaveData) {
                _missionController.setCurrentPlanViewSeqNum(vmi.sequenceNumber, true)
                break
            }
        }
    }


    function enterPlanEditMode(enter) {
        _modePlanEdit = enter
        if (enter) {
            toolStrip.showWidget(true)
            leftPanel.showWidget(false)
            rightPanel.showWidget(true)
        } else {
            toolStrip.showWidget(false)
            leftPanel.showWidget(true)
            rightPanel.showWidget(false)
        }
    }

    QGCFileDialog {
        id:             fileDialog
        folder:         _appSettings ? _appSettings.missionSavePath : ""

        property bool planFiles: true    ///< true: working with plan files, false: working with kml file

        onAcceptedForSave: (file) => {
            if (planFiles) {
                _planMasterController.saveToFile(file)
            } else {
                _planMasterController.saveToKml(file)
            }
            close()
        }

        onAcceptedForLoad: (file) => {
            _planMasterController.loadFromFile(file)
            _planMasterController.fitViewportToItems()
            _missionController.setCurrentPlanViewSeqNum(0, true)
            close()
        }
    }

    // PlanViewToolBar {
    //     id:                     planToolBar
    //     planMasterController:   _planMasterController
    // }

    Item {
        id:             panel
        anchors.left:   parent.left
        anchors.right:  parent.right
        anchors.top:    parent.top
        anchors.bottom: parent.bottom

        FlightMap {
            id:                         editorMap
            anchors.fill:               parent
            mapName:                    "MissionEditor"
            allowGCSLocationCenter:     true
            allowVehicleLocationCenter: true
            planView:                   true

            zoomLevel:                  QGroundControl.flightMapZoom
            center:                     QGroundControl.flightMapPosition

            // This is the center rectangle of the map which is not obscured by tools
            property rect centerViewport:   Qt.rect(_leftToolWidth + _margin,  _margin, editorMap.width - _leftToolWidth - _rightToolWidth - (_margin * 2), (terrainStatus.visible ? terrainStatus.y : height - _margin) - _margin)

            property real _leftToolWidth:       toolStrip.x + toolStrip.width
            property real _rightToolWidth:      rightPanel.width + rightPanel.anchors.rightMargin
            property real _nonInteractiveOpacity:  0.5

            // Initial map position duplicates Fly view position
            Component.onCompleted: editorMap.center = QGroundControl.flightMapPosition

            QGCMapPalette { id: mapPal; lightColors: editorMap.isSatelliteMap }

            onZoomLevelChanged: {
                QGroundControl.flightMapZoom = editorMap.zoomLevel
            }
            onCenterChanged: {
                QGroundControl.flightMapPosition = editorMap.center
            }

            onMapClicked: (mouse) => { //(mouse) =>
                // Take focus to close any previous editing
                editorMap.focus = true
                if (!mainWindow.allowViewSwitch()) {
                    return
                }
                var coordinate = editorMap.toCoordinate(Qt.point(mouse.x, mouse.y), false /* clipToViewPort */)
                coordinate.latitude = coordinate.latitude.toFixed(_decimalPlaces)
                coordinate.longitude = coordinate.longitude.toFixed(_decimalPlaces)
                coordinate.altitude = coordinate.altitude.toFixed(_decimalPlaces)
                // if(_utmspEnabled){
    //             	QGroundControl.utmspManager.utmspVehicle.updateLastCoordinates(coordinate.latitude, coordinate.longitude)
    //             }
                
                switch (_editingLayer) {
                case _layerMission:
                    if (addWaypointRallyPointAction.checked) {
                        insertSimpleItemAfterCurrent(coordinate)
                    } else if (_addROIOnClick) {
                        insertROIAfterCurrent(coordinate)
                        _addROIOnClick = false
                    }

                    break
                case _layerRallyPoints:
                    console.log("onMapClicked _layerRallyPoints")
                    if (_rallyPointController.supported && addWaypointRallyPointAction.checked) {
                        _rallyPointController.addPoint(coordinate)
                        console.log("_rallyPointController.addPoint", coordinate)
                    }
                    break

                // case _layerUTMSP:
                //     if (addWaypointRallyPointAction.checked) {
                //     	insertSimpleItemAfterCurrent(coordinate)
                //     } else if (_addROIOnClick) {
                //     	insertROIAfterCurrent(coordinate)
                //         _addROIOnClick = false
                //     }
                //     break
                }
            }

            // Add the mission item visuals to the map
            Repeater {
                model: _missionController.visualItems
                delegate: MissionItemMapVisual {
                    map:         editorMap
                    opacity:     _editingLayer == _layerMission /*|| _editingLayer == _layerUTMSP*/ ? 1 : editorMap._nonInteractiveOpacity
                    interactive: _editingLayer == _layerMission/* || _editingLayer == _layerUTMSP*/
                    vehicle:     _planMasterController.controllerVehicle
                    onClicked:   (sequenceNumber) => { _missionController.setCurrentPlanViewSeqNum(sequenceNumber, false) }
                }
            }

            // Add lines between waypoints
            MissionLineView {
                showSpecialVisual:  _missionController.isROIBeginCurrentItem
                model:              _missionController.simpleFlightPathSegments
                opacity:            _editingLayer == _layerMission /*||  _editingLayer == _layerUTMSP*/  ? 1 : editorMap._nonInteractiveOpacity
            }

            // Direction arrows in waypoint lines
            MapItemView {
                model: _editingLayer == _layerMission/* ||_editingLayer == _layerUTMSP*/ ? _missionController.directionArrows : undefined

                delegate: MapLineArrow {
                    fromCoord:      object ? object.coordinate1 : undefined
                    toCoord:        object ? object.coordinate2 : undefined
                    arrowPosition:  3
                    z:              QGroundControl.zOrderWaypointLines + 1
                }
            }

            // Incomplete segment lines
            MapItemView {
                model: _missionController.incompleteComplexItemLines

                delegate: MapPolyline {
                    path:       [ object.coordinate1, object.coordinate2 ]
                    line.width: 1
                    line.color: "red"
                    z:          QGroundControl.zOrderWaypointLines
                    opacity:    _editingLayer == _layerMission ? 1 : editorMap._nonInteractiveOpacity
                }
            }

            // 云端航线的航迹。只在云端那页显示 —— 这页看的是库里那条航线，
            // 和编辑器里当前的任务不是一回事，混着画会让人以为任务被改了
            MapPolyline {
                path:       _root._cloudRouteCoords
                line.width: 3
                line.color: "#2f7fd1"
                visible:    planTabBar.currentIndex === 1 && _root._cloudRouteCoords.length > 1
                z:          QGroundControl.zOrderWaypointLines
            }

            // UI for splitting the current segment
            MapQuickItem {
                id:             splitSegmentItem
                anchorPoint.x:  sourceItem.width / 2
                anchorPoint.y:  sourceItem.height / 2
                z:              QGroundControl.zOrderWaypointLines + 1
                visible:        _editingLayer == _layerMission //||  _editingLayer == _layerUTMSP

                sourceItem: SplitIndicator {
                    onClicked:  _missionController.insertSimpleMissionItem(splitSegmentItem.coordinate,
                                                                           _missionController.currentPlanViewVIIndex,
                                                                           true /* makeCurrentItem */)
                }

                function _updateSplitCoord() {
                    if (_missionController.splitSegment) {
                        var distance = _missionController.splitSegment.coordinate1.distanceTo(_missionController.splitSegment.coordinate2)
                        var azimuth = _missionController.splitSegment.coordinate1.azimuthTo(_missionController.splitSegment.coordinate2)
                        splitSegmentItem.coordinate = _missionController.splitSegment.coordinate1.atDistanceAndAzimuth(distance / 2, azimuth)
                    } else {
                        coordinate = QtPositioning.coordinate()
                    }
                }

                Connections {
                    target:                 _missionController
                    function onSplitSegmentChanged()  { splitSegmentItem._updateSplitCoord() }
                }

                Connections {
                    target:                 _missionController.splitSegment
                    function onCoordinate1Changed()   { splitSegmentItem._updateSplitCoord() }
                    function onCoordinate2Changed()   { splitSegmentItem._updateSplitCoord() }
                }
            }

            // Add the vehicles to the map
            MapItemView {
                model: QGroundControl.multiVehicleManager.vehicles
                delegate: VehicleMapItem {
                    vehicle:        object
                    coordinate:     object.coordinate
                    map:            editorMap
                    size:           ScreenTools.defaultFontPixelHeight * 3
                    z:              QGroundControl.zOrderMapItems - 1
                }
            }

            GeoFenceMapVisuals {
                map:                    editorMap
                myGeoFenceController:   _geoFenceController
                interactive:            _editingLayer == _layerGeoFence
                homePosition:           _missionController.plannedHomePosition
                planView:               true
                opacity:                _editingLayer != _layerGeoFence ? editorMap._nonInteractiveOpacity : 1
            }

            RallyPointMapVisuals {
                map:                    editorMap
                myRallyPointController: _rallyPointController
                interactive:            _editingLayer == _layerRallyPoints
                planView:               true
                opacity:                _editingLayer != _layerRallyPoints ? editorMap._nonInteractiveOpacity : 1
            }

            // 云平台地图元素：非云元素图层时压暗，和围栏/备降点一个观感
            CloudElementMapLayer {
                map:                    editorMap
                opacity:                _editingLayer != _layerCloudElements ? editorMap._nonInteractiveOpacity : 1
            }

            // UTMSPMapVisuals {
            //     id: utmspvisual
            //     enabled:                _utmspEnabled
            //     map:                    editorMap
            //     currentMissionItems:    _visualItems
            //     myGeoFenceController:   _geoFenceController
            //     interactive:            _editingLayer == _layerUTMSP
            //     homePosition:           _missionController.plannedHomePosition
            //     planView:               true
            //     opacity:                _editingLayer != _layerUTMSP ? editorMap._nonInteractiveOpacity : 1
            //     resetCheck:             _resetGeofencePolygon
            // }

            // Connections {
            //     target: utmspEditor
            //     function onResetGeofencePolygonTriggered() {
            //         resetTimer.start()
            //     }
            // }
            // Timer {
            //     id: resetTimer
            //     interval: 2500
            //     running: false
            //     repeat: false
            //     onTriggered: {
            //         _resetGeofencePolygon = true
            //     }
            // }
        }

        /*PlanViewToolBar {
            id:                     planToolBar
            anchors.left:       parent.left
            anchors.right:      parent.right
            anchors.top:        parent.top
            planMasterController:   _planMasterController
        }*/

        //左侧任务面板
        Rectangle {
            id: leftPanel
            anchors.left:       parent.left
            anchors.top:        parent.top
            anchors.bottom:     parent.bottom
            color:              qgcPal.windowShade
            width:              _rightPanelWidth
            DeadMouseArea {
                anchors.fill: parent
            }
            ColumnLayout{
                id:                   columnLeftPanle
                anchors.fill:         parent
                anchors.margins:      2
                spacing:              _margin
                QGCTabBar {
                    id:         planTabBar
                    // width:      parent.width
                    Layout.fillWidth:  true
                    Layout.fillHeight: false
                    Layout.alignment: Qt.AlignHCenter
                    visible:    QGroundControl.corePlugin.options.enablePlanViewSelector/*  && !_utmspEnabled*/
                    Component.onCompleted: currentIndex = 0
                    // 切到云端航线库那一页才拉列表。见 WaylineLibraryView 末尾的注释：
                    // 启动时自动拉会在云端没连的情况下显示「获取航线列表失败」。
                    onCurrentIndexChanged: {
                        if (currentIndex === 1) {
                            waylineLibrary.refresh(1)
                        }
                    }
                    QGCTabButton {
                        text:       qsTr("Local Mission")
                    }
                    QGCTabButton {
                        text:       qsTr("Cloud Mission")
                    }
                }


                StackLayout{
                    currentIndex: planTabBar.currentIndex
                    width:        parent.width
                    // height:       leftPanel.height - planTabBar.y
                    Layout.fillHeight: true
                    /******************** 本地任务 ********************/
                    ColumnLayout {
                        Layout.fillHeight: true
                        Layout.fillWidth:  true
                        spacing: 5
                        // RowLayout {
                        //     QGCIconButton {
                        //         iconSource:
                        //     }
                        // }

                        QGCTextField {
                            Layout.fillWidth:      true
                            Layout.leftMargin:     _margin
                            Layout.rightMargin:    _margin
                            Layout.alignment:      Qt.AlignHCenter

                            placeholderText: qsTr("Search plans...")
                            onTextChanged:   newPlanList.model.nameFilters = ["*" + text + "*.plan"]
                        }

                        // 多选条。勾了才能一次传多条到云端（云端没有批量接口，
                        // 是客户端排队一条条传，见 WaylineUploadDialog）
                        RowLayout {
                            Layout.fillWidth:   true
                            Layout.leftMargin:  _margin * 2
                            Layout.rightMargin: _margin * 2
                            spacing:            _margin
                            visible:            newPlanList.count > 0

                            QGCCheckBox {
                                text:     qsTr("全选")
                                checked:  newPlanList.count > 0
                                          && newPlanList.checkedMissions().length === newPlanList.count
                                onClicked: {
                                    newPlanList.setAllChecked(!(newPlanList.checkedMissions().length === newPlanList.count
                                                                && newPlanList.count > 0))
                                    // 点击会命令式写 checked，把上面的绑定顶掉，这里装回去
                                    checked = Qt.binding(function () {
                                        return newPlanList.count > 0
                                               && newPlanList.checkedMissions().length === newPlanList.count
                                    })
                                }
                            }
                            Item { Layout.fillWidth: true }
                            QGCLabel {
                                text:           newPlanList.checkedMissions().length > 0
                                                ? qsTr("已选 ") + newPlanList.checkedMissions().length + qsTr(" 条")
                                                : ""
                                font.pointSize: ScreenTools.smallFontPointSize
                                color:          qgcPal.text
                                opacity:        0.7
                            }
                        }

                        PlanListView {
                            id:               newPlanList
                            directory: "NewMission"
                            Layout.fillWidth:  true
                            Layout.fillHeight: true
                            // height: Math.min(contentHeight, columnLeftPanel.height - fileToolBar.y + columnLeftPanel.spacing)
                        }
                    }

                    /******************** 云端航线库 ********************/
                    // 跟「本地任务」同宽同位置，就在这一格里画 —— 云端的条目
                    // 和本地任务是一一对应的东西，分两种版式反而难找。
                    WaylineLibraryView {
                        id:                     waylineLibrary
                        Layout.fillHeight:      true
                        Layout.fillWidth:       true

                        onWaylineDownloaded: (kmzPath, name, autoLoad) => {
                            if (!autoLoad) {
                                mainWindow.showMessageDialog(qsTr("航线库"),
                                    qsTr("「") + name + qsTr("」已下载到：") + kmzPath)
                                return
                            }
                            // 下载下来的 kmz 转成 .plan 再载入，否则用户在 QGC 里打不开这个文件
                            const planPath = _appSettings.missionSavePath + "/NewMission/" + name + ".plan"
                            if (!waylineLibrary.wayline.convertKmzToPlan(kmzPath, planPath)) {
                                mainWindow.showMessageDialog(qsTr("航线库"),
                                    qsTr("打开下载的航线失败：") + waylineLibrary.wayline.lastConvertError())
                                return
                            }
                            _currentPlanFileName = name
                            _planMasterController.loadFromFile(planPath)
                            _planMasterController.fitViewportToItems()
                            // 先切回本地任务页再进编辑模式：地图和左侧工具条都在那一页，
                            // 留在航线库这页会看不到刚载进来的航线
                            planTabBar.currentIndex = 0
                            enterPlanEditMode(true)
                        }
                    }
                }

                // 动作栏。图标按钮横排一条，两个页签共用这一套 ——
                // 只有一端能用的动作（清除 / 上传任务）在云端那页是置灰的，
                // 而不是藏起来：按钮位置固定，用户才不会每次都重新找
                RowLayout {
                    id:                planActionBar
                    Layout.fillWidth:  true
                    Layout.fillHeight: false
                    Layout.margins:    _margin * 2
                    Layout.alignment:  Qt.AlignHCenter | Qt.AlignBottom
                    spacing:           _margin / 2

                    /// 当前是不是「本地任务」那一页（0=本地，1=云端航线库）
                    readonly property bool _localTab: planTabBar.currentIndex === 0
                    /// 「上传到云端」传的始终是**本地**勾选的任务，两个页签同一条件
                    readonly property bool _hasLocalSelection: newPlanList.selectedMissions().length > 0
                    readonly property bool _cloudBusy: !!(djiBridgeServer && djiBridgeServer.djiWayline
                                                          && djiBridgeServer.djiWayline.busy)

                    QGCButton {
                        Layout.fillWidth: true
                        iconSource:       "/qmlimages/Plus.svg"
                        enabled:          planActionBar._localTab
                        opacity:          enabled ? 1 : 0.45
                        ToolTip.text:     qsTr("新建任务")
                        ToolTip.visible:  hovered && ToolTip.text !== ""
                        ToolTip.delay:    600
                        onClicked: {
                            if (_planMasterController.containsItems) {
                                createPlanRemoveAllPromptDialog.createObject(mainWindow, { mapCenter: _mapCenter(),/* planCreator: object,*/ enableInput: true }).open()
                            }
                        }
                    }
                    QGCButton {
                        Layout.fillWidth: true
                        iconSource:       "/res/TrashDelete.svg"
                        enabled:          planActionBar._localTab
                                          && !_planMasterController.offline
                                          && !_planMasterController.syncInProgress
                                          && _planMasterController.containsItems
                        opacity:          enabled ? 1 : 0.45
                        ToolTip.text:     qsTr("清除任务")
                        ToolTip.visible:  hovered && ToolTip.text !== ""
                        ToolTip.delay:    600
                        onClicked:        clearButtonClicked()
                    }
                    QGCButton {
                        Layout.fillWidth: true
                        iconSource:       "/res/ArrowUpload.svg"
                        visible:          !QGroundControl.corePlugin.options.disableVehicleConnection
                        enabled:          planActionBar._localTab
                                          && !_planMasterController.offline
                                          && !_planMasterController.syncInProgress
                                          && _planMasterController.containsItems
                        opacity:          enabled ? 1 : 0.45
                        ToolTip.text:     qsTr("上传任务到飞控")
                        ToolTip.visible:  hovered && ToolTip.text !== ""
                        ToolTip.delay:    600
                        onClicked:        _planMasterController.upload()
                    }
                    QGCButton {
                        Layout.fillWidth: true
                        iconSource:       "/res/ArrowDownload.svg"
                        // 云端那页的「下载」是下勾选的云端航线，跟飞控无关，
                        // 所以不能被 disableVehicleConnection 一起藏掉
                        visible:          planActionBar._localTab
                                          ? !QGroundControl.corePlugin.options.disableVehicleConnection
                                          : true
                        enabled:          planActionBar._localTab
                                          ? (!_planMasterController.offline
                                             && !_planMasterController.syncInProgress
                                             && _planMasterController.containsItems)
                                          : (waylineLibrary.selectedRows.length > 0 && !planActionBar._cloudBusy)
                        opacity:          enabled ? 1 : 0.45
                        ToolTip.text:     planActionBar._localTab
                                          ? qsTr("从飞控下载任务（会覆盖当前方案）")
                                          : (qsTr("下载已勾选的 ") + waylineLibrary.selectedRows.length + qsTr(" 条云端航线"))
                        ToolTip.visible:  hovered && ToolTip.text !== ""
                        ToolTip.delay:    600
                        onClicked: {
                            if (planActionBar._localTab) {
                                downloadClicked(qsTr("Plan overwrite"))
                            } else {
                                waylineLibrary.downloadSelected()
                            }
                        }
                    }
                    QGCButton {
                        Layout.fillWidth: true
                        iconSource:       "/InstrumentValueIcons/cloud.svg"
                        visible:          !QGroundControl.corePlugin.options.disableVehicleConnection || !planActionBar._localTab
                        // 传的就是本地列表里勾选的那几条（没勾选就是当前选中的那一条）。
                        // 云端那页也看同一个条件 —— 弹窗里列的就是那几条本地任务，
                        // 一条都没勾时打开它只会得到一个传不了任何东西的空窗
                        enabled:          planActionBar._hasLocalSelection && !planActionBar._cloudBusy
                        opacity:          enabled ? 1 : 0.45
                        ToolTip.text:     {
                            if (!planActionBar._hasLocalSelection) {
                                return qsTr("上传到云端（先在「本地任务」页勾选要上传的任务）")
                            }
                            return newPlanList.checkedMissions().length > 0
                                    ? qsTr("上传到云端（已勾选 ") + newPlanList.checkedMissions().length + qsTr(" 条）")
                                    : qsTr("上传到云端")
                        }
                        ToolTip.visible:  hovered && ToolTip.text !== ""
                        ToolTip.delay:    600
                        onClicked:        uploadSelectedToCloud()
                    }
                }
            }

            Behavior on anchors.leftMargin {
                NumberAnimation { duration: 300 }
            }

            function showWidget(slipIn) {
                if (slipIn)
                    anchors.leftMargin = 0
                else
                    anchors.leftMargin = -width
            }
        }

        //-----------------------------------------------------------
        // Left tool strip
        ToolStrip {
            id:                 toolStrip
            anchors.margins:    _toolsMargin
            anchors.left:       parent.left
            anchors.leftMargin: -width
            // anchors.right:      leftPanel.left
            anchors.top:        parent.top
            z:                  QGroundControl.zOrderWidgets
            maxHeight:          parent.height - toolStrip.y

            readonly property int flyButtonIndex:       0
            readonly property int fileButtonIndex:      1
            readonly property int takeoffButtonIndex:   2
            readonly property int waypointButtonIndex:  3
            readonly property int roiButtonIndex:       4
            readonly property int patternButtonIndex:   5
            readonly property int landButtonIndex:      6
            readonly property int centerButtonIndex:    7

            property bool _isRallyLayer:    _editingLayer == _layerRallyPoints
            property bool _isMissionLayer:  _editingLayer == _layerMission
            // property bool _isUtmspLayer:     _editingLayer == _layerUTMSP

            ToolStripActionList {
                id: toolStripActionList
                model: [
                    ToolStripAction {
                        text:                   qsTr("File")
                        enabled:                !_planMasterController.syncInProgress
                        visible:                true
                        showAlternateIcon:      _planMasterController.dirty
                        iconSource:             "/qmlimages/MapSync.svg"
                        alternateIconSource:    "/qmlimages/MapSyncChanged.svg"
                        dropPanelComponent:     syncDropPanel
                    },
                    ToolStripAction {
                        text:       qsTr("Takeoff")
                        iconSource: "/res/takeoff.svg"
                        enabled:    _missionController.isInsertTakeoffValid
                        visible:    true//(toolStrip._isMissionLayer || toolStrip._isUtmspLayer) && !_planMasterController.controllerVehicle.rover
                        onTriggered: {
                            toolStrip.allAddClickBoolsOff()
                            insertTakeItemAfterCurrent()
                            _triggerSubmit = true
                        }
                    },
                    ToolStripAction {
                        id:                 addWaypointRallyPointAction
                        text:               _editingLayer == _layerRallyPoints ? qsTr("Rally Point") : qsTr("Waypoint")
                        iconSource:         "/qmlimages/MapAddMission.svg"
                        enabled:            toolStrip._isRallyLayer ? true : _missionController.flyThroughCommandsAllowed
                        visible:            true//toolStrip._isRallyLayer || toolStrip._isMissionLayer || toolStrip._isUtmspLayer
                        checkable:          true
                    },
                    ToolStripAction {
                        text:               _missionController.isROIActive ? qsTr("Cancel ROI") : qsTr("ROI")
                        iconSource:         "/qmlimages/MapAddMission.svg"
                        enabled:            !_missionController.onlyInsertTakeoffValid
                        visible:            toolStrip._isMissionLayer && _planMasterController.controllerVehicle.roiModeSupported
                        checkable:          !_missionController.isROIActive
                        onCheckedChanged:   _addROIOnClick = checked
                        onTriggered: {
                            if (_missionController.isROIActive) {
                                toolStrip.allAddClickBoolsOff()
                                insertCancelROIAfterCurrent()
                            }
                        }
                        property bool myAddROIOnClick: _addROIOnClick
                        onMyAddROIOnClickChanged: checked = _addROIOnClick
                    },
                    ToolStripAction {
                        text:               _singleComplexItem ? _missionController.complexMissionItemNames[0] : qsTr("Pattern")
                        iconSource:         "/qmlimages/MapDrawShape.svg"
                        enabled:            true//_missionController.flyThroughCommandsAllowed
                        visible:            toolStrip._isMissionLayer
                        dropPanelComponent: _singleComplexItem ? undefined : patternDropPanel
                        onTriggered: {
                            toolStrip.allAddClickBoolsOff()
                            if (_singleComplexItem) {
                                insertComplexItemAfterCurrent(_missionController.complexMissionItemNames[0])
                            }
                        }
                    },
                    ToolStripAction {
                        text:       _planMasterController.controllerVehicle.multiRotor
                                    ? qsTr("Return")
                                    : _missionController.isInsertLandValid && _missionController.hasLandItem
                                      ? qsTr("Alt Land")
                                      : qsTr("Land")
                        iconSource: "/res/rtl.svg"
                        enabled:    _missionController.isInsertLandValid
                        visible:    toolStrip._isMissionLayer || toolStrip._isUtmspLayer
                        onTriggered: {
                            toolStrip.allAddClickBoolsOff()
                            insertLandItemAfterCurrent()
                        }
                    },
                    ToolStripAction {
                        text:               qsTr("Center")
                        iconSource:         "/qmlimages/MapCenter.svg"
                        enabled:            true
                        visible:            true
                        dropPanelComponent: centerMapDropPanel
                    },
                    ToolStripAction {
                        text:               qsTr("Done")
                        iconSource:         "/qmlimages/Done.svg"
                        enabled:            true
                        visible:            true
                        checkable:          false
                        dropPanelComponent: null
                        onTriggered: {
                            if (_planMasterController.dirty) {
                                mainWindow.showMessageDialog(qsTr("Quit Plan Edit"),
                                                             qsTr("Save Plan"),
                                                             Dialog.Yes | Dialog.Cancel,
                                                             function() {
                                                                 console.log("accept:" + _currentPlanFileDir + "/" + _currentPlanFileName)
                                                                 _planMasterController.saveToCurrent()
                                                                 // _planMasterController.saveToFile(_currentPlanFileDir + "/" + _currentPlanFileName)
                                                                 // _planMasterController.saveToFile(_currentPlanFileDir + "/" + _currentPlanFileName)
                                                                 enterPlanEditMode(false)
                                                             })
                            } else {
                                // if (_itemCurrentWaypoint) {
                                //     _itemCurrentWaypoint.isCurrentWaypoint = false
                                // }
                                enterPlanEditMode(false)
                            }
                        }
                    }
                ]
            }

            model: toolStripActionList.model

            function allAddClickBoolsOff() {
                _addROIOnClick =        false
                addWaypointRallyPointAction.checked = false
            }

            onDropped: allAddClickBoolsOff()
        }

        //-----------------------------------------------------------
        // Right pane for mission editing controls
        Rectangle {
            id:                 rightPanel
            height:             parent.height
            width:              _rightPanelWidth
            color:              qgcPal.window
            // opacity:            0.2//layerTabBar.visible ? 0.2 : 0
            anchors.bottom:     parent.bottom
            anchors.right:      parent.right
            anchors.rightMargin: -width
            Behavior on anchors.rightMargin {
                NumberAnimation { duration: 300 }
            }

            function showWidget(slipIn) {
                if (slipIn) {
                    anchors.rightMargin = 0//_toolsMargin
                }
                else {
                    anchors.rightMargin = -width
                }
            }
        }

        // 云元素图标列：贴着右侧面板左缘，面板滑出时跟着一起走
        // showElementPanel 为 false —— 这边右侧面板本身就是元素编辑器（第 4 个图层 tab），
        // 再弹一个列表就是两份了
        CloudElementToolBar {
            id:                  cloudElementToolBar
            anchors.right:       rightPanel.left
            anchors.rightMargin: ScreenTools.defaultFontPixelWidth
            anchors.top:         editorMap.top
            // 让开右边的图层 tab 栏
            anchors.topMargin:   ScreenTools.defaultFontPixelHeight * 4
            map:                 editorMap
            showElementPanel:    false
            z:                   QGroundControl.zOrderWidgets
        }

        // 云端航线详情面板。选中航线库里的某条航线后贴在地图右侧 ——
        // 列表接口只给得出名字/机型/负载/模板/上传者，航点数、起点、规划长度都得
        // 解析那条航线的 kmz 才有，所以这块面板分两段：上段立刻能显示，下段等解析回来。
        //
        // 位置贴在 CloudElementToolBar 的**左边**，不能直接贴 parent.right：那个图标列
        // 在云端页正好停在窗口最右缘（右侧面板是滑走而非隐藏），贴 parent.right 就是把它盖住。
        Rectangle {
            id:                  cloudDetailPanel

            /// 比右侧编辑面板窄一截：这块面板只是「看」的，里面的键值对一行放不下
            /// 就省略号截断，没必要占满 300px 宽
            readonly property real _panelWidth: _rightPanelWidth * 0.8
            /// 面板最多能长到哪。底边原来是锚在 terrainStatus.top 上的，面板就一路
            /// 抻满整列，六行内容下面空着一大块，看着像盖在地图上一块黑板 ——
            /// 改成「内容多高就多高」，这里算的是别越过地形状态条的底线
            readonly property real _maxHeight: (terrainStatus.visible ? terrainStatus.y : parent.height)
                                               - editorMap.y
            /// 行距。键值行之间空一点点就够，用 _margin（9px）太空
            readonly property real _rowGap: ScreenTools.defaultFontPixelHeight / 3

            width:               cloudDetailPanel._panelWidth
            height:              Math.min(contentColumn.implicitHeight + _margin * 2, _maxHeight)
            color:               qgcPal.window
            anchors.top:         editorMap.top
            anchors.right:       cloudElementToolBar.left
            anchors.rightMargin: _margin
            // 左面板是「滑走」不是「隐藏」，不能靠滑动判断当前在哪页；
            // 而站在云端页时编辑模式必然是关的，所以这块面板不会跟编辑器右侧面板打架
            visible:             planTabBar.currentIndex === 1 && waylineLibrary.hasWaylineInfo
            z:                   QGroundControl.zOrderWidgets + 1

            /// 当前选中的那条航线（列表接口给的字段）
            readonly property var _info:    waylineLibrary.currentWayline
            /// 解析结果（还没回来或失败时是空对象）
            readonly property var _preview: waylineLibrary.preview

            /// 机型/负载/模板复用列表页那三张枚举表，不在这里抄一份 ——
            /// 抄了就会有一天跟列表页对不上
            readonly property var _metaRows: waylineLibrary.hasWaylineInfo ? [
                { "label": qsTr("机型"),   "value": waylineLibrary._modelText(_info.drone) },
                { "label": qsTr("负载"),   "value": waylineLibrary._payloadText(_info.payload) },
                { "label": qsTr("模板"),   "value": waylineLibrary._templateText(_info.template) },
                { "label": qsTr("上传者"), "value": _info.user ? _info.user : "--" },
                { "label": qsTr("更新"),   "value": _info.time ? _info.time : "--" }
            ] : []

            /// 解析出来才有的三项。Number() 包一层：QVariantMap 里的数到 QML
            /// 不一定就是 JS number，直接 .toFixed 会炸
            readonly property var _parsedRows: (_preview && _preview.ok) ? [
                { "label": qsTr("航点数"), "value": String(_preview.waypointCount) },
                { "label": qsTr("起点"),   "value": (Number(_preview.startLatitude)).toFixed(6)
                                                    + ", " + (Number(_preview.startLongitude)).toFixed(6) },
                { "label": qsTr("规划长度"), "value": QGroundControl.unitsConversion
                                                        .metersToAppSettingsHorizontalDistanceUnits(Number(_preview.lengthMeters)).toFixed(1)
                                                        + " " + QGroundControl.unitsConversion.appSettingsHorizontalDistanceUnitsString }
            ] : []

            // 面板浮在地图上，不挡一下鼠标会漏到底下的地图 —— 那一点就落一个航点
            DeadMouseArea { anchors.fill: parent }

            ColumnLayout {
                id:              contentColumn
                anchors.fill:    parent
                anchors.margins: _margin
                // 面板高度就是这一列算出来的（见 _maxHeight），所以这一列不能再有
                // 撑满剩余空间的子项，也不能用 anchors.bottom 去抻它
                spacing:         cloudDetailPanel._rowGap

                QGCLabel {
                    Layout.fillWidth: true
                    text:             cloudDetailPanel._info.name ? cloudDetailPanel._info.name : ""
                    font.bold:        true
                    elide:            Text.ElideRight
                }

                // 键值两列。两段内容（列表接口给的字段 / 解析出来的字段）共用这一个样式，
                // 只是喂进去的数组不同。
                //
                // 字号用 defaultFontPointSize：这一版之前全是 smallFontPointSize，
                // 1080p/100% 下只有 9pt，这是面板上唯一的实际内容，看不清等于没有
                Repeater {
                    model: cloudDetailPanel._metaRows
                    delegate: RowLayout {
                        Layout.fillWidth: true
                        spacing:          4
                        QGCLabel {
                            text:           modelData.label + qsTr("：")
                            opacity:        0.7
                            font.pointSize: ScreenTools.defaultFontPointSize
                        }
                        QGCLabel {
                            Layout.fillWidth: true
                            text:             modelData.value
                            elide:            Text.ElideRight
                            font.pointSize:   ScreenTools.defaultFontPointSize
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth:       true
                    Layout.preferredHeight: 1
                    color:                  qgcPal.groupBorder
                }

                // ---- 下段：三种状态互斥，一次只显示一种 ----
                QGCLabel {
                    Layout.fillWidth: true
                    visible:          waylineLibrary.previewBusy
                    text:             qsTr("正在取航线文件…")
                    opacity:          0.7
                    font.pointSize:   ScreenTools.defaultFontPointSize
                }

                QGCLabel {
                    Layout.fillWidth: true
                    visible:          !waylineLibrary.previewBusy && waylineLibrary.previewFailed
                    text:             (cloudDetailPanel._preview && cloudDetailPanel._preview.error)
                                      ? cloudDetailPanel._preview.error
                                      : qsTr("这条航线的航线文件取不到")
                    color:            qgcPal.warningText
                    wrapMode:         Text.WordWrap
                    font.pointSize:   ScreenTools.defaultFontPointSize
                }

                Repeater {
                    model: cloudDetailPanel._parsedRows
                    delegate: RowLayout {
                        Layout.fillWidth: true
                        spacing:          4
                        QGCLabel {
                            text:           modelData.label + qsTr("：")
                            opacity:        0.7
                            font.pointSize: ScreenTools.defaultFontPointSize
                        }
                        QGCLabel {
                            Layout.fillWidth: true
                            text:             modelData.value
                            elide:            Text.ElideRight
                            font.pointSize:   ScreenTools.defaultFontPointSize
                        }
                    }
                }
            }
        }
        //-------------------------------------------------------
        // Right Panel Controls
        Item {
            anchors.fill:           rightPanel
            DeadMouseArea {
                anchors.fill:   parent
            }

            ColumnLayout {
                id:                 rightControls
                spacing:            ScreenTools.defaultFontPixelHeight * 0.5
                // anchors.fill:       parent
                anchors.top:          parent.top
                anchors.left:         parent.left
                anchors.right:        parent.right
                anchors.topMargin:    _margin
                anchors.bottomMargin: _margin

                // anchors.left:       parent.left
                // anchors.right:      parent.right
                // anchors.top:        parent.top
                //-------------------------------------------------------

                RowLayout {
                    id: rowFileName
                    Layout.fillWidth:     true
                    Layout.leftMargin:    _margin
                    Layout.rightMargin:   _margin
                    spacing:              _margin
                    // implicitHeight: 40

                    TextField {
                        id:               textFieldFileName
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter
                        implicitHeight:   28
                        implicitWidth:    240
                        font.pointSize:   11
                        text:             _currentPlanFileName
                        color:            qgcPal.text
                        hoverEnabled:     true
                        enabled:          true
                        background: Rectangle {
                            anchors.fill: parent
                            color: "transparent"
                            border.color: (parent.hovered || parent.focus) ? qgcPal.colorGrey : "transparent"

                        }
                        leftPadding: focus ? 5 : 0
                        onEditingFinished: {
                            focus = false
                            var previousPlanFileName = _currentPlanFileName
                            if (_currentPlanFileName !== text && text !== "") {
                                _currentPlanFileName = text
                                if (!_planMasterController.renameCurrentFile(text)) {
                                    _currentPlanFileName = previousPlanFileName
                                    text = previousPlanFileName
                                }
                            } else if (text === "") {
                                text = previousPlanFileName
                            }
                        }
                    }

                    QGCIconButton {
                        Layout.fillHeight: true
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                        iconSource: "/InstrumentValueIcons/edit-pencil.svg"
                        highlighted: hovered
                        onClicked: {
                            textFieldFileName.focus = true
                        }
                    }
                    QGCIconButton {
                        Layout.fillHeight: true
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                        iconSource: "/InstrumentValueIcons/save-disk.svg"
                        highlighted: hovered
                        onClicked: {
                            if(_planMasterController.currentPlanFile !== "") {
                                _planMasterController.saveToCurrent()
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth:     true
                    Layout.fillHeight:    false
                    height: 1
                    // anchors.left:   parent.left
                    // anchors.right:  parent.right
                    color:          qgcPal.groupBorder
                }
                GridLayout {
                    columns:                5
                    rowSpacing:             2//_rowSpacing
                    columnSpacing:          _labelToValueSpacing
                    Layout.alignment:       Qt.AlignVCenter | Qt.AlignHCenter
                    Layout.fillWidth:       true
                    Layout.fillHeight:      false
                    Layout.minimumHeight:   40
                    // 第一行
                    QGCLabel { text: qsTr("规划长度:"); font.pointSize: _dataFontSize; }
                    QGCLabel {
                        text:                   _missionPlannedDistanceText
                        font.pointSize:         _dataFontSize + 2
                        font.bold:              true
                        Layout.minimumWidth:    _largeValueWidth
                    }

                    Item { width: 1; height: 1 }

                    QGCLabel { text: qsTr("航线长度:"); font.pointSize: _dataFontSize; }
                    QGCLabel {
                        text:                   _missionTotalDistanceText
                        font.pointSize:         _dataFontSize + 2
                        font.bold:              true
                        Layout.minimumWidth:    _largeValueWidth
                    }



                    // 第二行
                    QGCLabel {
                        text: qsTr("最远距离:");
                        font.pointSize:          _dataFontSize;
                        Layout.alignment:        Qt.AlignRight
                    }
                    QGCLabel {
                        text:                   _missionMaxTelemetryText
                        font.pointSize:         _dataFontSize + 2
                        font.bold:              true
                        Layout.minimumWidth:    _largeValueWidth
                        Layout.alignment:        Qt.AlignRight
                    }

                    Item { width: 1; height: 1 }

                    QGCLabel {
                        text: qsTr("飞行时长:");
                        font.pointSize:          _dataFontSize;
                        Layout.alignment:        Qt.AlignRight
                    }
                    QGCLabel {
                        text:                   getMissionTime()

                        font.pointSize:         _dataFontSize + 2
                        font.bold:              true
                        Layout.minimumWidth:    _largeValueWidth
                        Layout.alignment:        Qt.AlignRight
                    }

                }

                // Mission Controls (Expanded)
                QGCTabBar {
                    id:         layerTabBar
                    Layout.fillWidth: true
                    Layout.topMargin: _margin
                    implicitHeight:    50
                    // width:      parent.width
                    visible:       true//QGroundControl.corePlugin.options.enablePlanViewSelector  && !_utmspEnabled
                    Component.onCompleted: currentIndex = 0
                    QGCTabButton {
                        text:       qsTr("Mission")
                        pointSize:      ScreenTools.mediumFontPointSize
                    }
                    QGCTabButton {
                        text:       qsTr("Fence")
                        pointSize:  ScreenTools.mediumFontPointSize
                        enabled:    _geoFenceController.supported
                    }
                    QGCTabButton {
                        text:       qsTr("Rally")
                        pointSize:  ScreenTools.mediumFontPointSize
                        enabled:    _rallyPointController.supported
                    }
                    QGCTabButton {
                        // 别写「云元素」：TabBar 把每个 tab 的宽度统一成最宽那个（89px），
                        // 4 个 tab 就是 356px，超过右面板的 300px，第 4 个会被窗口边缘裁掉。
                        // 两个字跟「任务/围栏/集结」一致，也跟云指示器抽屉里那个 tab 同名。
                        text:       qsTr("元素")
                        pointSize:  ScreenTools.mediumFontPointSize
                        // 云平台没连上时没有元素可编，切过去只是个空面板
                        enabled:    djiBridgeServer.cloudWsConnected
                    }
                }

                /*QGCTabBar {
                    id:         layerTabBarUTMSP
                    width:      parent.width
                    visible:    QGroundControl.corePlugin.options.enablePlanViewSelector && _utmspEnabled
                    QGCTabButton {
                        text:       qsTr("Mission")
                    }
                    TabButton {
                        text:       qsTr("Rally")
                        enabled:    _rallyPointController.supported
                    }
                    QGCTabButton {
                        id: utmspbutton
                        text:       qsTr("UTM-Adapter")
                        visible: _utmspEnabled
                    }
                }
                */
            }

            //-------------------------------------------------------
            // Mission Item Editor
            Item {
                id:                     missionItemEditor
                // Layout.fillWidth:       true
                // Layout.fillHeight:      true
                anchors.left:           parent.left
                anchors.leftMargin:      ScreenTools.defaultFontPixelHeight * 0.25
                anchors.right:          parent.right
                anchors.rightMargin:      ScreenTools.defaultFontPixelHeight * 0.25
                anchors.top:            rightControls.bottom
                anchors.topMargin:      ScreenTools.defaultFontPixelHeight * 0.25
                anchors.bottom:         parent.bottom
                anchors.bottomMargin:   ScreenTools.defaultFontPixelHeight * 0.25
                visible:                _editingLayer == _layerMission && !planControlColapsed
                QGCListView {
                    id:                 missionItemEditorListView
                    anchors.fill:       parent
                    spacing:            ScreenTools.defaultFontPixelHeight / 4
                    orientation:        ListView.Vertical
                    model:              _missionController.visualItems
                    cacheBuffer:        Math.max(height * 2, 0)
                    clip:               true
                    currentIndex:       _missionController.currentPlanViewSeqNum
                    highlightMoveDuration: 250
                    visible:            true//_editingLayer == _layerMission && !planControlColapsed
                    //-- List Elements
                    delegate: MissionItemEditor {
                        map:            editorMap
                        masterController:  _planMasterController
                        missionItem:    object
                        width:          missionItemEditorListView.width
                        readOnly:       false
                        onClicked: (sequenceNumber) => { _missionController.setCurrentPlanViewSeqNum(object.sequenceNumber, false) }
                        onRemove: {
                            var removeVIIndex = index
                            _missionController.removeVisualItem(removeVIIndex)
                            if (removeVIIndex >= _missionController.visualItems.count) {
                                removeVIIndex--
                            }
                        }
                        onSelectNextNotReadyItem:   selectNextNotReady()
                    }
                }
            }

            // GeoFence Editor
            GeoFenceEditor {
                anchors.top:            rightControls.bottom
                anchors.topMargin:      ScreenTools.defaultFontPixelHeight * 0.25
                anchors.bottom:         parent.bottom
                anchors.left:           parent.left
                anchors.leftMargin:      ScreenTools.defaultFontPixelHeight * 0.25
                anchors.right:          parent.right
                anchors.rightMargin:      ScreenTools.defaultFontPixelHeight * 0.25
                myGeoFenceController:   _geoFenceController
                flightMap:              editorMap
                visible:                _editingLayer == _layerGeoFence
            }

            // Cloud Element Editor（云平台地图元素）
            CloudElementEditor {
                anchors.top:            rightControls.bottom
                anchors.topMargin:      ScreenTools.defaultFontPixelHeight * 0.25
                anchors.bottom:         parent.bottom
                anchors.left:           parent.left
                anchors.leftMargin:      ScreenTools.defaultFontPixelHeight * 0.25
                anchors.right:          parent.right
                anchors.rightMargin:      ScreenTools.defaultFontPixelHeight * 0.25
                flightMap:              editorMap
                visible:                _editingLayer == _layerCloudElements
            }

            // Rally Point Editor
            RallyPointEditorHeader {
                id:                     rallyPointHeader
                anchors.top:            rightControls.bottom
                anchors.topMargin:      ScreenTools.defaultFontPixelHeight * 0.25
                anchors.left:           parent.left
                anchors.leftMargin:    ScreenTools.defaultFontPixelHeight * 0.25
                anchors.right:          parent.right
                anchors.rightMargin:    ScreenTools.defaultFontPixelHeight * 0.25
                visible:                _editingLayer == _layerRallyPoints
                controller:             _rallyPointController
            }
            RallyPointItemEditor {
                id:                     rallyPointEditor
                anchors.top:            rallyPointHeader.bottom
                anchors.topMargin:      ScreenTools.defaultFontPixelHeight * 0.25
                anchors.left:           parent.left
                anchors.leftMargin:     ScreenTools.defaultFontPixelHeight * 0.25
                anchors.right:          parent.right
                anchors.rightMargin:    ScreenTools.defaultFontPixelHeight * 0.25
                visible:                _editingLayer == _layerRallyPoints && _rallyPointController.points.count
                rallyPoint:             _rallyPointController.currentRallyPoint
                controller:             _rallyPointController
            }

            /* UTMSPAdapterEditor{
                id: utmspEditor
                enabled:                 _utmspEnabled
                anchors.top:             rightControls.bottom
                anchors.topMargin:       ScreenTools.defaultFontPixelHeight * 0.25
                anchors.bottom:          parent.bottom
                anchors.left:            parent.left
                anchors.right:           parent.right
                currentMissionItems:     _visualItems
                myGeoFenceController:    _geoFenceController
                flightMap:               editorMap
                visible:                 _editingLayer == _layerUTMSP
                triggerSubmitButton:     _triggerSubmit
                resetRegisterFlightPlan: _resetRegisterFlightPlan
            }
            */
        }

        QGCLabel {
            // Elevation provider notice on top of terrain plot
            readonly property string _licenseString: QGroundControl.elevationProviderNotice

            id:                         licenseLabel
            visible:                    terrainStatus.visible && _licenseString !== ""
            anchors.bottom:             terrainStatus.top
            anchors.horizontalCenter:   terrainStatus.horizontalCenter
            anchors.bottomMargin:       ScreenTools.defaultFontPixelWidth * 0.5
            font.pointSize:             ScreenTools.smallFontPointSize
            text:                       qsTr("Powered by %1").arg(_licenseString)
        }

        TerrainStatus {
            id:                 terrainStatus
            anchors.margins:    _toolsMargin
            // anchors.leftMargin: 0
            anchors.left:       leftPanel.right//mapScale.left
            anchors.right:      rightPanel.left
            anchors.bottom:     parent.bottom
            height:             ScreenTools.defaultFontPixelHeight * 7
            missionController:  _missionController
            visible:            _internalVisible && _editingLayer === _layerMission && QGroundControl.corePlugin.options.showMissionStatus

            onSetCurrentSeqNum: _missionController.setCurrentPlanViewSeqNum(seqNum, true)

            property bool _internalVisible: _planViewSettings.showMissionItemStatus.rawValue

            function toggleVisible() {
                _internalVisible = !_internalVisible
                _planViewSettings.showMissionItemStatus.rawValue = _internalVisible
            }
        }

        MapScale {
            id:                     mapScale
            anchors.margins:        _toolsMargin
            anchors.bottom:         terrainStatus.visible ? terrainStatus.top : parent.bottom
            anchors.left:           leftPanel.right//toolStrip.y + toolStrip.height + _toolsMargin > mapScale.y ? toolStrip.right: parent.left
            mapControl:             editorMap
            buttonsOnLeft:          true
            terrainButtonVisible:   _editingLayer === _layerMission
            terrainButtonChecked:   terrainStatus.visible
            onTerrainButtonClicked: terrainStatus.toggleVisible()
        }

    }

    function showLoadFromFileOverwritePrompt(title) {
        mainWindow.showMessageDialog(title,
                                     qsTr("You have unsaved/unsent changes. Loading from a file will lose these changes. Are you sure you want to load from a file?"),
                                     Dialog.Yes | Dialog.Cancel,
                                     function() { _planMasterController.loadFromSelectedFile() } )
    }

    /// 上传队列跑完之后收尾：刷新云端列表、切到云端那页、清掉本地勾选。
    /// 写成根对象上的函数而不是直接写在 Component 里 —— Component 里的箭头
    /// 函数碰 planTabBar / newPlanList 这些后声明的 id 容易踩作用域的坑
    function _afterCloudUpload(doneCount, failCount) {
        if (doneCount === 0) {
            return
        }
        // 传成功了就切到云端那页，让用户直接看到刚上去的航线
        waylineLibrary.refreshAfterUpload()
        if (planTabBar.currentIndex !== 1) {
            planTabBar.currentIndex = 1
        }
        newPlanList.clearChecked()
    }

    /// 「新建任务」落点要用地图**可视区**（扣掉左右面板）的中心。
    /// 与上面那个 mapCenter() 不是一回事：那个取的是 editorMap.center ——
    /// 整块地图的几何中心，把被面板盖住的那部分也算进去了。
    /// 原来这是写在按钮里的局部函数，按钮改成图标后参数要直接喂给弹窗，
    /// 放到根对象上更稳
    function _mapCenter() {
        var centerPoint = Qt.point(editorMap.centerViewport.left + (editorMap.centerViewport.width / 2),
                                   editorMap.centerViewport.top + (editorMap.centerViewport.height / 2))
        return editorMap.toCoordinate(centerPoint, false /* clipToViewPort */)
    }

    /// 选中那条云航线的航迹，转成 QtPositioning 坐标给地图上的 MapPolyline。
    /// 经纬度都是 0 的点要跳过：解析失败的航点就是 (0,0)，不跳过会把航线画到几内亚湾去
    /// （MapFitFunctions.qml 对任务点也是这么处理的）。
    ///
    /// pts 是**拍平**的 [lat0, lon0, lat1, lon1, ...]，所以下标要按 2 步走 ——
    /// 别写成 pts[i][0]。嵌套的 QVariantList 过 QML 边界时会被摊平，
    /// 按嵌套读会得到一堆 undefined（C++ 那边有同样的注释）
    ///
    /// 写成 property 而不是函数：MapPolyline 的 path 和 visible 都要用它，
    /// 每次读都重算一遍没必要
    readonly property var _cloudRouteCoords: {
        var preview = waylineLibrary.preview
        if (!preview || !preview.ok || !preview.points) {
            return []
        }
        var pts = preview.points
        var out = []
        for (var i = 0; i + 1 < pts.length; i += 2) {
            var lat = Number(pts[i])
            var lon = Number(pts[i + 1])
            if (isNaN(lat) || isNaN(lon) || (lat === 0 && lon === 0)) {
                continue
            }
            out.push(QtPositioning.coordinate(lat, lon))
        }
        return out
    }

    /// 把云端航迹取景到地图的**可视区**里。做法照 MapFitFunctions.fitMapViewportToAllCoordinates：
    /// 先算航迹的包围盒，再按「可视区外那圈像素折合多少度」把盒子撑大 ——
    /// 这样航线两端才不会被左右面板压住。
    ///
    /// 必须走 editorMap.setVisibleRegion：直接设 center + zoomLevel 是算不准的，
    /// 同一个 zoomLevel 在不同纬度覆盖的经度宽不一样。
    function _fitCloudRoute(points) {
        if (!points || points.length === 0) {
            return
        }

        var north = points[0].latitude
        var south = north
        var west  = points[0].longitude
        var east  = west
        for (var i = 1; i < points.length; ++i) {
            north = Math.max(north, points[i].latitude)
            south = Math.min(south, points[i].latitude)
            east  = Math.max(east,  points[i].longitude)
            west  = Math.min(west,  points[i].longitude)
        }

        // 单点，或者正南北/正东西的一条直线：矩形会退化成点或线，QGeoRectangle
        // 就不是有效区域了，setVisibleRegion 会把地图一口气推到最大级别。
        // 这种情况退回「中心 + 一个固定级别」
        var pad = 0.0005
        if (north - south < pad || east - west < pad) {
            editorMap.center    = QtPositioning.coordinate((north + south) / 2, (east + west) / 2)
            editorMap.zoomLevel = 17
            return
        }

        // 可视区四条边各被盖住多少像素。**不要用 editorMap.centerViewport**：
        // 那个 rect 是按「编辑模式」的叠加层算的 —— 左边算的是 55px 宽的工具条，
        // 右边算的是编辑器的 rightPanel；而云端页盖在地图上的是 300px 的任务左栏
        // 加右边这条「元素工具栏 + 详情面板」，两者完全对不上。
        // 用它量出来的航迹，西端会落到左栏后面 240 来个像素处 —— 看上去就是
        // 航线从面板边上凭空冒出来。
        //
        // 这些面板都是靠锚点滑动进出的（showWidget），滑出去时停到屏幕外，
        // 所以直接按它们真实的 x 算，不必去猜谁可见
        var leftInset  = Math.max(leftPanel.x + leftPanel.width, 0)
        var topInset   = 0
        var rightEdge  = Math.min(rightPanel.x,
                                  cloudDetailPanel.visible ? cloudDetailPanel.x : cloudElementToolBar.x)
        var rightInset = Math.max(editorMap.width - rightEdge, 0)
        var botInset   = terrainStatus.visible ? Math.max(editorMap.height - terrainStatus.y, 0) : 0

        var vpWidth  = Math.max(editorMap.width - leftInset - rightInset, 1)
        var vpHeight = Math.max(editorMap.height - topInset - botInset, 1)

        // 再留出这几像素的空。只按「面板盖住多少」撑的话，航迹的两个极值点正好落在
        // 面板边缘上（而且墨卡托与线性的纬度差还会再挤出一点），看上去像航线钻到面板底下。
        // 留一点空才是「没有被挡住」
        var clearance = 12

        // 注意是拿扣掉 clearance 后的尺寸去算每像素多少度：这样航迹占的正好是
        // 「可视区再往里缩 clearance」那圈，而不是被撑到可视区边上
        var latPerPixel = (north - south) / Math.max(vpHeight - 2 * clearance, 1)
        var lonPerPixel = (east  - west)  / Math.max(vpWidth  - 2 * clearance, 1)
        north = Math.min(north + ((topInset   + clearance) * latPerPixel),  90)
        south = Math.max(south - ((botInset   + clearance) * latPerPixel), -90)
        west  = Math.max(west  - ((leftInset  + clearance) * lonPerPixel), -180)
        east  = Math.min(east  + ((rightInset + clearance) * lonPerPixel),  180)

        // QtPositioning.rectangle 是先左上后右下：(北, 西) → (南, 东)
        editorMap.setVisibleRegion(QtPositioning.rectangle(QtPositioning.coordinate(north, west),
                                                          QtPositioning.coordinate(south, east)))
    }

    /// 云端航线的解析结果回来了就取景。挂在根上而不是列表页里 —— 地图是这边的
    Connections {
        target: waylineLibrary

        function onPreviewChanged() {
            if (waylineLibrary.preview && waylineLibrary.preview.ok) {
                _root._fitCloudRoute(_root._cloudRouteCoords)
            }
        }
    }

    /// 上传本地勾选的任务到云端。底部动作栏的「上传到云端」两个页签共用这一个入口。
    /// 刻意不再像原来那样先把页签切回本地任务页：弹窗里本来就列着要传的是哪几条，
    /// 而传完 _afterCloudUpload 会刷新列表并落到云端那页
    function uploadSelectedToCloud() {
        uploadWaylineDialog.createObject(mainWindow, {
            "wayline":    djiBridgeServer ? djiBridgeServer.djiWayline : null,
            "missions":   newPlanList.selectedMissions(),
            "cloudNames": waylineLibrary.loadedNames()
        }).open()
    }

    /// 上传本地任务到云端。可以一次多条 —— 后台没有批量登记接口，
    /// 客户端一条条排队传，每条结果单独记，见 WaylineUploadDialog
    Component {
        id: uploadWaylineDialog

        WaylineUploadDialog {
            onUploadCompleted: (doneCount, failCount) => _afterCloudUpload(doneCount, failCount)
        }
    }

    Component {
        id: createPlanRemoveAllPromptDialog

        QGCSimpleMessageDialog {
            title:      qsTr("Create Plan")
            text:       qsTr("Are you sure you want to remove current plan and create a new plan? ")
            buttons:    Dialog.Yes | Dialog.No

            property var mapCenter
            property var planCreator

            onAccepted: { //planCreator.createPlan(mapCenter)
                if (enableInput) {
                    _currentPlanFileName = inputText
                    _missionController.removeAllVisualItems()
                    _planMasterController.removeAllFromVehicle();
                    _planMasterController.saveToFile(_appSettings.missionSavePath + "/NewMission/" + inputText)
                }
            }
        }
    }

    function clearButtonClicked() {
        mainWindow.showMessageDialog(qsTr("Clear"),
                                     qsTr("Are you sure you want to remove all mission items and clear the mission from the vehicle?"),
                                     Dialog.Yes | Dialog.Cancel,
                                     function() { _planMasterController.removeAllFromVehicle();
                                                  _missionController.setCurrentPlanViewSeqNum(0, true);
                                                  if(_utmspEnabled)
                                                    {_resetRegisterFlightPlan = true;
                                                      QGroundControl.utmspManager.utmspVehicle.triggerActivationStatusBar(false);
                                                      UTMSPStateStorage.startTimeStamp = "";
                                                      UTMSPStateStorage.showActivationTab = false;
                                                      UTMSPStateStorage.flightID = "";
                                                      UTMSPStateStorage.enableMissionUploadButton = false;
                                                      UTMSPStateStorage.indicatorPendingStatus = true;
                                                      UTMSPStateStorage.indicatorApprovedStatus = false;
                                                      UTMSPStateStorage.indicatorActivatedStatus = false;
                                                      UTMSPStateStorage.currentStateIndex = 0}})
    }

    //- ToolStrip ToolStripDropPanel Components

    Component {
        id: centerMapDropPanel

        CenterMapDropPanel {
            map:            editorMap
            fitFunctions:   mapFitFunctions
        }
    }

    Component {
        id: patternDropPanel

        ColumnLayout {
            spacing:    ScreenTools.defaultFontPixelWidth * 0.5

            QGCLabel { text: qsTr("Create complex pattern:") }

            Repeater {
                model: _missionController.complexMissionItemNames

                QGCButton {
                    text:               modelData
                    Layout.fillWidth:   true

                    onClicked: {
                        insertComplexItemAfterCurrent(modelData)
                        dropPanel.hide()
                    }
                }
            }
        } // Column
    }

    function downloadClicked(title) {
        if (_planMasterController.dirty) {
            mainWindow.showMessageDialog(title,
                                         qsTr("You have unsaved/unsent changes. Loading from the Vehicle will lose these changes. Are you sure you want to load from the Vehicle?"),
                                         Dialog.Yes | Dialog.Cancel,
                                         function() { _planMasterController.loadFromVehicle() })
        } else {
            _planMasterController.loadFromVehicle()
        }
    }

    Component {
        id: syncDropPanel

        ColumnLayout {
            id:         columnHolder
            spacing:    _margin

            property string _overwriteText: qsTr("Plan overwrite")

            QGCLabel {
                id:                 unsavedChangedLabel
                Layout.fillWidth:   true
                wrapMode:           Text.WordWrap
                text:               globals.activeVehicle ?
                                        qsTr("You have unsaved changes. You should upload to your vehicle, or save to a file.") :
                                        qsTr("You have unsaved changes.")
                visible:            _planMasterController.dirty
            }

            /* 取消复杂编辑 */
            SectionHeader {
                id:                 createSection
                Layout.fillWidth:   true
                text:               qsTr("Create Plan")
                showSpacer:         false
            }

            GridLayout {
                columns:            2
                columnSpacing:      _margin
                rowSpacing:         _margin
                Layout.fillWidth:   true
                visible:            createSection.checked

                Repeater {
                    model: _planMasterController.planCreators

                    Rectangle {
                        id:     button
                        width:  ScreenTools.defaultFontPixelHeight * 7
                        height: planCreatorNameLabel.y + planCreatorNameLabel.height
                        color:  button.pressed || button.highlighted ? qgcPal.buttonHighlight : qgcPal.button

                        property bool highlighted: mouseArea.containsMouse
                        property bool pressed:     mouseArea.pressed

                        Image {
                            id:                 planCreatorImage
                            anchors.left:       parent.left
                            anchors.right:      parent.right
                            source:             object.imageResource
                            sourceSize.width:   width
                            fillMode:           Image.PreserveAspectFit
                            mipmap:             true
                        }

                        QGCLabel {
                            id:                     planCreatorNameLabel
                            anchors.top:            planCreatorImage.bottom
                            anchors.left:           parent.left
                            anchors.right:          parent.right
                            horizontalAlignment:    Text.AlignHCenter
                            text:                   object.name
                            color:                  button.pressed || button.highlighted ? qgcPal.buttonHighlightText : qgcPal.buttonText
                        }

                        QGCMouseArea {
                            id:                 mouseArea
                            anchors.fill:       parent
                            hoverEnabled:       true
                            preventStealing:    true
                            onClicked:          {
                                if (_planMasterController.containsItems) {
                                    createPlanRemoveAllPromptDialog.createObject(mainWindow, { mapCenter: _mapCenter(), planCreator: object }).open()
                                } else {
                                    object.createPlan(_mapCenter())
                                }
                                dropPanel.hide()
                            }

                            function _mapCenter() {
                                var centerPoint = Qt.point(editorMap.centerViewport.left + (editorMap.centerViewport.width / 2), editorMap.centerViewport.top + (editorMap.centerViewport.height / 2))
                                return editorMap.toCoordinate(centerPoint, false /* clipToViewPort */)
                            }
                        }
                    }
                }
            }

            SectionHeader {
                id:                 storageSection
                Layout.fillWidth:   true
                text:               qsTr("Storage")
            }


            GridLayout {
                columns:            3
                rowSpacing:         _margin
                columnSpacing:      ScreenTools.defaultFontPixelWidth
                visible:            storageSection.checked

                QGCButton {
                    text:               qsTr("Open...")
                    Layout.fillWidth:   true
                    enabled:            !_planMasterController.syncInProgress
                    onClicked: {
                        dropPanel.hide()
                        if (_planMasterController.dirty) {
                            showLoadFromFileOverwritePrompt(columnHolder._overwriteText)
                        } else {
                            _planMasterController.loadFromSelectedFile()
                        }
                    }
                }

                QGCButton {
                    text:               qsTr("Save")
                    Layout.fillWidth:   true
                    enabled:            !_planMasterController.syncInProgress && _planMasterController.currentPlanFile !== ""
                    onClicked: {
                        dropPanel.hide()
                        if(_planMasterController.currentPlanFile !== "") {
                            _planMasterController.saveToCurrent()
                        } else {
                            _planMasterController.saveToSelectedFile()
                        }
                    }
                }

                QGCButton {
                    text:               qsTr("Save As...")
                    Layout.fillWidth:   true
                    enabled:            !_planMasterController.syncInProgress && _planMasterController.containsItems
                    onClicked: {
                        dropPanel.hide()
                        _planMasterController.saveToSelectedFile()
                    }
                }

                QGCButton {
                    Layout.columnSpan:  3
                    Layout.fillWidth:   true
                    text:               qsTr("Save Mission Waypoints As KML...")
                    enabled:            !_planMasterController.syncInProgress && _visualItems.count > 1
                    onClicked: {
                        // First point does not count
                        if (_visualItems.count < 2) {
                            mainWindow.showMessageDialog(qsTr("KML"), qsTr("You need at least one item to create a KML."))
                            return
                        }
                        dropPanel.hide()
                        _planMasterController.saveKmlToSelectedFile()
                    }
                }
            }

            SectionHeader {
                id:                 vehicleSection
                Layout.fillWidth:   true
                text:               qsTr("Vehicle")
            }

            RowLayout {
                Layout.fillWidth:   true
                spacing:            _margin
                visible:            vehicleSection.checked

                QGCButton {
                    text:               qsTr("Upload")
                    Layout.fillWidth:   true
                    enabled:            !_planMasterController.offline && !_planMasterController.syncInProgress && _planMasterController.containsItems
                    visible:            !QGroundControl.corePlugin.options.disableVehicleConnection
                    onClicked: {
                        dropPanel.hide()
                        _planMasterController.upload()
                    }
                }

                QGCButton {
                    text:               qsTr("Download")
                    Layout.fillWidth:   true
                    enabled:            !_planMasterController.offline && !_planMasterController.syncInProgress
                    visible:            !QGroundControl.corePlugin.options.disableVehicleConnection

                    onClicked: {
                        dropPanel.hide()
                        downloadClicked(columnHolder._overwriteText)
                    }
                }

                QGCButton {
                    text:               qsTr("Clear")
                    Layout.fillWidth:   true
                    Layout.columnSpan:  2
                    enabled:            !_planMasterController.offline && !_planMasterController.syncInProgress
                    visible:            !QGroundControl.corePlugin.options.disableVehicleConnection
                    onClicked: {
                        dropPanel.hide()
                        clearButtonClicked()
                    }
                }
            }
        }
    }

    // Connections {
    //     target: utmspEditor
    //     function onVehicleIDSent(id) {
    //         _vehicleID = id
    //     }
    // }
    // Connections {
    //     target: utmspEditor
    //     function onRemoveFlightPlanTriggered() {
    //         _planMasterController.removeAllFromVehicle();
    //         _missionController.setCurrentPlanViewSeqNum(0, true);
    //         if(_utmspEnabled){_resetRegisterFlightPlan = true}
    //     }
    // }

}
