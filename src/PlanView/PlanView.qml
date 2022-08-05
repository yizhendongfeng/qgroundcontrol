/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

import QtQuick          2.3
import QtQuick.Controls 2.12
import QtQuick.Dialogs  1.2
import QtLocation       5.3
import QtPositioning    5.3
import QtQuick.Layouts  1.3
import QtQuick.Window   2.2
import QtQuick.Controls.Styles 1.4

import QGroundControl                   1.0
import QGroundControl.FlightMap         1.0
import QGroundControl.ScreenTools       1.0
import QGroundControl.Controls          1.0
import QGroundControl.FactSystem        1.0
import QGroundControl.FactControls      1.0
import QGroundControl.Palette           1.0
import QGroundControl.Controllers       1.0

Item {
    id: _root

    property bool planControlColapsed: false

    readonly property int   _decimalPlaces:             8
    readonly property real  _margin:                    ScreenTools.defaultFontPixelHeight * 0.5
    readonly property real  _toolsMargin:               ScreenTools.defaultFontPixelWidth * 0.75
    readonly property real  _radius:                    ScreenTools.defaultFontPixelWidth  * 0.5
    readonly property real  _rightPanelWidth:           Math.min(parent.width / 3, ScreenTools.defaultFontPixelWidth * 50)
    readonly property var   _defaultVehicleCoordinate:  QtPositioning.coordinate(37.803784, -122.462276)
    readonly property bool  _waypointsOnlyMode:         QGroundControl.corePlugin.options.missionWaypointsOnly

    property bool   _airspaceEnabled:                    QGroundControl.airmapSupported ? (QGroundControl.settingsManager.airMapSettings.enableAirMap.rawValue && QGroundControl.airspaceManager.connected): false
    property var    _missionController:                 _planMasterController.missionController
    property var    _geoFenceController:                _planMasterController.geoFenceController
    property var    _rallyPointController:              _planMasterController.rallyPointController
//    property var    _visualItems:                       _missionController.visualItems
    property bool   _lightWidgetBorders:                editorMap.isSatelliteMap
    property bool   _addROIOnClick:                     false
    property bool   _singleComplexItem:                 _missionController.complexMissionItemNames.length === 1
    property int    _editingLayer:                      layerTabBar.currentIndex ? _layers[layerTabBar.currentIndex] : _layerMission
    property int    _toolStripBottom:                   toolStrip.height + toolStrip.y
    property var    _appSettings:                       QGroundControl.settingsManager.appSettings
    property var    _planViewSettings:                  QGroundControl.settingsManager.planViewSettings
    property bool   _promptForPlanUsageShowing:         false
    property string _currentPlanFileDir:                _appSettings.missionSavePath
    property string _currentPlanFileName: ""
    property var    _currentCreatePlanObject: null

    readonly property var       _layers:                [_layerMission, _layerGeoFence, _layerRallyPoints]
    readonly property int       _layerMission:              1
    readonly property int       _layerGeoFence:             2
    readonly property int       _layerRallyPoints:          3
    readonly property string    _armedVehicleUploadPrompt:  qsTr("Vehicle is currently armed. Do you want to upload the mission to the vehicle?")
    property color _splitLineColor: "#707070"
    property color _splitLineColorLighter: "#424242"
    property int toolTipDelay: 500
    readonly property real  _iconSize:          ScreenTools.implicitButtonHeight

    function mapCenter() {
        var coordinate = editorMap.center
        coordinate.latitude  = coordinate.latitude.toFixed(_decimalPlaces)
        coordinate.longitude = coordinate.longitude.toFixed(_decimalPlaces)
        coordinate.altitude  = coordinate.altitude.toFixed(_decimalPlaces)
        return coordinate
    }

    function updateAirspace(reset) {
        if(_airspaceEnabled) {
            var coordinateNW = editorMap.toCoordinate(Qt.point(0,0), false /* clipToViewPort */)
            var coordinateSE = editorMap.toCoordinate(Qt.point(width,height), false /* clipToViewPort */)
            if(coordinateNW.isValid && coordinateSE.isValid) {
                QGroundControl.airspaceManager.setROI(coordinateNW, coordinateSE, true /*planView*/, reset)
            }
        }
    }

    property bool _firstMissionLoadComplete:    false
    property bool _firstFenceLoadComplete:      false
    property bool _firstRallyLoadComplete:      false
    property bool _firstLoadComplete:           false
    property bool _modePlanEdit:                false
    property var  _itemsBank:               _missionController.itemsBank
    property var  _itemCurrentBank
    property var  _itemCurrentWaypoint:     _itemCurrentBank ? _itemCurrentBank.itemCurrentWaypoint : null
    property var  _itemsInfoSlot:           _itemCurrentWaypoint ? _itemCurrentWaypoint.itemsInfoSlot : null
    property var  _itemCurrentInfoSlot:     _itemsInfoSlot ? _itemsInfoSlot.get(listViewInfoSlot.currentIndex) : null
    property var  _waypointPath


    MapFitFunctions {
        id:                         mapFitFunctions  // The name for this id cannot be changed without breaking references outside of this code. Beware!
        map:                        editorMap
        usePlannedHomePosition:     true
        planMasterController:       _planMasterController
    }

    on_AirspaceEnabledChanged: {
        if(QGroundControl.airmapSupported) {
            if(_airspaceEnabled) {
                planControlColapsed = QGroundControl.airspaceManager.airspaceVisible
                updateAirspace(true)
            } else {
                planControlColapsed = false
            }
        } else {
            planControlColapsed = false
        }
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
//            if (_itemsBank.count > 0) {
//                mainWindow.showComponentDialog(applyNewAltitude, qsTr("Apply new altitude"), mainWindow.showDialogDefaultWidth, StandardButton.Yes | StandardButton.No)
//            }
        }
    }

    Component {
        id: applyNewAltitude
        QGCViewMessage {
            message:    qsTr("You have changed the default altitude for mission items. Would you like to apply that altitude to all the items in the current mission?")
            function accept() {
                hideDialog()
                _missionController.applyDefaultMissionAltitude()
            }
        }
    }

    Component {
        id: promptForPlanUsageOnVehicleChangePopupComponent
        QGCPopupDialog {
            title:      _planMasterController.managerVehicle.isOfflineEditingVehicle ? qsTr("Plan View - Vehicle Disconnected") : qsTr("Plan View - Vehicle Changed")
            buttons:    StandardButton.NoButton

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
                        hideDialog();
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
                        hideDialog()
                    }
                }
            }
        }
    }


    Component {
        id: firmwareOrVehicleMismatchUploadDialogComponent
        QGCViewMessage {
            message: qsTr("This Plan was created for a different firmware or vehicle type than the firmware/vehicle type of vehicle you are uploading to. " +
                            "This can lead to errors or incorrect behavior. " +
                            "It is recommended to recreate the Plan for the correct firmware/vehicle type.\n\n" +
                            "Click 'Ok' to upload the Plan anyway.")

            function accept() {
                _planMasterController.sendToVehicle()
                hideDialog()
            }
        }
    }

    Component {
        id: noItemForKML
        QGCViewMessage {
            message:    qsTr("You need at least one item to create a KML.")
        }
    }

    Component {
        id: noBankExist

        QGCPopupDialog {
            title:      qsTr("Warning")
            buttons:    StandardButton.Close
            width:      400
            height:     70
            QGCLabel {
                anchors.margins:    10
                anchors.fill:       parent
                wrapMode:           Text.WordWrap
                font.pointSize:     ScreenTools.defaultFontPointSize * 1.2
                text:               qsTr("You must create a new bank before inserting waypoint!")
            }
        }
    }


    PlanMasterController {
        id:         _planMasterController
        flyView:    false

        Component.onCompleted: {
            _planMasterController.start()
//            _missionController.setCurrentPlanViewSeqNum(0, true)
            globals.planMasterControllerPlanView = _planMasterController
        }

        onPromptForPlanUsageOnVehicleChange: {
            if (!_promptForPlanUsageShowing) {
                _promptForPlanUsageShowing = true
                mainWindow.showPopupDialogFromComponent(promptForPlanUsageOnVehicleChangePopupComponent)
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

            if (readyForSaveState() == MissionController.NotReadyForSaveData) {
                waitingOnIncompleteDataMessage(save)
                return false
            } else if (readyForSaveState() == MissionController.NotReadyForSaveTerrain) {
                waitingOnTerrainDataMessage(save)
                return false
            }
            return true
        }

        function upload() {
//            if (!checkReadyForSaveUpload(false /* save */)) {
//                return
//            }
            switch (_missionController.sendToVehiclePreCheck()) {
                case MissionController.SendToVehiclePreCheckStateOk:
                    sendToVehicle()
                    break
                case MissionController.SendToVehiclePreCheckStateActiveMission:
                    mainWindow.showMessageDialog(qsTr("Send To Vehicle"), qsTr("Current mission must be paused prior to uploading a new Plan"))
                    break
                case MissionController.SendToVehiclePreCheckStateFirwmareVehicleMismatch:
                    mainWindow.showComponentDialog(firmwareOrVehicleMismatchUploadDialogComponent, qsTr("Plan Upload"), mainWindow.showDialogDefaultWidth, StandardButton.Ok | StandardButton.Cancel)
                    break
            }
        }

        function loadFromSelectedFile() {
            fileDialog.title =          qsTr("Select Plan File")
            fileDialog.planFiles =      true
            fileDialog.selectExisting = true
            fileDialog.nameFilters =    _planMasterController.loadNameFilters
            fileDialog.openForLoad()
        }

        function saveToSelectedFile() {
            if (!checkReadyForSaveUpload(true /* save */)) {
                return
            }
            fileDialog.title =          qsTr("Save Plan")
            fileDialog.planFiles =      true
            fileDialog.selectExisting = false
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
            fileDialog.selectExisting = false
            fileDialog.nameFilters =    ShapeFileHelper.fileDialogKMLFilters
            fileDialog.openForSave()
        }
    }

    Connections {
        target: _missionController

        function onNewItemsFromVehicle() {

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
//        for (var i=0; i<_missionController.visualItems.count; i++) {
//            var vmi = _missionController.visualItems.get(i)
//            if (vmi.readyForSaveState === VisualMissionItem.NotReadyForSaveData) {
//                _missionController.setCurrentPlanViewSeqNum(vmi.sequenceNumber, true)
//                break
//            }
//        }
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

        onAcceptedForSave: {
            if (planFiles) {
                _planMasterController.saveToFile(file)
            } else {
                _planMasterController.saveToKml(file)
            }
            close()
        }

        onAcceptedForLoad: {
            _planMasterController.loadFromFile(file)
            _planMasterController.fitViewportToItems()
//            _missionController.setCurrentPlanViewSeqNum(0, true)
            close()
        }
    }

    Item {
        id:             panel
        anchors.fill:   parent

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
            property rect centerViewport:   Qt.rect(_leftToolWidth + _margin,  _margin, editorMap.width - _leftToolWidth - _rightToolWidth - (_margin * 2), height - _margin - _margin)

            property real _leftToolWidth:       toolStrip.x + toolStrip.width
            property real _rightToolWidth:      rightPanel.width + rightPanel.anchors.rightMargin
            property real _nonInteractiveOpacity:  0.5

            // Initial map position duplicates Fly view position
            Component.onCompleted: editorMap.center = QGroundControl.flightMapPosition

            QGCMapPalette { id: mapPal; lightColors: editorMap.isSatelliteMap }

            onZoomLevelChanged: {
                QGroundControl.flightMapZoom = zoomLevel
                updateAirspace(false)
            }
            onCenterChanged: {
                QGroundControl.flightMapPosition = center
                updateAirspace(false)
            }

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    // Take focus to close any previous editing
                    editorMap.focus = true
                    var coordinate = editorMap.toCoordinate(Qt.point(mouse.x, mouse.y), false /* clipToViewPort */)
                    coordinate.latitude = coordinate.latitude.toFixed(_decimalPlaces)
                    coordinate.longitude = coordinate.longitude.toFixed(_decimalPlaces)
                    coordinate.altitude = coordinate.altitude.toFixed(_decimalPlaces)
                    switch (_editingLayer) {
                    case _layerMission:
                        if (!_itemCurrentBank) {
                            if (!mainWindow.preventViewSwitch()) {
                                showPopupDialogFromComponent(noBankExist)
                            }
                            break;
                        }

                        if (addWaypointRallyPointAction.checked) {
//                            insertSimpleItemAfterCurrent(coordinate)
                            _itemCurrentBank.insertWaypoint(coordinate, _itemCurrentBank.indexCurrentWaypoint + 1)
                            splitSegmentItem._updateSplitCoord()
                        } else if (_addROIOnClick) {
                            insertROIAfterCurrent(coordinate)
                            _addROIOnClick = false
                        }

                        break
                    case _layerRallyPoints:
                        if (_rallyPointController.supported && addWaypointRallyPointAction.checked) {
                            _rallyPointController.addPoint(coordinate)
                        }
                        break
                    }
                }
            }

            // Add the mission item visuals to the map
            Repeater {
                model: _missionController.itemsBank
                delegate: Repeater {
                    property var _itemBank: object
                    model: _itemBank.itemsWaypoint
                    delegate: MissionItemMapVisual {
                        property var _currentWaypoint: _itemBank.itemCurrentWaypoint ? _itemBank.itemCurrentWaypoint : null
                        map:        editorMap
                        opacity:    _editingLayer == _layerMission ? 1 : editorMap._nonInteractiveOpacity
                        interactive: _editingLayer == _layerMission
                        enabled:    _modePlanEdit
                        focus:      _currentWaypoint ? _currentWaypoint.isCurrentWaypoint : false
                        onClicked: {
                            focus = true
                            console.log("waypoint clicked, indexBank:", indexBank, "indexWaypoint:", indexWaypoint)
                            var itemNewBank = _missionController.itemsBank.get(indexBank)
                            if (_itemCurrentBank !== itemNewBank) {
                                if (_itemCurrentBank.itemCurrentWaypoint)
                                    _itemCurrentBank.itemCurrentWaypoint.isCurrentWaypoint = false
                                listViewBank.currentIndex = indexBank
                            }
                            _itemCurrentBank = itemNewBank
                            _itemCurrentBank.setItemCurrentWaypoint(indexWaypoint)
                            splitSegmentItem._updateSplitCoord()
                        }
                        Keys.onPressed: {
                            if (event.key === Qt.Key_Delete) {
                                console.log("delete waypoint index:", _itemCurrentBank.indexCurrentWaypoint);
                                event.accepted = true;
                                _itemCurrentBank.removeWaypoint(_itemCurrentBank.indexCurrentWaypoint)
                                splitSegmentItem._updateSplitCoord()
                            }
                        }
                    }
                }
            }

            MapItemView {
                model: _missionController.itemsBank
                delegate: MapPolyline {
                    line.width: 3
                    line.color: object === _itemCurrentBank ? "red" : QGroundControl.globalPalette.mapMissionTrajectory
                    z:          QGroundControl.zOrderWaypointLines
                    path:       object.waypointPath
                }
            }



            // Direction arrows in waypoint lines
//            MapItemView {
//                model: _editingLayer == _layerMission ? _missionController.directionArrows : undefined

//                delegate: MapLineArrow {
//                    fromCoord:      object ? object.coordinate1 : undefined
//                    toCoord:        object ? object.coordinate2 : undefined
//                    arrowPosition:  3
//                    z:              QGroundControl.zOrderWaypointLines + 1
//                }
//            }

            // UI for splitting the current segment
            MapQuickItem {
                id:             splitSegmentItem
                anchorPoint.x:  sourceItem.width / 2
                anchorPoint.y:  sourceItem.height / 2
                z:              QGroundControl.zOrderWaypointLines + 1
                visible:        _editingLayer == _layerMission && _modePlanEdit
                                && (_itemCurrentWaypoint ? _itemCurrentWaypoint.isCurrentWaypoint : false)

                sourceItem: SplitIndicator {
                    onClicked:  _itemCurrentBank.insertWaypoint(splitSegmentItem.coordinate, _itemCurrentBank.indexCurrentWaypoint)
                }

                function _updateSplitCoord() {
                    if (_itemCurrentBank.indexCurrentWaypoint > 0 && _itemCurrentBank.waypointPath.length > 1) {
                        var coordinate1 = _itemCurrentBank.waypointPath[_itemCurrentBank.indexCurrentWaypoint - 1]
                        var coordinate2 = _itemCurrentBank.waypointPath[_itemCurrentBank.indexCurrentWaypoint]
                        var distance = coordinate1.distanceTo(coordinate2)
                        var azimuth = coordinate1.azimuthTo(coordinate2)
                        splitSegmentItem.coordinate = coordinate1.atDistanceAndAzimuth(distance / 2, azimuth)
                    } else {
                        coordinate = QtPositioning.coordinate()
                    }
                }

                Connections {
                    target:                 _itemCurrentBank ? _itemCurrentBank.itemCurrentWaypoint : null
                    function onCoordinateChanged() { splitSegmentItem._updateSplitCoord() }
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

        }

        LeftPanel { // 整合任务管理
            id: leftPanel
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: _rightPanelWidth
            Column {
                id: columnLeftPanel
                anchors.fill: parent
                anchors.leftMargin: 2
                anchors.topMargin: 2
                spacing: 0
                Rectangle {
                    id: fileToolBar
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 50
                    color:      qgcPal.toolbarBackground

                    QGCLabel {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.leftMargin: ScreenTools.defaultFontPixelWidth + 5
                        text: qsTr("Plan")
                    }
                    Row {
                        id:                     viewButtonRow
                        anchors.right: parent.right
                        anchors.rightMargin: ScreenTools.defaultFontPixelWidth
                        anchors.verticalCenter: parent.verticalCenter
                        spacing:                ScreenTools.defaultFontPixelWidth
                        QGCIconButton {
                            id:                     newFileButton
                            iconSource:            "/qmlimages/NewFile.svg"
                            onClicked: {
                                if (!mainWindow.preventViewSwitch()) {
                                    showPopupDialogFromComponent(componentCreatePlanDialog)
                                }
                            }
                        }

                        QGCIconButton {
                            id:                 openFileButton
                            iconSource:         "/qmlimages/OpenFile.svg"
                            onClicked: {
                                _planMasterController.loadFromSelectedFile()
                            }
                        }
                    }

                    Rectangle {
                        height: 1
                        color: _splitLineColor
                        anchors.bottom: parent.bottom
                        anchors.left: parent.left
                        anchors.right: parent.right
                    }
                }
                PlanListView {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: Math.min(contentHeight, columnLeftPanel.height - fileToolBar.y + columnLeftPanel.spacing)

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
            onFocusChanged: console.log("leftpanel focus changed: ", focus)

        }

        //-----------------------------------------------------------
        ToolStripColumn {
            id:                 toolStrip
            anchors.top: parent.top
            anchors.topMargin:    1//_toolsMargin
            anchors.left:       parent.left
            anchors.leftMargin: -width
            z:                  QGroundControl.zOrderWidgets - 1
            maxHeight:          parent.height - toolStrip.y
//            maxWidth:           parent.width - rightPanel.width
//            title:              qsTr("Plan")

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
                        id:                 addWaypointRallyPointAction
                        text:               qsTr("Waypoint")
                        iconSource:         "/qmlimages/MapAddMission.svg"
                        enabled:            true
                        visible:            toolStrip._isMissionLayer
                        checkable:          true
                    },
                    ToolStripAction {
                        text:               _singleComplexItem ? _missionController.complexMissionItemNames[0] : qsTr("Pattern")
                        iconSource:         "/qmlimages/MapDrawShape.svg"
                        enabled:            _missionController.flyThroughCommandsAllowed
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
                                showPopupDialogFromComponent(componentQuitPlanEdit)
                            } else {
                                if (_itemCurrentWaypoint) {
                                    _itemCurrentWaypoint.isCurrentWaypoint = false
                                }
                                _itemCurrentBank = null
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
            color:              qgcPal.windowShade
            opacity:            1 //layerTabBar.visible ? 0.2 : 0
            anchors.bottom:     parent.bottom
            anchors.right:      parent.right
            anchors.rightMargin: -width//_toolsMargin
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
            Component.onCompleted: {
                console.log("_rightPanelWidth:", height, _root.width / 3, ScreenTools.defaultFontPixelWidth * 30)
            }
        }
        //-------------------------------------------------------
        // Right Panel Controls
        Item {
            anchors.fill:           rightPanel
            anchors.topMargin:      _toolsMargin
            DeadMouseArea {
                anchors.fill:   parent
            }

            ColumnLayout {
                id:                 rightControls
                spacing:            ScreenTools.defaultFontPixelHeight * 0.5
                anchors.fill:       parent

                //-------------------------------------------------------
                // Mission Controls (Expanded)
                QGCTabBar {
                    id:         layerTabBar
                    Layout.fillWidth:   true

                    Component.onCompleted: currentIndex = 0
                    QGCTabButton {
                        text:       qsTr("Mission")
                    }
                    QGCTabButton {
                        text:       qsTr("Fence")
                    }
                }
                //-------------------------------------------------------
                // Mission Controls (Colapsed)
                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: _splitLineColor
                }

                StackLayout {
                    Layout.fillWidth:   true
                    Layout.fillHeight:  true
                    currentIndex: layerTabBar.currentIndex
                    Item {  // mission
                        Layout.fillWidth:   true
                        Layout.fillHeight:  true
                        ColumnLayout {
                            anchors.fill: parent
                            // 航线名
                            TextField {
                                id: textFieldPlanName
                                property string fileName: _currentPlanFileName
                                implicitHeight: 30
                                implicitWidth: rightControls.width -  20
                                Layout.alignment: Qt.AlignVCenter | Qt.AlignHCenter
                                text: fileName
                                color: activeFocus ? qgcPal.textFieldText : qgcPal.text
                                font.pointSize: ScreenTools.defaultFontPointSize * 1.2
                                selectedTextColor: qgcPal.text
                                antialiasing:       true
                                onActiveFocusChanged: selectAllIfActiveFocus()
                                selectByMouse: true
                                background: Rectangle {
                                    radius: 2
                                    color: textFieldPlanName.activeFocus ? qgcPal.textField : "transparent"
                                    border.color: color
                                }

                                onFileNameChanged: {
                                    text = fileName
                                }

                                onEditingFinished: {
                                    focus = false
                                    if (_currentPlanFileName !== text && text != "") {
                                        var previousPlanFileName = _currentPlanFileName
                                        _currentPlanFileName = text
                                        if (!_planMasterController.renameCurrentFile(text)) {
                                            _currentPlanFileName = previousPlanFileName
                                            text = previousPlanFileName
                                        }
                                    } else if (text == "") {
                                        text = fileName
                                    }
                                }

                                function selectAllIfActiveFocus() {
                                    if (activeFocus) {
                                        selectAll()
                                    }
                                }
                            }
                            Rectangle {
                                Layout.fillWidth: true
                                height: 1
                                color: _splitLineColor
                            }

                            Item {
                                id: missionInfo
                                Layout.fillWidth: true
                                Layout.preferredHeight: 80
                                anchors.margins: _margin
                                GridLayout {
                                    id: gridLayoutMissionInfo
                                    columns: 2
                                    rowSpacing: 0
                                    columnSpacing: _margin
                                    Layout.alignment: Qt.AlignVCenter | Qt.AlignHCenter
                                    anchors.fill: parent
                                    anchors.margins: _margin

                                    QGCLabel { text: qsTr("BankCount"); font.pointSize: ScreenTools.defaultFontPointSize}
                                    QGCLabel { text: qsTr("TotalDistance"); font.pointSize: ScreenTools.defaultFontPointSize}
                                    QGCLabel {text: "12"; font.pointSize: ScreenTools.defaultFontPointSize + 4}
                                    QGCLabel {text: "12345"; font.pointSize: ScreenTools.defaultFontPointSize + 4}

                                    Item { width: 1; height: 5 }
                                    Item { width: 1; height: 5 }

                                    QGCLabel { text: qsTr("EstimatedFlyTime"); font.pointSize: ScreenTools.defaultFontPointSize}
                                    QGCLabel { text: qsTr("WaypointCount"); font.pointSize: ScreenTools.defaultFontPointSize}
                                    QGCLabel {text: "00:23:45"; font.pointSize: ScreenTools.defaultFontPointSize + 4}
                                    QGCLabel {text: "12345"; font.pointSize: ScreenTools.defaultFontPointSize + 4}
                                }
                            }
                            Rectangle {
                                Layout.fillWidth: true
                                height: 1
                                color: _splitLineColor
                            }
                            // 航线设置与航点设置切换
                            QGCTabBar {
                                id: tabBarMissionWaypoint
                                Layout.alignment: Qt.AlignHCenter
                                contentHeight: 30
                                Layout.preferredWidth: parent.width * 0.9
                                Component.onCompleted: currentIndex = 0
                                QGCTabButton {
                                    text:       qsTr("Banks")
                                }
                                QGCTabButton {
                                    text:       qsTr("Waypoint")
                                }
                            }
                            Rectangle {
                                Layout.fillWidth: true
                                height: 1
                                color: _splitLineColor
                            }

                            // 航线与航点界面切换
                            StackLayout {
                                currentIndex: tabBarMissionWaypoint.currentIndex
                                Layout.fillWidth:   true
                                Layout.fillHeight:  true

                                Item {
                                    id: tabMissionSettingEditor
                                    Layout.fillHeight: true
                                    Layout.fillWidth: true

                                    Column {
                                        id: columnBankInfo
                                        anchors.fill: parent
                                        Item {
                                            id: bankTools
                                            width: parent.width
                                            height: 40
                                            QGCLabel {
                                                anchors.left: parent.left
                                                anchors.leftMargin: 10
                                                anchors.verticalCenter: parent.verticalCenter
                                                text: "bank count: " + listViewBank.count
                                            }

                                            QGCIconButton {
                                                id: buttonAddBank
                                                anchors.right: parent.right
                                                anchors.rightMargin: _margin
                                                anchors.verticalCenter: parent.verticalCenter
                                                width: parent.height * 2 / 3
                                                iconSource: "/qmlimages/NewFile.svg"
                                                onClicked: {
                                                    if (_itemCurrentBank && _itemCurrentBank.itemCurrentWaypoint)
                                                        _itemCurrentBank.itemCurrentWaypoint.isCurrentWaypoint = false
                                                    _itemCurrentBank = _missionController.insertNewBank()
                                                    listViewBank.currentIndex = _itemCurrentBank.idBank
                                                    console.log("create new bank ", _itemCurrentBank)
                                                }
                                            }
                                        }
                                        Rectangle {
                                            width: parent.width
                                            height: 1
                                            color: _splitLineColor
                                        }

                                        QGCListView {
                                            id: listViewBank
                                            width: parent.width
                                            height: Math.min(contentHeight, columnBankInfo.height - listViewBank.y)
                                            Layout.leftMargin: 10
                                            spacing: 5
                                            focus: true
                                            model: _missionController.itemsBank
                                            delegate: componentBankInfo
                                            onCurrentIndexChanged: {
                                                if (_itemCurrentBank && _itemCurrentBank.itemCurrentWaypoint)
                                                    _itemCurrentBank.itemCurrentWaypoint.isCurrentWaypoint = false
                                                _itemCurrentBank = listViewBank.currentIndex < 0 ?
                                                            null : _missionController.itemsBank.get(listViewBank.currentIndex)
                                            }
                                            Component.onCompleted: {
                                                console.log("listViewBank currentIndex:", currentIndex, "count:" <<  _missionController.itemsBank.count)
                                                currentIndex = -1
                                            }
                                        }
                                    }
                                }

                                Item {
                                    id: tabWaypointEditor
                                    Layout.fillHeight: true
                                    Layout.fillWidth: true
                                    Layout.margins: ScreenTools.defaultFontPixelHeight * 0.25
                                    property int indexCurrentWaypoint: _itemCurrentBank ? _itemCurrentBank.indexCurrentWaypoint : -1
                                    Column {
                                        id: columnWaypointInfo
                                        anchors.fill: parent
                                        Item {
                                            id:    waypointChange
                                            width: parent.width
                                            height: 40

                                            QGCIconButton {
                                                id: buttonLeftArrow
                                                anchors.left: parent.left
                                                anchors.leftMargin: 20
                                                anchors.verticalCenter: parent.verticalCenter
                                                width:                  _iconSize
                                                height:                 width
                                                sourceSize.height:      _iconSize
                                                iconSource:            "qrc:/InstrumentValueIcons/cheveron-left.svg"
                                                enabled: tabWaypointEditor.indexCurrentWaypoint > 0
                                                onClicked: {
                                                    _itemCurrentBank.setItemCurrentWaypoint(_itemCurrentBank.indexCurrentWaypoint - 1)
                                                }
                                            }

                                            QGCLabel {
                                                id: waypointIndexLabel
                                                anchors.centerIn: parent
                                                text: "bank " + listViewBank.currentIndex +"  Waypoint " + tabWaypointEditor.indexCurrentWaypoint
                                            }

                                            QGCIconButton {
                                                id: buttonRightArrow
                                                anchors.right: parent.right
                                                anchors.rightMargin: 20
                                                anchors.verticalCenter: parent.verticalCenter
                                                width:                  _iconSize
                                                height:                 width
                                                iconSource:            "qrc:/InstrumentValueIcons/cheveron-right.svg"
                                                sourceSize.height:      _iconSize
                                                enabled: _itemCurrentBank ? tabWaypointEditor.indexCurrentWaypoint < _itemCurrentBank.nWp - 1 : false
                                                onClicked: {
                                                    _itemCurrentBank.setItemCurrentWaypoint(_itemCurrentBank.indexCurrentWaypoint + 1)
                                                }
                                            }
                                        }

                                        Rectangle {
                                            width: parent.width
                                            height: 1
                                            color: _splitLineColor
                                        }

                                        QGCListView {
                                            id: listViewInfoSlot
                                            property int indexCurrentInfoSlot: _itemCurrentWaypoint ? _itemCurrentWaypoint.indexCurrentInfoSlot : -1
                                            width: parent.width
                                            height: Math.min(contentHeight, columnWaypointInfo.height - listViewInfoSlot.y)
                                            Layout.leftMargin: 10
                                            spacing: 5
                                            focus: true
                                            model: _itemCurrentWaypoint ? _itemCurrentWaypoint.itemsInfoSlot : null
                                            currentIndex: indexCurrentInfoSlot
                                            delegate: componentWaypointInfo
                                            onIndexCurrentInfoSlotChanged: {
                                                currentIndex = indexCurrentInfoSlot
                                                console.log("listViewInfoSlot onCurrentIndexChanged", currentIndex, _itemCurrentWaypoint ? _itemCurrentWaypoint.indexCurrentInfoSlot : -1)
                                            }
                                        }
                                    }
                                }

                            }
                        }
                    }

                    Item {
                        Layout.fillWidth:   true
                        Layout.fillHeight:  true
                        GeoFenceEditor { // fence
                            anchors.top:    parent.top
                            anchors.left:   parent.left
                            anchors.right:  parent.right
                            anchors.margins:        ScreenTools.defaultFontPixelWidth
                            myGeoFenceController:   _geoFenceController
                            flightMap:              editorMap
                            visible:                _editingLayer == _layerGeoFence
                        }
                    }
                }
            }



//            Item {
//                id:                     missionItemEditor
//                anchors.left:           parent.left
//                anchors.right:          parent.right
//                anchors.top:            rightControls.bottom
//                anchors.topMargin:      ScreenTools.defaultFontPixelHeight * 0.25
//                anchors.bottom:         parent.bottom
//                anchors.bottomMargin:   ScreenTools.defaultFontPixelHeight * 0.25
//                visible:                false//_editingLayer == _layerMission && !planControlColapsed
//                QGCListView {
//                    id:                 missionItemEditorListView
//                    anchors.fill:       parent
//                    spacing:            ScreenTools.defaultFontPixelHeight / 4
//                    orientation:        ListView.Vertical
//                    model:              _missionController.visualItems
//                    cacheBuffer:        Math.max(height * 2, 0)
//                    clip:               true
//                    currentIndex:       _missionController.currentPlanViewSeqNum
//                    highlightMoveDuration: 250
//                    visible:            _editingLayer == _layerMission && !planControlColapsed
//                    //-- List Elements
//                    delegate: MissionItemEditor {
//                        map:            editorMap
//                        masterController:  _planMasterController
//                        missionItem:    object
//                        width:          parent.width
//                        readOnly:       false
//                        onClicked:      _missionController.setCurrentPlanViewSeqNum(object.sequenceNumber, false)
//                        onRemove: {
//                            var removeVIIndex = index
//                            _missionController.removeVisualItem(removeVIIndex)
//                            if (removeVIIndex >= _missionController.visualItems.count) {
//                                removeVIIndex--
//                            }
//                        }
//                        onSelectNextNotReadyItem:   selectNextNotReady()
//                    }
//                }
//            }
        }

        MapScale {
            id:                     mapScale
            anchors.margins:        _toolsMargin
            anchors.bottom:         parent.bottom
            anchors.left:           toolStrip.y + toolStrip.height + _toolsMargin > mapScale.y && leftPanel.x + leftPanel.width < mapScale.y ? toolStrip.right: leftPanel.right
            mapControl:             editorMap
            buttonsOnLeft:          true
            terrainButtonVisible:   false
            terrainButtonChecked:   false
        }
    }

    Component {
        id: syncLoadFromVehicleOverwrite
        QGCViewMessage {
            id:         syncLoadFromVehicleCheck
            message:   qsTr("You have unsaved/unsent changes. Loading from the Vehicle will lose these changes. Are you sure you want to load from the Vehicle?")
            function accept() {
                hideDialog()
                _planMasterController.loadFromVehicle()
            }
        }
    }

    Component {
        id: syncLoadFromFileOverwrite
        QGCViewMessage {
            id:         syncLoadFromVehicleCheck
            message:   qsTr("You have unsaved/unsent changes. Loading from a file will lose these changes. Are you sure you want to load from a file?")
            function accept() {
                hideDialog()
                _planMasterController.loadFromSelectedFile()
            }
        }
    }

    property var createPlanRemoveAllPromptDialogMapCenter
    property var createPlanRemoveAllPromptDialogPlanCreator
    Component {
        id: createPlanRemoveAllPromptDialog
        QGCViewMessage {
            message: qsTr("Are you sure you want to remove current plan and create a new plan? ")
            function accept() {
                createPlanRemoveAllPromptDialogPlanCreator.createPlan(createPlanRemoveAllPromptDialogMapCenter)
                hideDialog()
            }
        }
    }

    Component {
        id: clearVehicleMissionDialog
        QGCViewMessage {
            message: qsTr("Are you sure you want to remove all mission items and clear the mission from the vehicle?")
            function accept() {
                _planMasterController.removeAllFromVehicle()
//                _missionController.setCurrentPlanViewSeqNum(0, true)
                hideDialog()
            }
        }
    }

    //- ToolStrip DropPanel Components

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

    Component {
        id: createPlanDropPanel
        Rectangle {
            color: qgcPal.window
            ColumnLayout {
                id: createPlanColumnHolder
                anchors.fill: parent
                spacing: _margin
                Rectangle {
                    Layout.fillWidth: true
                    color: qgcPal.windowShadeLight
                    QGCLabel {
                        id:                 createPlanLabel
                        anchors.horizontalCenter: parent.horizontalCenter
                        Layout.fillWidth:   true
                        wrapMode:           Text.WordWrap
                        text:               qsTr("Create new plan")
                        visible:            _planMasterController.dirty
                    }
                    QGCIconButton {
                        id: iconButtonClose
                        anchors.right: parent.right
                        iconSource: "/res/XDelete.svg"
                        onClicked: {
                            enterPlanEditMode(false)
                            console.log("close create plan widget")
                        }
                    }
                }
                RowLayout {
                    spacing:         _margin
                    visible:            createSection.visible

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
                                        createPlanRemoveAllPromptDialogMapCenter = _mapCenter()
                                        createPlanRemoveAllPromptDialogPlanCreator = object
                                        mainWindow.showComponentDialog(createPlanRemoveAllPromptDialog, qsTr("Create Plan"), mainWindow.showDialogDefaultWidth, StandardButton.Yes | StandardButton.No)
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
            }
        }
    }

    Component {
        id: syncDropPanel

        ColumnLayout {
            id:         columnHolder
            spacing:    _margin

            property string _overwriteText: (_editingLayer == _layerMission) ? qsTr("Mission overwrite") : ((_editingLayer == _layerGeoFence) ? qsTr("GeoFence overwrite") : qsTr("Rally Points overwrite"))

            QGCLabel {
                id:                 unsavedChangedLabel
                Layout.fillWidth:   true
                wrapMode:           Text.WordWrap
                text:               globals.activeVehicle ?
                                        qsTr("You have unsaved changes. You should upload to your vehicle, or save to a file.") :
                                        qsTr("You have unsaved changes.")
                visible:            _planMasterController.dirty
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
                visible:            storageSection.visible

                QGCButton {
                    text:               qsTr("Save")
                    Layout.fillWidth:   true
                    enabled:            !_planMasterController.syncInProgress && _planMasterController.dirty
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
            }

            SectionHeader {
                id:                 vehicleSection
                Layout.fillWidth:   true
                text:               qsTr("Vehicle")
            }

            RowLayout {
                Layout.fillWidth:   true
                spacing:            _margin
                visible:            vehicleSection.visible

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
                        if (_planMasterController.dirty) {
                            mainWindow.showComponentDialog(syncLoadFromVehicleOverwrite, columnHolder._overwriteText, mainWindow.showDialogDefaultWidth, StandardButton.Yes | StandardButton.Cancel)
                        } else {
                            _planMasterController.loadFromVehicle()
                        }
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
                        mainWindow.showComponentDialog(clearVehicleMissionDialog, text, mainWindow.showDialogDefaultWidth, StandardButton.Yes | StandardButton.Cancel)
                    }
                }
            }
        }
    }



    Component {
        id: componentCreatePlanDialog

        QGCPopupDialog {
            id:         createPlanDialog
            title:      qsTr("Create Plan")
            buttons:    StandardButton.Close

            property real _toolButtonHeight:    ScreenTools.defaultFontPixelHeight * 3
            property real _margins:             ScreenTools.defaultFontPixelWidth


            RowLayout {
                spacing:         _margin
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
                                _currentCreatePlanObject = object
                                createPlanDialog.hideDialog(true)
                                var popup = mainWindow.showPopupDialogFromComponent(componentSetPlanFileName)
                            }

                        }
                        Component.onCompleted: console.log(object.name, "highlighted:", button.highlighted)
                    }
                }
            }
        }

    }

    Component {
        id: componentSetPlanFileName

        QGCPopupDialog {
            id:        setPlanNameDialog
            title:      qsTr("Set plan name")
            buttons:    StandardButton.Cancel |StandardButton.Ok
            width: 400
            onHideDialog: {
                if (accepted) {
                    if (planNameTextInput.displayText === "")
                        return
                    _currentPlanFileName = planNameTextInput.displayText
                    console.log(_currentPlanFileDir, _currentPlanFileName)
                    if (_planMasterController.containsItems) {
                        createPlanRemoveAllPromptDialogMapCenter = _mapCenter()
                        createPlanRemoveAllPromptDialogPlanCreator = _currentCreatePlanObject
                        mainWindow.showComponentDialog(createPlanRemoveAllPromptDialog, qsTr("Create Plan"), mainWindow.showDialogDefaultWidth, StandardButton.Yes | StandardButton.No)
                    } else {
                        _currentCreatePlanObject.createPlan(_mapCenter())
                    }
                    enterPlanEditMode(true)
                }
            }

            function _mapCenter() {
                var centerPoint = Qt.point(editorMap.centerViewport.left + (editorMap.centerViewport.width / 2), editorMap.centerViewport.top + (editorMap.centerViewport.height / 2))
                return editorMap.toCoordinate(centerPoint, false /* clipToViewPort */)
            }

            RowLayout {
                spacing: ScreenTools.defaultFontPixelWidth
                anchors.right: parent.right
                anchors.left: parent.left
                QGCLabel {
                    text: qsTr("Plan Name:")
                }
                QGCTextField {
                    id: planNameTextInput
                    Layout.fillWidth:  true
                    Layout.fillHeight: true
                    placeholderText: qsTr("Please input your plan name")
                }
            }
        }
    }

    Component {
        id: componentQuitPlanEdit

        QGCPopupDialog {
            id:         dialogQuitPlanEdit
            title:      qsTr("Quit Plan Edit")
            buttons:    StandardButton.Cancel | StandardButton.Save
            width:      300
            height:     80
            onHideDialog: {
                if (accepted) {
                    _planMasterController.saveToFile(_currentPlanFileDir + "/" + _currentPlanFileName)
                } else {
                    // 不保存修改的内容，则恢复显示文件中的内容来覆盖修改界面内容
                    _planMasterController.loadFromFile(_currentPlanFileDir + "/" + _currentPlanFileName)
                }

                if (_itemCurrentWaypoint) {
                    _itemCurrentWaypoint.isCurrentWaypoint = false
                }
                _itemCurrentBank = null
                enterPlanEditMode(false)
            }

            ColumnLayout {
                spacing: ScreenTools.defaultFontPixelWidth
                anchors.fill: parent

                QGCLabel {
                    text: qsTr("Save Changed?")
                    font.pointSize:     ScreenTools.defaultFontPointSize * 1.2
                }
            }
        }
    }
    // bank信息组件
    Component {
        id: componentBankInfo
        Rectangle {
            id: rect
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 1
            height: 50
            state: "normal"
            property bool rowHovered: mouseArea.containsMouse
            color: ListView.isCurrentItem ? qgcPal.window : (rowHovered ? qgcPal.windowShadeDark : qgcPal.windowShade)

            MouseArea {
                id: mouseArea
                anchors.fill: parent
                hoverEnabled: true
                propagateComposedEvents: true

                onClicked: {
                    listViewBank.currentIndex = index
                    listViewBank.focus = true
                    _waypointPath = object.waypointPath
                    console.log("bank index: ", object.idBank, "focus: ", focus)
                }

                RowLayout {
                    id: rowLayout
                    anchors.fill: parent
                    anchors.leftMargin: 5

                    QGCLabel {
                        id: labelBankId
                        Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter
                        Layout.leftMargin: 5
//                        implicitHeight: rect.height / 2
                        Layout.fillWidth: true
                        text: object.idBank//index
//                        color: qgcPal.text
//                        hoverEnabled: listViewBank.currentIndex === index
//                        enabled: listViewBank.currentIndex === index
//                        background: Rectangle {
//                            anchors.fill: parent
//                            color: "transparent"
//                            border.color: textInput.focus ? qgcPal.colorGrey : "transparent"
//                        }
                    }

                    QGCIconButton {
                        id: btnDelete
                        Layout.alignment: Qt.AlignRight
                        Layout.rightMargin: _margin
                        Layout.preferredHeight: _iconSize
                        Layout.preferredWidth: _iconSize
                        visible: rect.ListView.isCurrentItem || rect.rowHovered ? true : false
                        showNormal: hovered
                        iconSource: "/qmlimages/DeleteFile.svg"
//                        ToolTip.text: qsTr("delete bank")
//                        ToolTip.visible: hovered
//                        highlighted: hovered
                        onClicked: {
//                            toolTipDelete.visible = false
                            ToolTip.visible = false
                            _missionController.removeVisualItemBank(index)
                            console.log("remove bank: ", labelBankId.text)
                        }
                        ToolTip {
                            id: toolTipDelete
                            text: "delete bank"
                            visible: parent.hovered
                            delay: toolTipDelay
                        }

                    }
                }

            }

            Rectangle {
                height: 1
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                color: _splitLineColorLighter
            }

            // Add the mission item visuals to the map
            MapPolyline {
                line.width: 3
                // Note: Special visuals for ROI are hacked out for now since they are not working correctly
                line.color: QGroundControl.globalPalette.mapMissionTrajectory
                z:          QGroundControl.zOrderWaypointLines
                path:       object.waypointPath
            }
        }
    }


    // 航点信息组件
    Component {
        id: componentWaypointInfo
        Rectangle {
            id: rect
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 1
            height: 50
            state: "normal"
            property bool rowHovered: mouseArea.containsMouse
            color: ListView.isCurrentItem ? qgcPal.window : (rowHovered ? qgcPal.windowShadeDark : qgcPal.windowShade)

            MouseArea {
                id: mouseArea
                anchors.fill: parent
                hoverEnabled: true
                propagateComposedEvents: true

                onClicked: {
                    listViewInfoSlot.currentIndex = index
                    listViewInfoSlot.focus = true
                    console.log("listViewWaypoint waypoint index: ", index, "focus: ", focus, "listViewWaypoint:", listViewInfoSlot)
                }
                RowLayout {
                    id: rowLayout
                    anchors.fill: parent
                    anchors.leftMargin: 5

                    QGCLabel {
                        id: labelWaypointId
                        Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter
                        Layout.leftMargin: 5
//                        implicitHeight: rect.height / 2
                        Layout.fillWidth: true
                        text:  "infoslot " + object.idInfoSlot
//                        color: qgcPal.text
//                        hoverEnabled: listViewBank.currentIndex === index
//                        enabled: listViewBank.currentIndex === index
//                        background: Rectangle {
//                            anchors.fill: parent
//                            color: "transparent"
//                            border.color: textInput.focus ? qgcPal.colorGrey : "transparent"
//                        }
                    }

                    QGCIconButton {
                        id: buttonWaypointAddDelete
                        Layout.alignment: Qt.AlignRight
                        Layout.rightMargin: _margin
                        Layout.preferredHeight: _iconSize
                        Layout.preferredWidth: _iconSize
                        visible: rect.ListView.isCurrentItem || rect.rowHovered ? true : false
                        showNormal: hovered
                        iconSource: index === 0 ? "/qmlimages/Plus.svg" : "/qmlimages/DeleteFile.svg"
                        highlighted: hovered
                        onClicked: {
                            toolTipButtonWaypointAddDelete.visible = false
                            if (index === 0) {
                                _itemCurrentWaypoint.appendInfoSlot()
                            } else if (index > 0) {
                                _itemCurrentWaypoint.removeInfoSlot(index)
                            }
//                            console.log("buttonWaypointAddDelete clicked", listViewWaypoint)
//                            console.log("button.width:", width, listViewWaypoint.count)
//                            listViewWaypoint.currentIndex = listViewWaypoint.count - 1
//                            console.log(index === 0 ? "add" : "remove" + " waypoint: ", labelWaypointId.text, "icon size:", sourceSize)
                        }
                        ToolTip {
                            id: toolTipButtonWaypointAddDelete
                            text: (index === 0 ? "add" : "remove") + " infoslot"
                            visible: parent.hovered
                            delay: toolTipDelay
                        }
                    }

                    QGCIconButton {
                        id: buttonWaypointEdit
                        Layout.alignment: Qt.AlignRight
                        Layout.rightMargin: _margin
                        Layout.preferredHeight: _iconSize
                        Layout.preferredWidth: _iconSize
                        visible: rect.ListView.isCurrentItem || rect.rowHovered ? true : false
                        showNormal: hovered
                        iconSource: "/qmlimages/Edit.svg"
                        ToolTip.text: qsTr("edit")
                        highlighted: hovered
                        onClicked: {
                            toolTipButtonWaypointEdit.visible = false
                            listViewInfoSlot.currentIndex = index
                            mainWindow.showComponentDialog(componentInfoslotEditor, qsTr("InfoSlot ") + _itemCurrentInfoSlot.idInfoSlot, _rightPanelWidth / ScreenTools.defaultFontPixelWidth, StandardButton.Close)
                        }
                        ToolTip {
                            id: toolTipButtonWaypointEdit
                            text: "edit infoslot"
                            visible: parent.hovered
                            delay: toolTipDelay
                        }
                    }
                }
            }

            Rectangle {
                height: 1
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                color: _splitLineColorLighter
            }

        }
    }

    Component {
        id: componentInfoslotEditor
            MissionItemEditor {
                id: infoSlotEditor
                itemInfoSlot: _itemCurrentInfoSlot//_itemsInfoSlot.get(listViewInfoSlot.currentIndex)
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: _rightPanelWidth
                masterController: _planMasterController
            }
    }
}
