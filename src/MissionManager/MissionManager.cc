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
#include "QGCApplication.h"
//#include "MissionCommandTree.h"
//#include "MissionCommandUIInfo.h"

QGC_LOGGING_CATEGORY(MissionManagerLog, "MissionManagerLog")

MissionManager::MissionManager(Vehicle* vehicle)
    : PlanManager               (vehicle, SHENHANG_MISSION_TYPE_MISSION)
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

