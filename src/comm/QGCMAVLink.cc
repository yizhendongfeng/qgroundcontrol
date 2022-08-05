/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "QGCMAVLink.h"

#include <QtGlobal>
#include <QDebug>

constexpr QGCMAVLink::FirmwareClass_t QGCMAVLink::FirmwareClassGeneric;
constexpr QGCMAVLink::FirmwareClass_t QGCMAVLink::FirmwareClassShenHang;

constexpr QGCMAVLink::VehicleClass_t QGCMAVLink::VehicleClassGeneric;
constexpr QGCMAVLink::VehicleClass_t QGCMAVLink::VehicleClassHelicopter;
constexpr QGCMAVLink::VehicleClass_t QGCMAVLink::VehicleClassFixedWing;
constexpr QGCMAVLink::VehicleClass_t QGCMAVLink::VehicleClassMultiRotor;
constexpr QGCMAVLink::VehicleClass_t QGCMAVLink::VehicleClassTandemVector;
constexpr QGCMAVLink::VehicleClass_t QGCMAVLink::VehicleClassJustaposedVector;
constexpr QGCMAVLink::VehicleClass_t QGCMAVLink::VehicleClassTiltRotor;

QGCMAVLink::QGCMAVLink(QObject* parent)
    : QObject(parent)
{

}

QList<QGCMAVLink::FirmwareClass_t> QGCMAVLink::allFirmwareClasses(void)
{
    static const QList<QGCMAVLink::FirmwareClass_t> classes = {
        FirmwareClassGeneric,
        FirmwareClassShenHang,
    };
    return classes;
}

QList<QGCMAVLink::VehicleClass_t> QGCMAVLink::allVehicleClasses(void)
{
    static const QList<QGCMAVLink::VehicleClass_t> classes = {
        VehicleClassGeneric,
        VehicleClassHelicopter,
        VehicleClassFixedWing,
        VehicleClassMultiRotor,
        VehicleClassTandemVector,
        VehicleClassJustaposedVector,
        VehicleClassTiltRotor,
    };
    return classes;
}

QGCMAVLink::FirmwareClass_t QGCMAVLink::firmwareClass(MAV_AUTOPILOT autopilot)
{
    if (isShenHangFirmwareClass(autopilot)) {
        return FirmwareClassShenHang;
    }
    else {
        return FirmwareClassGeneric;
    }
}

QString QGCMAVLink::firmwareClassToString(FirmwareClass_t firmwareClass)
{
    switch (firmwareClass) {
    case MAV_AUTOPILOT_SHEN_HANG:
        return QT_TRANSLATE_NOOP("Firmware Class", "ShenHang");
   case FirmwareClassGeneric:
        return QT_TRANSLATE_NOOP("Firmware Class", "Generic");
    default:
        return QT_TRANSLATE_NOOP("Firmware Class", "Unknown");
    }
}

bool QGCMAVLink::isHelicopter(MAV_TYPE mavType)
{
    return vehicleClass(mavType) == VehicleClassHelicopter;
}

bool QGCMAVLink::isFixedWing(MAV_TYPE mavType)
{
    return vehicleClass(mavType) == VehicleClassFixedWing;
}

bool QGCMAVLink::isMultiRotor(MAV_TYPE mavType)
{
    return vehicleClass(mavType) == VehicleClassMultiRotor;
}

bool QGCMAVLink::isTandemVector(MAV_TYPE mavType)
{
    return vehicleClass(mavType) == VehicleClassTandemVector;
}


bool QGCMAVLink::isJustaposedVector(MAV_TYPE mavType)
{
    return vehicleClass(mavType) == VehicleClassJustaposedVector;
}

bool QGCMAVLink::TiltRotor(MAV_TYPE mavType)
{
    return vehicleClass(mavType) == VehicleClassTiltRotor;
}

