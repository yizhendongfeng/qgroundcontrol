/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/


#include "ShenHangRadioComponent.h"
#include "ShenHangAutoPilotPlugin.h"

ShenHangRadioComponent::ShenHangRadioComponent(Vehicle* vehicle, AutoPilotPlugin* autopilot, QObject* parent) :
    VehicleComponent(vehicle, autopilot, parent),
    _name(tr("Radio"))
{
}

QString ShenHangRadioComponent::name(void) const
{
    return _name;
}

QString ShenHangRadioComponent::description(void) const
{
    return tr("Radio Setup is used to calibrate your transmitter. "
              "It also assign channels for Roll, Pitch, Yaw and Throttle vehicle control as well as determining whether they are reversed.");
}

QString ShenHangRadioComponent::iconResource(void) const
{
    return "/qmlimages/RadioComponentIcon.png";
}

bool ShenHangRadioComponent::requiresSetup(void) const
{
    return _vehicle->parameterManager()->getParameter(-1, "COM_RC_IN_MODE")->rawValue().toInt() == 1 ? false : true;
}

bool ShenHangRadioComponent::setupComplete(void) const
{
    if (_vehicle->parameterManager()->getParameter(-1, "COM_RC_IN_MODE")->rawValue().toInt() != 1) {
        // The best we can do to detect the need for a radio calibration is look for attitude
        // controls to be mapped.
        for(const QString &mapParam : {"RC_MAP_ROLL", "RC_MAP_PITCH", "RC_MAP_YAW", "RC_MAP_THROTTLE"}) {
            if (_vehicle->parameterManager()->getParameter(FactSystem::defaultComponentId, mapParam)->rawValue().toInt() == 0) {
                return false;
            }
        }
    }

    return true;
}

QStringList ShenHangRadioComponent::setupCompleteChangedTriggerList(void) const
{
    return {"COM_RC_IN_MODE", "RC_MAP_ROLL", "RC_MAP_PITCH", "RC_MAP_YAW", "RC_MAP_THROTTLE"};
}

QUrl ShenHangRadioComponent::setupSource(void) const
{
    return QUrl::fromUserInput("qrc:/qml/RadioComponent.qml");
}

QUrl ShenHangRadioComponent::summaryQmlSource(void) const
{
    return QUrl::fromUserInput("qrc:/qml/ShenHangRadioComponentSummary.qml");
}
