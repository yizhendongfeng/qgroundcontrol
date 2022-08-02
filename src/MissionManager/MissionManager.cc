/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "MissionManager.h"
#include "Vehicle.h"
#include "FirmwarePlugin.h"
#include "MAVLinkProtocol.h"
#include "QGCApplication.h"
#include "MissionCommandTree.h"
#include "MissionCommandUIInfo.h"

QGC_LOGGING_CATEGORY(MissionManagerLog, "MissionManagerLog")

MissionManager::MissionManager(Vehicle* vehicle)
    : PlanManager               (vehicle, MAV_MISSION_TYPE_MISSION)
    , _cachedLastCurrentIndex   (-1)
{
    connect(_vehicle, &Vehicle::shenHangMessageReceived, this, &MissionManager::_shenHangMessageReceived);
}

MissionManager::~MissionManager()
{

}

void MissionManager::_shenHangMessageReceived(const ShenHangProtocolMessage& message)
{
//    qDebug() << "MissionManager::_shenHangMessageReceived" << message.tyMsg0 << message.tyMsg1;
}

void MissionManager::_updateMissionIndex(int index)
{
    if (index != _currentMissionIndex) {
        qCDebug(MissionManagerLog) << "_updateMissionIndex currentIndex:" << index;
        _currentMissionIndex = index;
        emit currentIndexChanged(_currentMissionIndex);
    }

    if (_currentMissionIndex != _lastCurrentIndex && _cachedLastCurrentIndex != _currentMissionIndex) {
        // We have to be careful of an RTL sequence causing a change of index to the DO_LAND_START sequence. This also triggers
        // a flight mode change away from mission flight mode. So we only update _lastCurrentIndex when the flight mode is mission.
        // But we can run into problems where we may get the MISSION_CURRENT message for the RTL/DO_LAND_START sequenc change prior
        // to the HEARTBEAT message which contains the flight mode change which will cause things to work incorrectly. To fix this
        // We force the sequencing of HEARTBEAT following by MISSION_CURRENT by caching the possible _lastCurrentIndex update until
        // the next HEARTBEAT comes through.
        qCDebug(MissionManagerLog) << "_updateMissionIndex caching _lastCurrentIndex for possible update:" << _currentMissionIndex;
        _cachedLastCurrentIndex = _currentMissionIndex;
    }
}

void MissionManager::_handleHighLatency(const mavlink_message_t& message) 
{
    mavlink_high_latency_t highLatency;
    mavlink_msg_high_latency_decode(&message, &highLatency);
    _updateMissionIndex(highLatency.wp_num);
}

void MissionManager::_handleHighLatency2(const mavlink_message_t& message) 
{
    mavlink_high_latency2_t highLatency2;
    mavlink_msg_high_latency2_decode(&message, &highLatency2);
    _updateMissionIndex(highLatency2.wp_num);
}

void MissionManager::_handleMissionCurrent(const mavlink_message_t& message)
{
    mavlink_mission_current_t missionCurrent;
    mavlink_msg_mission_current_decode(&message, &missionCurrent);
    _updateMissionIndex(missionCurrent.seq);
}

void MissionManager::_handleHeartbeat(const mavlink_message_t& message)
{
    Q_UNUSED(message);

    if (_cachedLastCurrentIndex != -1 &&  _vehicle->flightMode() == _vehicle->missionFlightMode()) {
        qCDebug(MissionManagerLog) << "_handleHeartbeat updating lastCurrentIndex from cached value:" << _cachedLastCurrentIndex;
        _lastCurrentIndex = _cachedLastCurrentIndex;
        _cachedLastCurrentIndex = -1;
        emit lastCurrentIndexChanged(_lastCurrentIndex);
    }
}