QGCMAVLink::VehicleClass_t QGCMAVLink::vehicleClass(MAV_TYPE mavType)
{
    switch (mavType) {
    case MAV_TYPE_HELICOPTER:
        return VehicleClassHelicopter;
    case MAV_TYPE_FIXED_WING:
        return VehicleClassFixedWing;
    case MAV_TYPE_TANDEM_VECTOR:
        return VehicleClassMultiRotor;
    case MAV_TYPE_JUXTAPOSED_VECTOR:
        return VehicleClassTandemVector;
    case MAV_TYPE_TILT_ROTOR:
        return VehicleClassTiltRotor;
    default:
        return VehicleClassGeneric;
    }
}


constexpr QGCMAVLink::VehicleClass_t QGCMAVLink::VehicleClassGeneric;
constexpr QGCMAVLink::VehicleClass_t QGCMAVLink::VehicleClassHelicopter;
constexpr QGCMAVLink::VehicleClass_t QGCMAVLink::VehicleClassFixedWing;
constexpr QGCMAVLink::VehicleClass_t QGCMAVLink::VehicleClassMultiRotor;
constexpr QGCMAVLink::VehicleClass_t QGCMAVLink::VehicleClassTandemVector;
constexpr QGCMAVLink::VehicleClass_t QGCMAVLink::VehicleClassJustaposedVector;
constexpr QGCMAVLink::VehicleClass_t QGCMAVLink::VehicleClassTiltRotor;

QString QGCMAVLink::vehicleClassToString(VehicleClass_t vehicleClass)
{
    switch (vehicleClass) {
    case VehicleClassGeneric:
        return QT_TRANSLATE_NOOP("Vehicle Class", "Generic");
    case VehicleClassHelicopter:
        return QT_TRANSLATE_NOOP("Vehicle Class", "Helicopter");
    case VehicleClassFixedWing:
        return QT_TRANSLATE_NOOP("Vehicle Class", "Fixed Wing");
    case VehicleClassMultiRotor:
        return QT_TRANSLATE_NOOP("Vehicle Class", "Multi-Rotor");
    case VehicleClassTandemVector:
        return QT_TRANSLATE_NOOP("Vehicle Class", "Tandem Vector");
    case VehicleClassJustaposedVector:
        return QT_TRANSLATE_NOOP("Vehicle Class", "Justaposed Vector");
    default:
        return QT_TRANSLATE_NOOP("Vehicle Class", "Unknown");
    }
}

QString QGCMAVLink::mavResultToString(MAV_RESULT result)
{
    switch (result) {
    case MAV_RESULT_ACCEPTED:
        return QStringLiteral("MAV_RESULT_ACCEPTED");
    case MAV_RESULT_TEMPORARILY_REJECTED:
        return QStringLiteral("MAV_RESULT_TEMPORARILY_REJECTED");
    case MAV_RESULT_DENIED:
        return QStringLiteral("MAV_RESULT_DENIED");
    case MAV_RESULT_UNSUPPORTED:
        return QStringLiteral("MAV_RESULT_UNSUPPORTED");
    case MAV_RESULT_FAILED:
        return QStringLiteral("MAV_RESULT_FAILED");
    case MAV_RESULT_IN_PROGRESS:
        return QStringLiteral("MAV_RESULT_IN_PROGRESS");
    default:
        return QStringLiteral("MAV_RESULT unknown %1").arg(result);
    }
}

