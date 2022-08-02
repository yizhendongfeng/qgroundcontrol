/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "ShenHangFirmwarePlugin.h"
#include "ShenHangParameterMetaData.h"
#include "QGCApplication.h"
#include "ShenHangAutoPilotPlugin.h"
//#include "ShenHangAdvancedFlightModesController.h"
//#include "ShenHangSimpleFlightModesController.h"
//#include "ShenHangAirframeComponentController.h"
//#include "ShenHangRadioComponentController.h"
#include "QGCCameraManager.h"
#include "QGCFileDownload.h"
#include "SettingsManager.h"
#include "PlanViewSettings.h"

#include <QDebug>

#include "ShenHang_custom_mode.h"

ShenHangFirmwarePluginInstanceData::ShenHangFirmwarePluginInstanceData(QObject* parent)
    : QObject(parent)
    , versionNotified(false)
{

}

ShenHangFirmwarePlugin::ShenHangFirmwarePlugin()
    : _manualFlightMode     (tr("Manual"))
    , _acroFlightMode       (tr("Acro"))
    , _stabilizedFlightMode (tr("Stabilized"))
    , _rattitudeFlightMode  (tr("Rattitude"))
    , _altCtlFlightMode     (tr("Altitude"))
    , _posCtlFlightMode     (tr("Position"))
    , _offboardFlightMode   (tr("Offboard"))
    , _readyFlightMode      (tr("Ready"))
    , _takeoffFlightMode    (tr("Takeoff"))
    , _holdFlightMode       (tr("Hold"))
    , _missionFlightMode    (tr("Mission"))
    , _rtlFlightMode        (tr("Return"))
    , _landingFlightMode    (tr("Land"))
    , _preclandFlightMode   (tr("Precision Land"))
    , _rtgsFlightMode       (tr("Return to Groundstation"))
    , _simpleFlightMode     (tr("Simple"))
    , _orbitFlightMode      (tr("Orbit"))
{
//    qmlRegisterType<ShenHangAdvancedFlightModesController>   ("QGroundControl.Controllers", 1, 0, "ShenHangAdvancedFlightModesController");
//    qmlRegisterType<ShenHangSimpleFlightModesController>     ("QGroundControl.Controllers", 1, 0, "ShenHangSimpleFlightModesController");
//    qmlRegisterType<ShenHangAirframeComponentController>        ("QGroundControl.Controllers", 1, 0, "AirframeComponentController");

    struct Modes2Name {
        uint8_t     main_mode;
        uint8_t     sub_mode;
        bool        canBeSet;   ///< true: Vehicle can be set to this flight mode
        bool        fixedWing;  /// fixed wing compatible
        bool        multiRotor;  /// multi rotor compatible
    };

    static const struct Modes2Name rgModes2Name[] = {
        //main_mode                         sub_mode                                canBeSet  FW      MC
        { ShenHang_CUSTOM_MAIN_MODE_MANUAL,      0,                                      true,   true,   true },
        { ShenHang_CUSTOM_MAIN_MODE_STABILIZED,  0,                                      true,   true,   true },
        { ShenHang_CUSTOM_MAIN_MODE_ACRO,        0,                                      true,   true,   true },
        { ShenHang_CUSTOM_MAIN_MODE_RATTITUDE,   0,                                      true,   true,   true },
        { ShenHang_CUSTOM_MAIN_MODE_ALTCTL,      0,                                      true,   true,   true },
        { ShenHang_CUSTOM_MAIN_MODE_OFFBOARD,    0,                                      true,   false,  true },
        { ShenHang_CUSTOM_MAIN_MODE_SIMPLE,      0,                                      false,  false,  true },
        { ShenHang_CUSTOM_MAIN_MODE_POSCTL,      ShenHang_CUSTOM_SUB_MODE_POSCTL_POSCTL,      true,   true,   true },
        { ShenHang_CUSTOM_MAIN_MODE_POSCTL,      ShenHang_CUSTOM_SUB_MODE_POSCTL_ORBIT,       false,  false,   false },
        { ShenHang_CUSTOM_MAIN_MODE_AUTO,        ShenHang_CUSTOM_SUB_MODE_AUTO_LOITER,        true,   true,   true },
        { ShenHang_CUSTOM_MAIN_MODE_AUTO,        ShenHang_CUSTOM_SUB_MODE_AUTO_MISSION,       true,   true,   true },
        { ShenHang_CUSTOM_MAIN_MODE_AUTO,        ShenHang_CUSTOM_SUB_MODE_AUTO_RTL,           true,   true,   true },
        { ShenHang_CUSTOM_MAIN_MODE_AUTO,        ShenHang_CUSTOM_SUB_MODE_AUTO_FOLLOW_TARGET, true,   false,  true },
        { ShenHang_CUSTOM_MAIN_MODE_AUTO,        ShenHang_CUSTOM_SUB_MODE_AUTO_LAND,          false,  true,   true },
        { ShenHang_CUSTOM_MAIN_MODE_AUTO,        ShenHang_CUSTOM_SUB_MODE_AUTO_PRECLAND,      false,  false,  true },
        { ShenHang_CUSTOM_MAIN_MODE_AUTO,        ShenHang_CUSTOM_SUB_MODE_AUTO_READY,         false,  true,   true },
        { ShenHang_CUSTOM_MAIN_MODE_AUTO,        ShenHang_CUSTOM_SUB_MODE_AUTO_RTGS,          false,  true,   true },
        { ShenHang_CUSTOM_MAIN_MODE_AUTO,        ShenHang_CUSTOM_SUB_MODE_AUTO_TAKEOFF,       false,  true,   true },
    };

    // Must be in same order as above structure
    const QString* rgModeNames[] = {
        &_manualFlightMode,
        &_stabilizedFlightMode,
        &_acroFlightMode,
        &_rattitudeFlightMode,
        &_altCtlFlightMode,
        &_offboardFlightMode,
        &_simpleFlightMode,
        &_posCtlFlightMode,
        &_orbitFlightMode,
        &_holdFlightMode,
        &_missionFlightMode,
        &_rtlFlightMode,
        &_landingFlightMode,
        &_preclandFlightMode,
        &_readyFlightMode,
        &_rtgsFlightMode,
        &_takeoffFlightMode,
    };

    // Convert static information to dynamic list. This allows for plugin override class to manipulate list.
    for (size_t i=0; i<sizeof(rgModes2Name)/sizeof(rgModes2Name[0]); i++) {
        const struct Modes2Name* pModes2Name = &rgModes2Name[i];

        FlightModeInfo_t info;

        info.main_mode =    pModes2Name->main_mode;
        info.sub_mode =     pModes2Name->sub_mode;
        info.name =         rgModeNames[i];
        info.canBeSet =     pModes2Name->canBeSet;
        info.fixedWing =    pModes2Name->fixedWing;
        info.multiRotor =   pModes2Name->multiRotor;

        _flightModeInfoList.append(info);
    }
}

