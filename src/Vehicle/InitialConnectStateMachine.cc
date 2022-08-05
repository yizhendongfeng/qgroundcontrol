/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "InitialConnectStateMachine.h"
#include "Vehicle.h"
#include "QGCCorePlugin.h"
#include "QGCOptions.h"
#include "FirmwarePlugin.h"
#include "ParameterManager.h"
#include "MissionManager.h"

QGC_LOGGING_CATEGORY(InitialConnectStateMachineLog, "InitialConnectStateMachineLog")

const StateMachine::StateFn InitialConnectStateMachine::_rgStates[] = {
    InitialConnectStateMachine::_stateRequestParameters,
//    InitialConnectStateMachine::_stateRequestMission,
//    InitialConnectStateMachine::_stateRequestGeoFence,
    InitialConnectStateMachine::_stateSignalInitialConnectComplete
};

const int InitialConnectStateMachine::_cStates = sizeof(InitialConnectStateMachine::_rgStates) / sizeof(InitialConnectStateMachine::_rgStates[0]);

InitialConnectStateMachine::InitialConnectStateMachine(Vehicle* vehicle)
    : _vehicle(vehicle)
{

}

int InitialConnectStateMachine::stateCount(void) const
{
    return _cStates;
}

const InitialConnectStateMachine::StateFn* InitialConnectStateMachine::rgStates(void) const
{
    return &_rgStates[0];
}

void InitialConnectStateMachine::statesCompleted(void) const
{

}

void InitialConnectStateMachine::_stateRequestParameters(StateMachine* stateMachine)
{
    InitialConnectStateMachine* connectMachine  = static_cast<InitialConnectStateMachine*>(stateMachine);
    Vehicle*                    vehicle         = connectMachine->_vehicle;

    qCDebug(InitialConnectStateMachineLog) << "_stateRequestParameters";
    vehicle->_parameterManager->refreshGroupParameters();
}

void InitialConnectStateMachine::_stateRequestMission(StateMachine* stateMachine)
{
    InitialConnectStateMachine* connectMachine  = static_cast<InitialConnectStateMachine*>(stateMachine);
    Vehicle*                    vehicle         = connectMachine->_vehicle;
    SharedLinkInterfacePtr      sharedLink      = vehicle->vehicleLinkManager()->primaryLink().lock();

    if (!sharedLink) {
        qCDebug(InitialConnectStateMachineLog) << "_stateRequestMission: Skipping first mission load request due to no primary link";
        connectMachine->advance();
    } else {
        if (sharedLink->linkConfiguration()->isHighLatency() || sharedLink->isPX4Flow() || sharedLink->isLogReplay()) {
            qCDebug(InitialConnectStateMachineLog) << "_stateRequestMission: Skipping first mission load request due to link type";
            vehicle->_firstMissionLoadComplete();
        } else {
            qCDebug(InitialConnectStateMachineLog) << "_stateRequestMission";
            vehicle->_missionManager->loadFromVehicle();
        }
    }
}

void InitialConnectStateMachine::_stateRequestGeoFence(StateMachine* stateMachine)
{
    InitialConnectStateMachine* connectMachine  = static_cast<InitialConnectStateMachine*>(stateMachine);
    Vehicle*                    vehicle         = connectMachine->_vehicle;
    SharedLinkInterfacePtr      sharedLink      = vehicle->vehicleLinkManager()->primaryLink().lock();

    if (!sharedLink) {
        qCDebug(InitialConnectStateMachineLog) << "_stateRequestGeoFence: Skipping first geofence load request due to no primary link";
        connectMachine->advance();
    } else {
        if (sharedLink->linkConfiguration()->isHighLatency() || sharedLink->isPX4Flow() || sharedLink->isLogReplay()) {
            qCDebug(InitialConnectStateMachineLog) << "_stateRequestGeoFence: Skipping first geofence load request due to link type";
            vehicle->_firstGeoFenceLoadComplete();
        } else {
//            if (vehicle->_geoFenceManager->supported()) {
//                qCDebug(InitialConnectStateMachineLog) << "_stateRequestGeoFence";
//                vehicle->_geoFenceManager->loadFromVehicle();
//            } else {
//                qCDebug(InitialConnectStateMachineLog) << "_stateRequestGeoFence: skipped due to no support";
//                vehicle->_firstGeoFenceLoadComplete();
//            }
        }
    }
}

void InitialConnectStateMachine::_stateSignalInitialConnectComplete(StateMachine* stateMachine)
{
    InitialConnectStateMachine* connectMachine  = static_cast<InitialConnectStateMachine*>(stateMachine);
    Vehicle*                    vehicle         = connectMachine->_vehicle;

    qCDebug(InitialConnectStateMachineLog) << "Signalling initialConnectComplete";
    emit vehicle->initialConnectComplete();
}

//float InitialConnectStateMachine::_progress(float subProgress)
//{
//    int progressWeight = 0;
//    for (int i = 0; i < _stateIndex && i < _cStates; ++i) {
//        progressWeight += _rgProgressWeights[i];
//    }
//    int currentWeight = _stateIndex < _cStates ? _rgProgressWeights[_stateIndex] : 1;
//    return (progressWeight + currentWeight * subProgress) / (float)_progressWeightTotal;
//}