QString QGCMAVLink::mavSysStatusSensorToString(MAV_SYS_STATUS_SENSOR sysStatusSensor)
{
    struct sensorInfo_s {
        uint32_t    bit;
        QString     sensorName;
    };

    static const sensorInfo_s rgSensorInfo[] = {
        { MAV_SYS_STATUS_SENSOR_3D_GYRO,                QT_TRANSLATE_NOOP("MAVLink SYS_STATUS_SENSOR value", "Gyro") },
        { MAV_SYS_STATUS_SENSOR_3D_ACCEL,               QT_TRANSLATE_NOOP("MAVLink SYS_STATUS_SENSOR value", "Accelerometer") },
        { MAV_SYS_STATUS_SENSOR_3D_MAG,                 QT_TRANSLATE_NOOP("MAVLink SYS_STATUS_SENSOR value", "Magnetometer") },
        { MAV_SYS_STATUS_SENSOR_ABSOLUTE_PRESSURE,      QT_TRANSLATE_NOOP("MAVLink SYS_STATUS_SENSOR value", "Absolute pressure") },
        { MAV_SYS_STATUS_SENSOR_DIFFERENTIAL_PRESSURE,  QT_TRANSLATE_NOOP("MAVLink SYS_STATUS_SENSOR value", "Differential pressure") },
        { MAV_SYS_STATUS_SENSOR_GPS,                    QT_TRANSLATE_NOOP("MAVLink SYS_STATUS_SENSOR value", "GPS") },
        { MAV_SYS_STATUS_SENSOR_OPTICAL_FLOW,           QT_TRANSLATE_NOOP("MAVLink SYS_STATUS_SENSOR value", "Optical flow") },
        { MAV_SYS_STATUS_SENSOR_VISION_POSITION,        QT_TRANSLATE_NOOP("MAVLink SYS_STATUS_SENSOR value", "Computer vision position") },
        { MAV_SYS_STATUS_SENSOR_LASER_POSITION,         QT_TRANSLATE_NOOP("MAVLink SYS_STATUS_SENSOR value", "Laser based position") },
        { MAV_SYS_STATUS_SENSOR_EXTERNAL_GROUND_TRUTH,  QT_TRANSLATE_NOOP("MAVLink SYS_STATUS_SENSOR value", "External ground truth") },
        { MAV_SYS_STATUS_SENSOR_ANGULAR_RATE_CONTROL,   QT_TRANSLATE_NOOP("MAVLink SYS_STATUS_SENSOR value", "Angular rate control") },
        { MAV_SYS_STATUS_SENSOR_ATTITUDE_STABILIZATION, QT_TRANSLATE_NOOP("MAVLink SYS_STATUS_SENSOR value", "Attitude stabilization") },
        { MAV_SYS_STATUS_SENSOR_YAW_POSITION,           QT_TRANSLATE_NOOP("MAVLink SYS_STATUS_SENSOR value", "Yaw position") },
        { MAV_SYS_STATUS_SENSOR_Z_ALTITUDE_CONTROL,     QT_TRANSLATE_NOOP("MAVLink SYS_STATUS_SENSOR value", "Z/altitude control") },
        { MAV_SYS_STATUS_SENSOR_XY_POSITION_CONTROL,    QT_TRANSLATE_NOOP("MAVLink SYS_STATUS_SENSOR value", "X/Y position control") },
        { MAV_SYS_STATUS_SENSOR_MOTOR_OUTPUTS,          QT_TRANSLATE_NOOP("MAVLink SYS_STATUS_SENSOR value", "Motor outputs / control") },
        { MAV_SYS_STATUS_SENSOR_RC_RECEIVER,            QT_TRANSLATE_NOOP("MAVLink SYS_STATUS_SENSOR value", "RC receiver") },
        { MAV_SYS_STATUS_SENSOR_3D_GYRO2,               QT_TRANSLATE_NOOP("MAVLink SYS_STATUS_SENSOR value", "Gyro 2") },
        { MAV_SYS_STATUS_SENSOR_3D_ACCEL2,              QT_TRANSLATE_NOOP("MAVLink SYS_STATUS_SENSOR value", "Accelerometer 2") },
        { MAV_SYS_STATUS_SENSOR_3D_MAG2,                QT_TRANSLATE_NOOP("MAVLink SYS_STATUS_SENSOR value", "Magnetometer 2") },
        { MAV_SYS_STATUS_GEOFENCE,                      QT_TRANSLATE_NOOP("MAVLink SYS_STATUS_SENSOR value", "GeoFence") },
        { MAV_SYS_STATUS_AHRS,                          QT_TRANSLATE_NOOP("MAVLink SYS_STATUS_SENSOR value", "AHRS") },
        { MAV_SYS_STATUS_TERRAIN,                       QT_TRANSLATE_NOOP("MAVLink SYS_STATUS_SENSOR value", "Terrain") },
        { MAV_SYS_STATUS_REVERSE_MOTOR,                 QT_TRANSLATE_NOOP("MAVLink SYS_STATUS_SENSOR value", "Motors reversed") },
        { MAV_SYS_STATUS_LOGGING,                       QT_TRANSLATE_NOOP("MAVLink SYS_STATUS_SENSOR value", "Logging") },
        { MAV_SYS_STATUS_SENSOR_BATTERY,                QT_TRANSLATE_NOOP("MAVLink SYS_STATUS_SENSOR value", "Battery") },
        { MAV_SYS_STATUS_SENSOR_PROXIMITY,              QT_TRANSLATE_NOOP("MAVLink SYS_STATUS_SENSOR value", "Proximity") },
        { MAV_SYS_STATUS_SENSOR_SATCOM,                 QT_TRANSLATE_NOOP("MAVLink SYS_STATUS_SENSOR value", "Satellite Communication") },
        { MAV_SYS_STATUS_PREARM_CHECK,                  QT_TRANSLATE_NOOP("MAVLink SYS_STATUS_SENSOR value", "Pre-Arm Check") },
        { MAV_SYS_STATUS_OBSTACLE_AVOIDANCE,            QT_TRANSLATE_NOOP("MAVLink SYS_STATUS_SENSOR value", "Avoidance/collision prevention") },
    };

    for (size_t i=0; i<sizeof(rgSensorInfo)/sizeof(sensorInfo_s); i++) {
        const sensorInfo_s* pSensorInfo = &rgSensorInfo[i];
        if (sysStatusSensor == pSensorInfo->bit) {
            return pSensorInfo->sensorName;
        }
    }

    qWarning() << "QGCMAVLink::mavSysStatusSensorToString: Unknown sensor" << sysStatusSensor;

    return QT_TRANSLATE_NOOP("MAVLink unknown SYS_STATUS_SENSOR value", "Unknown sensor");
}

