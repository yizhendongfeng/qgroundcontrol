/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/


import QtQuick      2.3
import QtLocation   5.3

import QGroundControl.ScreenTools   1.0
import QGroundControl.Controls      1.0
import QGroundControl.Vehicle       1.0

/// Marker for displaying a mission item on the map
MapQuickItem {
    id: _item

    property var itemWaypoint
    signal clicked

    anchorPoint.x:  sourceItem.anchorPointX
    anchorPoint.y:  sourceItem.anchorPointY

    sourceItem:
        MissionItemIndexLabel {
            id:                 _label
            checked:            _isCurrentWaypoint && _modePlanEdit
//            label:              itemWaypoint.abbreviation
            index:              itemWaypoint.indexWaypoint//itemWaypoint.abbreviation.charAt(0) > 'A' && itemWaypoint.abbreviation.charAt(0) < 'z' ? -1 : itemWaypoint.sequenceNumber
//            gimbalYaw:          itemWaypoint.missionGimbalYaw
//            vehicleYaw:         itemWaypoint.missionVehicleYaw
//            showGimbalYaw:      !isNaN(itemWaypoint.missionGimbalYaw)
            highlightSelected:  true
            onClicked:          _item.clicked()
            opacity:            _item.opacity

            property bool _isCurrentWaypoint:   itemWaypoint ? itemWaypoint.isCurrentWaypoint : false
        }
}
