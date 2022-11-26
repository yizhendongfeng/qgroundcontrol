﻿/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "FactPanelController.h"
#include "MultiVehicleManager.h"
#include "QGCApplication.h"
#include "ParameterManager.h"

#include <QQmlEngine>

/// @file
///     @author Don Gagne <don@thegagnes.com>

QGC_LOGGING_CATEGORY(FactPanelControllerLog, "FactPanelControllerLog")

FactPanelController::FactPanelController()
{
    _vehicle = qgcApp()->toolbox()->multiVehicleManager()->activeVehicle();
    if (_vehicle) {
        _autopilot = _vehicle->autopilotPlugin();
    } else {
        _vehicle = qgcApp()->toolbox()->multiVehicleManager()->offlineEditingVehicle();
    }

    _missingParametersTimer.setInterval(500);
    _missingParametersTimer.setSingleShot(true);
    connect(&_missingParametersTimer, &QTimer::timeout, this, &FactPanelController::_checkForMissingParameters);
}

void FactPanelController::_reportMissingParameter(const QString& name)
{
    qgcApp()->reportMissingParameter(name);
    qCWarning(FactPanelControllerLog) << "Missing parameter:" << name;
}

bool FactPanelController::_allParametersExists(int componentId, QStringList names)
{
//    bool noMissingFacts = true;

//    foreach (const QString &name, names) {
//        if (_vehicle && !_vehicle->parameterManager()->parameterExists(name)) {
//            _reportMissingParameter(name);
//            noMissingFacts = false;
//        }
//    }

//    return noMissingFacts;
    return false;
}


Fact* FactPanelController::getParameterFact(int componentId, const QString& name, bool reportMissing)
{
//    if (_vehicle && _vehicle->parameterManager()->parameterExists(name)) {
//        Fact* fact = _vehicle->parameterManager()->getParameter(name);
//        QQmlEngine::setObjectOwnership(fact, QQmlEngine::CppOwnership);
//        return fact;
//    } else {
//        if (reportMissing) {
//            _reportMissingParameter(name);
//        }
//        return nullptr;
//    }
    return nullptr;
}

bool FactPanelController::parameterExists(int componentId, const QString& name)
{
//    return _vehicle ? _vehicle->parameterManager()->parameterExists(name) : false;
    return false;
}

void FactPanelController::getMissingParameters(QStringList rgNames)
{
    for (const QString& name: rgNames) {
        _missingParameterWaitList.append(name);
//        _vehicle->parameterManager()->refreshParameter(MAV_COMP_ID_AUTOPILOT1, name);
    }

    _missingParametersTimer.start();
}

void FactPanelController::_checkForMissingParameters(void)
{
    QStringList waitList = _missingParameterWaitList;
//    for (const QString& name: waitList) {
//        if (_vehicle->parameterManager()->parameterExists(MAV_COMP_ID_AUTOPILOT1, name)) {
//            _missingParameterWaitList.removeOne(name);
//        }
//    }

    if (_missingParameterWaitList.isEmpty()) {
        emit missingParametersAvailable();
    } else {
        _missingParametersTimer.start();
    }
}
