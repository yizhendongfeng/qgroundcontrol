/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/


#include "ShenHangAutoPilotPlugin.h"
//#include "ShenHangAirframeLoader.h"
//#include "ShenHangAdvancedFlightModesController.h"
//#include "ShenHangAirframeComponentController.h"
#include "FirmwarePlugin/ShenHang/ShenHangParameterMetaData.h"  // FIXME: Hack
#include "FirmwarePlugin/ShenHang/ShenHangFirmwarePlugin.h"  // FIXME: Hack
#include "QGCApplication.h"
//#include "ShenHangFlightModesComponent.h"

/// @file
///     @brief This is the AutoPilotPlugin implementatin for the MAV_AUTOPILOT_ShenHang type.
///     @author Don Gagne <don@thegagnes.com>

ShenHangAutoPilotPlugin::ShenHangAutoPilotPlugin(Vehicle* vehicle, QObject* parent)
    : AutoPilotPlugin(vehicle, parent)
    , _incorrectParameterVersion(false)
//    , _airframeComponent(nullptr)
//    , _radioComponent(nullptr)
//    , _flightModesComponent(nullptr)
{
    if (!vehicle) {
        qWarning() << "Internal error";
        return;
    }

//    _airframeFacts = new ShenHangAirframeLoader(this, this);
//    Q_CHECK_PTR(_airframeFacts);

//    ShenHangAirframeLoader::loadAirframeMetaData();
}

ShenHangAutoPilotPlugin::~ShenHangAutoPilotPlugin()
{
//    delete _airframeFacts;
}

const QVariantList& ShenHangAutoPilotPlugin::vehicleComponents(void)
{
    if (_components.count() == 0 && !_incorrectParameterVersion) {
        if (_vehicle) {
            if (_vehicle->parameterManager()->parametersReady()) {
//                _airframeComponent = new AirframeComponent(_vehicle, this);
//                _airframeComponent->setupTriggerSignals();
//                _components.append(QVariant::fromValue((VehicleComponent*)_airframeComponent));

//                _sensorsComponent = new SensorsComponent(_vehicle, this);
//                _sensorsComponent->setupTriggerSignals();
//                _components.append(QVariant::fromValue((VehicleComponent*)_sensorsComponent));

//                _radioComponent = new ShenHangRadioComponent(_vehicle, this);
//                _radioComponent->setupTriggerSignals();
//                _components.append(QVariant::fromValue((VehicleComponent*)_radioComponent));

//                _flightModesComponent = new FlightModesComponent(_vehicle, this);
//                _flightModesComponent->setupTriggerSignals();
//                _components.append(QVariant::fromValue((VehicleComponent*)_flightModesComponent));

//                _powerComponent = new PowerComponent(_vehicle, this);
//                _powerComponent->setupTriggerSignals();
//                _components.append(QVariant::fromValue((VehicleComponent*)_powerComponent));

//                _motorComponent = new MotorComponent(_vehicle, this);
//                _motorComponent->setupTriggerSignals();
//                _components.append(QVariant::fromValue((VehicleComponent*)_motorComponent));

//                _safetyComponent = new SafetyComponent(_vehicle, this);
//                _safetyComponent->setupTriggerSignals();
//                _components.append(QVariant::fromValue((VehicleComponent*)_safetyComponent));

//                _tuningComponent = new ShenHangTuningComponent(_vehicle, this);
//                _tuningComponent->setupTriggerSignals();
//                _components.append(QVariant::fromValue((VehicleComponent*)_tuningComponent));

                //-- Is there support for cameras?
//                if(_vehicle->parameterManager()->parameterExists(_vehicle->id(), "TRIG_MODE")) {
//                    _cameraComponent = new ShenHangCameraComponent(_vehicle, this);
//                    _cameraComponent->setupTriggerSignals();
//                    _components.append(QVariant::fromValue((VehicleComponent*)_cameraComponent));
//                }
            } else {
                qWarning() << "Call to vehicleCompenents prior to parametersReady";
            }

        } else {
            qWarning() << "Internal error";
        }
    }

    return _components;
}

void ShenHangAutoPilotPlugin::parametersReadyPreChecks(void)
{
    // Base class must be called
    AutoPilotPlugin::parametersReadyPreChecks();

    QString hitlParam("SYS_HITL");
    if (_vehicle->parameterManager()->parameterExists(hitlParam) &&
            _vehicle->parameterManager()->getParameter(hitlParam)->rawValue().toBool()) {
        qgcApp()->showAppMessage(tr("Warning: Hardware In The Loop (HITL) simulation is enabled for this vehicle."));
    }
}

QString ShenHangAutoPilotPlugin::prerequisiteSetup(VehicleComponent* component) const
{
//    bool requiresAirframeCheck = false;

//    if (qobject_cast<const ShenHangFlightModesComponent*>(component)) {
//        if (_vehicle->parameterManager()->getParameter(-1, "COM_RC_IN_MODE")->rawValue().toInt() == 1) {
//            // No RC input
//            return QString();
//        } else {
//            if (_airframeComponent && !_airframeComponent->setupComplete()) {
//                return _airframeComponent->name();
//            } else if (_radioComponent && !_radioComponent->setupComplete()) {
//                return _radioComponent->name();
//            }
//        }
//    }

//    if (requiresAirframeCheck) {
//        if (_airframeComponent && !_airframeComponent->setupComplete()) {
//            return _airframeComponent->name();
//        }
//    }

    return QString();
}
