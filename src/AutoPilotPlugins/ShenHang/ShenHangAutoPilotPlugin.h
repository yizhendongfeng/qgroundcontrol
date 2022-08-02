/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/


#ifndef ShenHangAUTOPILOT_H
#define ShenHangAUTOPILOT_H

#include "AutoPilotPlugin.h"
//#include "ShenHangAirframeLoader.h"
//#include "ShenHangAirframeComponent.h"
//#include "ShenHangRadioComponent.h"
//#include "ShenHangFlightModesComponent.h"
//#include "ShenHangCameraComponent.h"
#include "Vehicle.h"

#include <QImage>

/// @file
///     @brief This is the ShenHang specific implementation of the AutoPilot class.
///     @author Don Gagne <don@thegagnes.com>

class ShenHangAutoPilotPlugin : public AutoPilotPlugin
{
    Q_OBJECT

public:
    ShenHangAutoPilotPlugin(Vehicle* vehicle, QObject* parent);
    ~ShenHangAutoPilotPlugin();

    // Overrides from AutoPilotPlugin
    const QVariantList& vehicleComponents(void) override;
    void parametersReadyPreChecks(void) override;
    QString prerequisiteSetup(VehicleComponent* component) const override;

protected:
    bool                    _incorrectParameterVersion; ///< true: parameter version incorrect, setup not allowed
//    ShenHangAirframeLoader*      _airframeFacts;
//    ShenHangAirframeComponent*      _airframeComponent;
//    ShenHangRadioComponent*      _radioComponent;
//    ShenHangFlightModesComponent*   _flightModesComponent;
//    ShenHangCameraComponent*        _cameraComponent;

private:
    QVariantList            _components;
};

#endif