ShenHangFirmwarePlugin::~ShenHangFirmwarePlugin()
{
}

AutoPilotPlugin* ShenHangFirmwarePlugin::autopilotPlugin(Vehicle* vehicle)
{
    return new ShenHangAutoPilotPlugin(vehicle, vehicle);
}

QList<VehicleComponent*> ShenHangFirmwarePlugin::componentsForVehicle(AutoPilotPlugin* vehicle)
{
    Q_UNUSED(vehicle);

    return QList<VehicleComponent*>();
}

QStringList ShenHangFirmwarePlugin::flightModes(Vehicle* vehicle)
{
    QStringList flightModes;

    foreach (const FlightModeInfo_t& info, _flightModeInfoList) {
        if (info.canBeSet) {
            bool fw = (vehicle->fixedWing() && info.fixedWing);
            bool mc = (vehicle->multiRotor() && info.multiRotor);

            // show all modes for generic, vtol, etc
            bool other = !vehicle->fixedWing() && !vehicle->multiRotor();

            if (fw || mc || other) {
                flightModes += *info.name;
            }
        }
    }

    return flightModes;
}

QString ShenHangFirmwarePlugin::flightMode(uint8_t base_mode, uint32_t custom_mode) const
{
    QString flightMode = "Unknown";

    if (base_mode & MAV_MODE_FLAG_CUSTOM_MODE_ENABLED) {
        union px4_custom_mode px4_mode;
        px4_mode.data = custom_mode;

        bool found = false;
        foreach (const FlightModeInfo_t& info, _flightModeInfoList) {
            if (info.main_mode == px4_mode.main_mode && info.sub_mode == px4_mode.sub_mode) {
                flightMode = *info.name;
                found = true;
                break;
            }
        }

        if (!found) {
            qWarning() << "Unknown flight mode" << custom_mode;
            return tr("Unknown %1:%2").arg(base_mode).arg(custom_mode);
        }
    }

    return flightMode;
}

