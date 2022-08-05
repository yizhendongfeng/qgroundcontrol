/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include "StateMachine.h"
#include "QGCMAVLink.h"
#include "QGCLoggingCategory.h"
#include "Vehicle.h"

Q_DECLARE_LOGGING_CATEGORY(InitialConnectStateMachineLog)

class Vehicle;

class InitialConnectStateMachine : public StateMachine
{
public:
    InitialConnectStateMachine(Vehicle* vehicle);

    // Overrides from StateMachine
    int             stateCount      (void) const final;
    const StateFn*  rgStates        (void) const final;
    void            statesCompleted (void) const final;

private:
    static void _stateRequestParameters                 (StateMachine* stateMachine);
    static void _stateRequestMission                    (StateMachine* stateMachine);
    static void _stateRequestGeoFence                   (StateMachine* stateMachine);
    static void _stateSignalInitialConnectComplete      (StateMachine* stateMachine);

//    float _progress(float subProgress = 0.f);

    Vehicle* _vehicle;

    static const StateFn    _rgStates[];
    static const int        _cStates;
};