//QString MavlinkFTP::opCodeToString(OpCode_t opCode)
//{
//    switch (opCode) {
//    case kCmdNone:
//        return "None";
//    case kCmdTerminateSession:
//        return "Terminate Session";
//    case kCmdResetSessions:
//        return "Reset Sessions";
//    case kCmdListDirectory:
//        return "List Directory";
//    case kCmdOpenFileRO:
//        return "Open File RO";
//    case kCmdReadFile:
//        return "Read File";
//    case kCmdCreateFile:
//        return "Create File";
//    case kCmdWriteFile:
//        return "Write File";
//    case kCmdRemoveFile:
//        return "Remove File";
//    case kCmdCreateDirectory:
//        return "Create Directory";
//    case kCmdRemoveDirectory:
//        return "Remove Directory";
//    case kCmdOpenFileWO:
//        return "Open File WO";
//    case kCmdTruncateFile:
//        return "Truncate File";
//    case kCmdRename:
//        return "Rename";
//    case kCmdCalcFileCRC32:
//        return "Calc File CRC32";
//    case kCmdBurstReadFile:
//        return "Burst Read File";
//    case kRspAck:
//        return "Ack";
//    case kRspNak:
//        return "Nak";
//    }

//    return "Unknown OpCode";
//}

//QString MavlinkFTP::errorCodeToString(ErrorCode_t errorCode)
//{
//    switch (errorCode) {
//    case kErrNone:
//        return "None";
//    case kErrFail:
//        return "Fail";
//    case kErrFailErrno:
//        return "Fail Errorno";
//    case kErrInvalidDataSize:
//        return "Invalid Data Size";
//    case kErrInvalidSession:
//        return "Invalid Session";
//    case kErrNoSessionsAvailable:
//        return "No Sessions Available";
//    case kErrEOF:
//        return "EOF";
//    case kErrUnknownCommand:
//        return "Unknown Command";
//    case kErrFailFileExists:
//        return "File Already Exists";
//    case kErrFailFileProtected:
//        return "File Protected";
//    case kErrFailFileNotFound:
//        return "File Not Found";
//    }

//    return "Unknown Error";
//}