bool ShenHangFirmwarePlugin::setFlightMode(const QString& flightMode, uint8_t* base_mode, uint32_t* custom_mode)
{
    *base_mode = 0;
    *custom_mode = 0;

    bool found = false;
    foreach (const FlightModeInfo_t& info, _flightModeInfoList) {
        if (flightMode.compare(info.name, Qt::CaseInsensitive) == 0) {
            union px4_custom_mode px4_mode;

            px4_mode.data = 0;
            px4_mode.main_mode = info.main_mode;
            px4_mode.sub_mode = info.sub_mode;

            *base_mode = MAV_MODE_FLAG_CUSTOM_MODE_ENABLED;
            *custom_mode = px4_mode.data;

            found = true;
            break;
        }
    }

    if (!found) {
        qWarning() << "Unknown flight Mode" << flightMode;
    }

    return found;
}

bool ShenHangFirmwarePlugin::isCapable(const Vehicle *vehicle, FirmwareCapabilities capabilities)
{
    int available = SetFlightModeCapability | PauseVehicleCapability | GuidedModeCapability;
    //-- This is arbitrary until I find how to really tell if ROI is avaiable
    if (vehicle->multiRotor()) {
        available |= ROIModeCapability;
    }
    if (vehicle->multiRotor() || vehicle->vtol()) {
        available |= TakeoffVehicleCapability | OrbitModeCapability;
    }
    return (capabilities & available) == capabilities;
}

void ShenHangFirmwarePlugin::initializeVehicle(Vehicle* vehicle)
{
    vehicle->setFirmwarePluginInstanceData(new ShenHangFirmwarePluginInstanceData);
}

bool ShenHangFirmwarePlugin::sendHomePositionToVehicle(void)
{
    // ShenHang stack does not want home position sent in the first position.
    // Subsequent sequence numbers must be adjusted.
    return false;
}

FactMetaData* ShenHangFirmwarePlugin::_getMetaDataForFact(QObject* parameterMetaData, const QString& name, FactMetaData::ValueType_t type, MAV_TYPE vehicleType)
{
    ShenHangParameterMetaData* shenHangMetaData = qobject_cast<ShenHangParameterMetaData*>(parameterMetaData);

    if (shenHangMetaData) {
        return shenHangMetaData->getMetaDataForFact(1, 1);      //默认改为第一组第一个参数
    } else {
        qWarning() << "Internal error: pointer passed to ShenHangFirmwarePlugin::getMetaDataForFact not ShenHangParameterMetaData";
    }

    return nullptr;
}

FactMetaData* ShenHangFirmwarePlugin::_getMetaDataForFactByIndexes(QObject* parameterMetaData, uint8_t idGroup, uint16_t addrOffset, MAV_TYPE vehicleType)
{
    ShenHangParameterMetaData* shenHangMetaData = qobject_cast<ShenHangParameterMetaData*>(parameterMetaData);

    if (shenHangMetaData) {
        return shenHangMetaData->getMetaDataForFact(idGroup, addrOffset);
    } else {
        qWarning() << "Internal error: pointer passed to ShenHangFirmwarePlugin::getMetaDataForFact not ShenHangParameterMetaData";
    }

    return nullptr;
}

void ShenHangFirmwarePlugin::_getParameterMetaDataVersionInfo(const QString& metaDataFile, int& majorVersion, int& minorVersion)
{
    return ShenHangParameterMetaData::getParameterMetaDataVersionInfo(metaDataFile, majorVersion, minorVersion);
}

