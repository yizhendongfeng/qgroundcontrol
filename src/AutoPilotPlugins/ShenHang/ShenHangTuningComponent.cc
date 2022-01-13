/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/


#include "ShenHangTuningComponent.h"
#include "ShenHangAutoPilotPlugin.h"
#include "AirframeComponent.h"

static bool isCopter(MAV_TYPE type) {
    switch (type) {
        case MAV_TYPE_QUADROTOR:
        case MAV_TYPE_COAXIAL:
        case MAV_TYPE_HELICOPTER:
        case MAV_TYPE_HEXAROTOR:
        case MAV_TYPE_OCTOROTOR:
        case MAV_TYPE_TRICOPTER:
            return true;
        default:
            break;
    }
    return false;
}

ShenHangTuningComponent::ShenHangTuningComponent(Vehicle* vehicle, AutoPilotPlugin* autopilot, QObject* parent)
    : VehicleComponent(vehicle, autopilot, parent)
    , _name(isCopter(vehicle->vehicleType()) ? tr("PID Tuning") : tr("Tuning"))
{
}

QString ShenHangTuningComponent::name(void) const
{
    return _name;
}

QString ShenHangTuningComponent::description(void) const
{
    return tr("Tuning Setup is used to tune the flight characteristics of the Vehicle.");
}

QString ShenHangTuningComponent::iconResource(void) const
{
    return "/qmlimages/TuningComponentIcon.png";
}

bool ShenHangTuningComponent::requiresSetup(void) const
{
    return false;
}

bool ShenHangTuningComponent::setupComplete(void) const
{
    return true;
}

QStringList ShenHangTuningComponent::setupCompleteChangedTriggerList(void) const
{
    return QStringList();
}

QUrl ShenHangTuningComponent::setupSource(void) const
{
    QString qmlFile;

    switch (_vehicle->vehicleType()) {
        case MAV_TYPE_FIXED_WING:
            qmlFile = "qrc:/qml/ShenHangTuningComponentPlane.qml";
            break;
        case MAV_TYPE_QUADROTOR:
        case MAV_TYPE_COAXIAL:
        case MAV_TYPE_HELICOPTER:
        case MAV_TYPE_HEXAROTOR:
        case MAV_TYPE_OCTOROTOR:
        case MAV_TYPE_TRICOPTER:
            qmlFile = "qrc:/qml/ShenHangTuningComponentCopter.qml";
            break;
        case MAV_TYPE_VTOL_DUOROTOR:
        case MAV_TYPE_VTOL_QUADROTOR:
        case MAV_TYPE_VTOL_TILTROTOR:
        case MAV_TYPE_VTOL_RESERVED2:
        case MAV_TYPE_VTOL_RESERVED3:
        case MAV_TYPE_VTOL_RESERVED4:
        case MAV_TYPE_VTOL_RESERVED5:
            qmlFile = "qrc:/qml/ShenHangTuningComponentVTOL.qml";
            break;
        default:
            break;
    }

    return QUrl::fromUserInput(qmlFile);
}

QUrl ShenHangTuningComponent::summaryQmlSource(void) const
{
    return QUrl();
}