QList<MAV_CMD> ShenHangFirmwarePlugin::supportedMissionCommands(QGCMAVLink::VehicleClass_t vehicleClass)
{
    QList<MAV_CMD> supportedCommands = {
        MAV_CMD_NAV_WAYPOINT,
        MAV_CMD_NAV_LOITER_UNLIM, MAV_CMD_NAV_LOITER_TIME,
        MAV_CMD_NAV_RETURN_TO_LAUNCH,
        MAV_CMD_DO_JUMP,
        MAV_CMD_DO_DIGICAM_CONTROL,
        MAV_CMD_DO_SET_CAM_TRIGG_DIST,
        MAV_CMD_DO_SET_SERVO,
        MAV_CMD_DO_CHANGE_SPEED,
        MAV_CMD_DO_LAND_START,
        MAV_CMD_DO_SET_ROI_LOCATION, MAV_CMD_DO_SET_ROI_WPNEXT_OFFSET, MAV_CMD_DO_SET_ROI_NONE,
        MAV_CMD_DO_MOUNT_CONFIGURE,
        MAV_CMD_DO_MOUNT_CONTROL,
        MAV_CMD_SET_CAMERA_MODE,
        MAV_CMD_IMAGE_START_CAPTURE, MAV_CMD_IMAGE_STOP_CAPTURE, MAV_CMD_VIDEO_START_CAPTURE, MAV_CMD_VIDEO_STOP_CAPTURE,
        MAV_CMD_NAV_DELAY,
        MAV_CMD_CONDITION_YAW,
        MAV_CMD_NAV_LOITER_TO_ALT,
    };

    QList<MAV_CMD> vtolCommands = {
        MAV_CMD_DO_VTOL_TRANSITION, MAV_CMD_NAV_VTOL_TAKEOFF, MAV_CMD_NAV_VTOL_LAND,
    };

    QList<MAV_CMD> flightCommands = {
        MAV_CMD_NAV_LAND, MAV_CMD_NAV_TAKEOFF,
    };

    if (vehicleClass == QGCMAVLink::VehicleClassGeneric) {
        supportedCommands   += vtolCommands;
        supportedCommands   += flightCommands;
    }
    if (vehicleClass == QGCMAVLink::VehicleClassVTOL) {
        supportedCommands += vtolCommands;
        supportedCommands += flightCommands;
    } else if (vehicleClass == QGCMAVLink::VehicleClassFixedWing || vehicleClass == QGCMAVLink::VehicleClassMultiRotor) {
        supportedCommands += flightCommands;
    }

    if (qgcApp()->toolbox()->settingsManager()->planViewSettings()->useConditionGate()->rawValue().toBool()) {
        supportedCommands.append(MAV_CMD_CONDITION_GATE);
    }

    return supportedCommands;
}

QString ShenHangFirmwarePlugin::missionCommandOverrides(QGCMAVLink::VehicleClass_t vehicleClass) const
{
    switch (vehicleClass) {
    case QGCMAVLink::VehicleClassGeneric:
        return QStringLiteral(":/json/ShenHang-MavCmdInfoCommon.json");
    case QGCMAVLink::VehicleClassFixedWing:
        return QStringLiteral(":/json/ShenHang-MavCmdInfoFixedWing.json");
    case QGCMAVLink::VehicleClassMultiRotor:
        return QStringLiteral(":/json/ShenHang-MavCmdInfoMultiRotor.json");
    case QGCMAVLink::VehicleClassVTOL:
        return QStringLiteral(":/json/ShenHang-MavCmdInfoVTOL.json");
    case QGCMAVLink::VehicleClassSub:
        return QStringLiteral(":/json/ShenHang-MavCmdInfoSub.json");
    case QGCMAVLink::VehicleClassRoverBoat:
        return QStringLiteral(":/json/ShenHang-MavCmdInfoRover.json");
    default:
        qWarning() << "ShenHangFirmwarePlugin::missionCommandOverrides called with bad VehicleClass_t:" << vehicleClass;
        return QString();
    }
}

QObject* ShenHangFirmwarePlugin::_loadParameterMetaData(const QString& metaDataFile)
{
    ShenHangParameterMetaData* metaData = new ShenHangParameterMetaData;
    if (!metaDataFile.isEmpty()) {
        metaData->loadParameterFactMetaDataFile(metaDataFile);
    }
    return metaData;
}

void ShenHangFirmwarePlugin::pauseVehicle(Vehicle* vehicle)
{

}

void ShenHangFirmwarePlugin::guidedModeRTL(Vehicle* vehicle, bool smartRTL)
{
    Q_UNUSED(smartRTL);
    _setFlightModeAndValidate(vehicle, _rtlFlightMode);
}

void ShenHangFirmwarePlugin::guidedModeLand(Vehicle* vehicle)
{
    _setFlightModeAndValidate(vehicle, _landingFlightMode);
}

void ShenHangFirmwarePlugin::_mavCommandResult(int vehicleId, int component, int command, int result, bool noReponseFromVehicle)
{
    Q_UNUSED(vehicleId);
    Q_UNUSED(component);
    Q_UNUSED(noReponseFromVehicle);

    auto* vehicle = qobject_cast<Vehicle*>(sender());
    if (!vehicle) {
        qWarning() << "Dynamic cast failed!";
        return;
    }

    if (command == MAV_CMD_NAV_TAKEOFF && result == MAV_RESULT_ACCEPTED) {
        // Now that we are in takeoff mode we can arm the vehicle which will cause it to takeoff.
        // We specifically don't retry arming if it fails. This way we don't fight with the user if
        // They are trying to disarm.
        disconnect(vehicle, &Vehicle::mavCommandResult, this, &ShenHangFirmwarePlugin::_mavCommandResult);
        if (!vehicle->armed()) {
            vehicle->setArmedShowError(true);
        }
    }
}

double ShenHangFirmwarePlugin::minimumTakeoffAltitude(Vehicle* vehicle)
{
    QString takeoffAltParam("MIS_TAKEOFF_ALT");

    if (vehicle->parameterManager()->parameterExists(FactSystem::defaultComponentId, takeoffAltParam)) {
        return vehicle->parameterManager()->getParameter(FactSystem::defaultComponentId, takeoffAltParam)->rawValue().toDouble();
    } else {
        return FirmwarePlugin::minimumTakeoffAltitude(vehicle);
    }
}

void ShenHangFirmwarePlugin::guidedModeTakeoff(Vehicle* vehicle, double takeoffAltRel)
{
    double vehicleAltitudeAMSL = vehicle->altitudeAMSL()->rawValue().toDouble();
    if (qIsNaN(vehicleAltitudeAMSL)) {
        qgcApp()->showAppMessage(tr("Unable to takeoff, vehicle position not known."));
        return;
    }

    double takeoffAltRelFromVehicle = minimumTakeoffAltitude(vehicle);
    double takeoffAltAMSL = qMax(takeoffAltRel, takeoffAltRelFromVehicle) + vehicleAltitudeAMSL;

    qDebug() << takeoffAltRel << takeoffAltRelFromVehicle << takeoffAltAMSL << vehicleAltitudeAMSL;

    connect(vehicle, &Vehicle::mavCommandResult, this, &ShenHangFirmwarePlugin::_mavCommandResult);
//    vehicle->sendMavCommand(
//        vehicle->defaultComponentId(),
//        MAV_CMD_NAV_TAKEOFF,
//        true,                                   // show error is fails
//        -1,                                     // No pitch requested
//        0, 0,                                   // param 2-4 unused
//        NAN, NAN, NAN,                          // No yaw, lat, lon
//        static_cast<float>(takeoffAltAMSL));    // AMSL altitude
}

void ShenHangFirmwarePlugin::guidedModeGotoLocation(Vehicle* vehicle, const QGeoCoordinate& gotoCoord)
{
    if (qIsNaN(vehicle->altitudeAMSL()->rawValue().toDouble())) {
        qgcApp()->showAppMessage(tr("Unable to go to location, vehicle position not known."));
        return;
    }

//    if (vehicle->capabilityBits() & MAV_PROTOCOL_CAPABILITY_COMMAND_INT) {
//        vehicle->sendMavCommandInt(vehicle->defaultComponentId(),
//                                   MAV_CMD_DO_REPOSITION,
//                                   MAV_FRAME_GLOBAL,
//                                   true,   // show error is fails
//                                   -1.0f,
//                                   MAV_DO_REPOSITION_FLAGS_CHANGE_MODE,
//                                   0.0f,
//                                   NAN,
//                                   gotoCoord.latitude(),
//                                   gotoCoord.longitude(),
//                                   vehicle->altitudeAMSL()->rawValue().toFloat());
//    } else {
//        vehicle->sendMavCommand(vehicle->defaultComponentId(),
//                                MAV_CMD_DO_REPOSITION,
//                                true,   // show error is fails
//                                -1.0f,
//                                MAV_DO_REPOSITION_FLAGS_CHANGE_MODE,
//                                0.0f,
//                                NAN,
//                                static_cast<float>(gotoCoord.latitude()),
//                                static_cast<float>(gotoCoord.longitude()),
//                                vehicle->altitudeAMSL()->rawValue().toFloat());
//    }
}

typedef struct {
    ShenHangFirmwarePlugin*  plugin;
    Vehicle*            vehicle;
    double              newAMSLAlt;
} PauseVehicleThenChangeAltData_t;


void ShenHangFirmwarePlugin::_changeAltAfterPause(void* resultHandlerData, bool pauseSucceeded)
{
    PauseVehicleThenChangeAltData_t* pData = static_cast<PauseVehicleThenChangeAltData_t*>(resultHandlerData);

    if (pauseSucceeded) {
//        pData->vehicle->sendMavCommand(
//                    pData->vehicle->defaultComponentId(),
//                    MAV_CMD_DO_REPOSITION,
//                    true,                                   // show error is fails
//                    -1.0f,                                  // Don't change groundspeed
//                    MAV_DO_REPOSITION_FLAGS_CHANGE_MODE,
//                    0.0f,                                   // Reserved
//                    qQNaN(), qQNaN(), qQNaN(),              // No change to yaw, lat, lon
//                    static_cast<float>(pData->newAMSLAlt));
    } else {
        qgcApp()->showAppMessage(tr("Unable to pause vehicle."));
    }

    delete pData;
}

void ShenHangFirmwarePlugin::guidedModeChangeAltitude(Vehicle* vehicle, double altitudeChange, bool pauseVehicle)
{
    if (!vehicle->homePosition().isValid()) {
        qgcApp()->showAppMessage(tr("Unable to change altitude, home position unknown."));
        return;
    }
    if (qIsNaN(vehicle->homePosition().altitude())) {
        qgcApp()->showAppMessage(tr("Unable to change altitude, home position altitude unknown."));
        return;
    }

    double currentAltRel = vehicle->altitudeRelative()->rawValue().toDouble();
    double newAltRel = currentAltRel + altitudeChange;

    PauseVehicleThenChangeAltData_t* resultData = new PauseVehicleThenChangeAltData_t;
    resultData->plugin      = this;
    resultData->vehicle     = vehicle;
    resultData->newAMSLAlt  = vehicle->homePosition().altitude() + newAltRel;

    if (pauseVehicle) {
//        vehicle->sendMavCommandWithHandler(
//                    _pauseVehicleThenChangeAltResultHandler,
//                    resultData,
//                    vehicle->defaultComponentId(),
//                    MAV_CMD_DO_REPOSITION,
//                    -1.0f,                                  // Don't change groundspeed
//                    MAV_DO_REPOSITION_FLAGS_CHANGE_MODE,
//                    0.0f,                                   // Reserved
//                    qQNaN(), qQNaN(), qQNaN(), qQNaN());    // No change to yaw, lat, lon, alt
    } else {
        _changeAltAfterPause(resultData, true /* pauseSucceeded */);
    }
}

void ShenHangFirmwarePlugin::startMission(Vehicle* vehicle)
{
    if (_setFlightModeAndValidate(vehicle, missionFlightMode())) {
        if (!_armVehicleAndValidate(vehicle)) {
            qgcApp()->showAppMessage(tr("Unable to start mission: Vehicle rejected arming."));
            return;
        }
    } else {
        qgcApp()->showAppMessage(tr("Unable to start mission: Vehicle not changing to %1 flight mode.").arg(missionFlightMode()));
    }
}

void ShenHangFirmwarePlugin::setGuidedMode(Vehicle* vehicle, bool guidedMode)
{
    if (guidedMode) {
        _setFlightModeAndValidate(vehicle, _holdFlightMode);
    } else {
        pauseVehicle(vehicle);
    }
}

bool ShenHangFirmwarePlugin::isGuidedMode(const Vehicle* vehicle) const
{
    // Not supported by generic vehicle
    return (vehicle->flightMode() == _holdFlightMode || vehicle->flightMode() == _takeoffFlightMode
            || vehicle->flightMode() == _landingFlightMode);
}

bool ShenHangFirmwarePlugin::adjustIncomingMavlinkMessage(Vehicle* vehicle, mavlink_message_t* message)
{
    //-- Don't process messages to/from UDP Bridge. It doesn't suffer from these issues
    if (message->compid == MAV_COMP_ID_UDP_BRIDGE) {
        return true;
    }

    switch (message->msgid) {
    case MAVLINK_MSG_ID_AUTOPILOT_VERSION:
        _handleAutopilotVersion(vehicle, message);
        break;
    }

    return true;
}

void ShenHangFirmwarePlugin::_handleAutopilotVersion(Vehicle* vehicle, mavlink_message_t* message)
{
    Q_UNUSED(vehicle);

    auto* instanceData = qobject_cast<ShenHangFirmwarePluginInstanceData*>(vehicle->firmwarePluginInstanceData());
    if (!instanceData->versionNotified) {
        bool notifyUser = false;
        int supportedMajorVersion = 1;
        int supportedMinorVersion = 4;
        int supportedPatchVersion = 1;

        mavlink_autopilot_version_t version;
        mavlink_msg_autopilot_version_decode(message, &version);

        if (version.flight_sw_version != 0) {
            int majorVersion, minorVersion, patchVersion;

            majorVersion = (version.flight_sw_version >> (8*3)) & 0xFF;
            minorVersion = (version.flight_sw_version >> (8*2)) & 0xFF;
            patchVersion = (version.flight_sw_version >> (8*1)) & 0xFF;

            if (majorVersion < supportedMajorVersion) {
                notifyUser = true;
            } else if (majorVersion == supportedMajorVersion) {
                if (minorVersion < supportedMinorVersion) {
                    notifyUser = true;
                } else if (minorVersion == supportedMinorVersion) {
                    notifyUser = patchVersion < supportedPatchVersion;
                }
            }
        } else {
            notifyUser = true;
        }

        if (notifyUser) {
            instanceData->versionNotified = true;
            qgcApp()->showAppMessage(tr("QGroundControl supports ShenHang Pro firmware Version %1.%2.%3 and above. You are using a version prior to that which will lead to unpredictable results. Please upgrade your firmware.").arg(supportedMajorVersion).arg(supportedMinorVersion).arg(supportedPatchVersion));
        }
    }
}

uint32_t ShenHangFirmwarePlugin::highLatencyCustomModeTo32Bits(uint16_t hlCustomMode)
{
    union px4_custom_mode px4_cm;
    px4_cm.data = 0;
    px4_cm.custom_mode_hl = hlCustomMode;

    return px4_cm.data;
}

QString ShenHangFirmwarePlugin::_getLatestVersionFileUrl(Vehicle* vehicle){
    Q_UNUSED(vehicle);
    return QStringLiteral("https://api.github.com/repos/ShenHang/Firmware/releases");
}

QString ShenHangFirmwarePlugin::_versionRegex() {
    return QStringLiteral("v([0-9,\\.]*) Stable");
}

bool ShenHangFirmwarePlugin::supportsNegativeThrust(Vehicle* vehicle)
{
    return vehicle->vehicleType() == MAV_TYPE_GROUND_ROVER;
}
