/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include <QObject>
#include <QString>
#include <QList>

#define MAVLINK_USE_MESSAGE_INFO
#define MAVLINK_EXTERNAL_RX_STATUS  // Single m_mavlink_status instance is in QGCApplication.cc
#include <stddef.h>                 // Hack workaround for Mav 2.0 header problem with respect to offsetof usage

// Ignore warnings from mavlink headers for both GCC/Clang and MSVC
#ifdef __GNUC__

#if __GNUC__ > 8
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#endif

#else
#pragma warning(push, 0)
#endif

//#include <mavlink_types.h>
//extern mavlink_status_t m_mavlink_status[MAVLINK_COMM_NUM_BUFFERS];
//#include <mavlink.h>

#ifdef __GNUC__
#pragma GCC diagnostic pop
#else
#pragma warning(pop, 0)
#endif

#ifdef __GNUC__
#define PACKED_STRUCT( __Declaration__ ) __Declaration__ __attribute__((packed))
#else
#define PACKED_STRUCT( __Declaration__ ) __pragma( pack(push, 1) ) __Declaration__ __pragma( pack(pop) )
#endif

# define MAVLINK_COMM_NUM_BUFFERS 16

typedef enum MAV_TYPE
{
   MAV_TYPE_GENERIC=0,          /* 通用类型 | */
   MAV_TYPE_HELICOPTER=1,       /* 直升机 | */
   MAV_TYPE_FIXED_WING=2,       /* 固定翼 | */
   MAV_TYPE_MULTIROTOR=3,       /* 多旋翼 | */
   MAV_TYPE_TANDEM_VECTOR=4,    /* 串列矢量 | */
   MAV_TYPE_JUXTAPOSED_VECTOR=5,/* 并列矢量 | */
   MAV_TYPE_TILT_ROTOR=6,       /* 倾转 | */
} MAV_TYPE;

typedef enum MAV_AUTOPILOT
{
   MAV_AUTOPILOT_GENERIC=0, /* Generic autopilot, full support for everything | */
   MAV_AUTOPILOT_SHEN_HANG=1, /* Reserved for future use. | */
} MAV_AUTOPILOT;

typedef enum MAV_RESULT
{
   MAV_RESULT_ACCEPTED=0, /* Command is valid (is supported and has valid parameters), and was executed. | */
   MAV_RESULT_TEMPORARILY_REJECTED=1, /* Command is valid, but cannot be executed at this time. This is used to indicate a problem that should be fixed just by waiting (e.g. a state machine is busy, can't arm because have not got GPS lock, etc.). Retrying later should work. | */
   MAV_RESULT_DENIED=2, /* Command is invalid (is supported but has invalid parameters). Retrying same command and parameters will not work. | */
   MAV_RESULT_UNSUPPORTED=3, /* Command is not supported (unknown). | */
   MAV_RESULT_FAILED=4, /* Command is valid, but execution has failed. This is used to indicate any non-temporary or unexpected problem, i.e. any problem that must be fixed before the command can succeed/be retried. For example, attempting to write a file when out of memory, attempting to arm when sensors are not calibrated, etc. | */
   MAV_RESULT_IN_PROGRESS=5, /* Command is valid and is being executed. This will be followed by further progress updates, i.e. the component may send further COMMAND_ACK messages with result MAV_RESULT_IN_PROGRESS (at a rate decided by the implementation), and must terminate by sending a COMMAND_ACK message with final result of the operation. The COMMAND_ACK.progress field can be used to indicate the progress of the operation. | */
   MAV_RESULT_CANCELLED=6, /* Command has been cancelled (as a result of receiving a COMMAND_CANCEL message). | */
   MAV_RESULT_ENUM_END=7, /*  | */
} MAV_RESULT;


typedef enum MAV_SYS_STATUS_SENSOR
{
   MAV_SYS_STATUS_SENSOR_3D_GYRO=1, /* 0x01 3D gyro | */
   MAV_SYS_STATUS_SENSOR_3D_ACCEL=2, /* 0x02 3D accelerometer | */
   MAV_SYS_STATUS_SENSOR_3D_MAG=4, /* 0x04 3D magnetometer | */
   MAV_SYS_STATUS_SENSOR_ABSOLUTE_PRESSURE=8, /* 0x08 absolute pressure | */
   MAV_SYS_STATUS_SENSOR_DIFFERENTIAL_PRESSURE=16, /* 0x10 differential pressure | */
   MAV_SYS_STATUS_SENSOR_GPS=32, /* 0x20 GPS | */
   MAV_SYS_STATUS_SENSOR_OPTICAL_FLOW=64, /* 0x40 optical flow | */
   MAV_SYS_STATUS_SENSOR_VISION_POSITION=128, /* 0x80 computer vision position | */
   MAV_SYS_STATUS_SENSOR_LASER_POSITION=256, /* 0x100 laser based position | */
   MAV_SYS_STATUS_SENSOR_EXTERNAL_GROUND_TRUTH=512, /* 0x200 external ground truth (Vicon or Leica) | */
   MAV_SYS_STATUS_SENSOR_ANGULAR_RATE_CONTROL=1024, /* 0x400 3D angular rate control | */
   MAV_SYS_STATUS_SENSOR_ATTITUDE_STABILIZATION=2048, /* 0x800 attitude stabilization | */
   MAV_SYS_STATUS_SENSOR_YAW_POSITION=4096, /* 0x1000 yaw position | */
   MAV_SYS_STATUS_SENSOR_Z_ALTITUDE_CONTROL=8192, /* 0x2000 z/altitude control | */
   MAV_SYS_STATUS_SENSOR_XY_POSITION_CONTROL=16384, /* 0x4000 x/y position control | */
   MAV_SYS_STATUS_SENSOR_MOTOR_OUTPUTS=32768, /* 0x8000 motor outputs / control | */
   MAV_SYS_STATUS_SENSOR_RC_RECEIVER=65536, /* 0x10000 rc receiver | */
   MAV_SYS_STATUS_SENSOR_3D_GYRO2=131072, /* 0x20000 2nd 3D gyro | */
   MAV_SYS_STATUS_SENSOR_3D_ACCEL2=262144, /* 0x40000 2nd 3D accelerometer | */
   MAV_SYS_STATUS_SENSOR_3D_MAG2=524288, /* 0x80000 2nd 3D magnetometer | */
   MAV_SYS_STATUS_GEOFENCE=1048576, /* 0x100000 geofence | */
   MAV_SYS_STATUS_AHRS=2097152, /* 0x200000 AHRS subsystem health | */
   MAV_SYS_STATUS_TERRAIN=4194304, /* 0x400000 Terrain subsystem health | */
   MAV_SYS_STATUS_REVERSE_MOTOR=8388608, /* 0x800000 Motors are reversed | */
   MAV_SYS_STATUS_LOGGING=16777216, /* 0x1000000 Logging | */
   MAV_SYS_STATUS_SENSOR_BATTERY=33554432, /* 0x2000000 Battery | */
   MAV_SYS_STATUS_SENSOR_PROXIMITY=67108864, /* 0x4000000 Proximity | */
   MAV_SYS_STATUS_SENSOR_SATCOM=134217728, /* 0x8000000 Satellite Communication  | */
   MAV_SYS_STATUS_PREARM_CHECK=268435456, /* 0x10000000 pre-arm check status. Always healthy when armed | */
   MAV_SYS_STATUS_OBSTACLE_AVOIDANCE=536870912, /* 0x20000000 Avoidance/collision prevention | */
   MAV_SYS_STATUS_SENSOR_PROPULSION=1073741824, /* 0x40000000 propulsion (actuator, esc, motor or propellor) | */
   MAV_SYS_STATUS_SENSOR_ENUM_END=1073741825, /*  | */
} MAV_SYS_STATUS_SENSOR;

class QGCMAVLink : public QObject
{
    Q_OBJECT

public:
    // Creating an instance of QGCMAVLink is only meant to be used for the Qml Singleton
    QGCMAVLink(QObject* parent = nullptr);

    typedef int FirmwareClass_t;
    typedef int VehicleClass_t;

    static constexpr FirmwareClass_t FirmwareClassGeneric        = MAV_AUTOPILOT_GENERIC;
    static constexpr FirmwareClass_t FirmwareClassShenHang       = MAV_AUTOPILOT_SHEN_HANG;

    static constexpr VehicleClass_t VehicleClassGeneric          = MAV_TYPE_GENERIC;
    static constexpr VehicleClass_t VehicleClassHelicopter       = MAV_TYPE_HELICOPTER;
    static constexpr VehicleClass_t VehicleClassFixedWing        = MAV_TYPE_FIXED_WING;
    static constexpr VehicleClass_t VehicleClassMultiRotor       = MAV_TYPE_MULTIROTOR;
    static constexpr VehicleClass_t VehicleClassTandemVector     = MAV_TYPE_TANDEM_VECTOR;
    static constexpr VehicleClass_t VehicleClassJustaposedVector = MAV_TYPE_JUXTAPOSED_VECTOR;
    static constexpr VehicleClass_t VehicleClassTiltRotor        = MAV_TYPE_TILT_ROTOR;

    static bool                     isShenHangFirmwareClass     (MAV_AUTOPILOT autopilot) { return autopilot == MAV_AUTOPILOT_SHEN_HANG; }
    static bool                     isGenericFirmwareClass      (MAV_AUTOPILOT autopilot) { return !isShenHangFirmwareClass(autopilot); }
    static FirmwareClass_t          firmwareClass               (MAV_AUTOPILOT autopilot);
    static MAV_AUTOPILOT            firmwareClassToAutopilot    (FirmwareClass_t firmwareClass) { return static_cast<MAV_AUTOPILOT>(firmwareClass); }
    static QString                  firmwareClassToString       (FirmwareClass_t firmwareClass);
    static QList<FirmwareClass_t>   allFirmwareClasses          (void);

    static bool                     isHelicopter                   (MAV_TYPE mavType);
    static bool                     isFixedWing                 (MAV_TYPE mavType);
    static bool                     isJustaposedVector                 (MAV_TYPE mavType);
    static bool                     isTandemVector                       (MAV_TYPE mavType);
    static bool                     isMultiRotor                (MAV_TYPE mavType);
    static bool                     TiltRotor                      (MAV_TYPE mavType);
    static VehicleClass_t           vehicleClass                (MAV_TYPE mavType);
    static MAV_TYPE                 vehicleClassToMavType       (VehicleClass_t vehicleClass) { return static_cast<MAV_TYPE>(vehicleClass); }
    static QString                  vehicleClassToString        (VehicleClass_t vehicleClass);
    static QList<VehicleClass_t>    allVehicleClasses           (void);

    static QString                  mavResultToString           (MAV_RESULT result);
    static QString                  mavSysStatusSensorToString  (MAV_SYS_STATUS_SENSOR sysStatusSensor);

    // Expose mavlink enums to Qml. I've tried various way to make this work without duping, but haven't found anything that works.

    enum MAV_BATTERY_FUNCTION {
        MAV_BATTERY_FUNCTION_UNKNOWN=0, /* Battery function is unknown | */
        MAV_BATTERY_FUNCTION_ALL=1, /* Battery supports all flight systems | */
        MAV_BATTERY_FUNCTION_PROPULSION=2, /* Battery for the propulsion system | */
        MAV_BATTERY_FUNCTION_AVIONICS=3, /* Avionics battery | */
        MAV_BATTERY_TYPE_PAYLOAD=4, /* Payload battery | */
    };
    Q_ENUM(MAV_BATTERY_FUNCTION)

    enum MAV_BATTERY_CHARGE_STATE
    {
       MAV_BATTERY_CHARGE_STATE_UNDEFINED=0, /* Low battery state is not provided | */
       MAV_BATTERY_CHARGE_STATE_OK=1, /* Battery is not in low state. Normal operation. | */
       MAV_BATTERY_CHARGE_STATE_LOW=2, /* Battery state is low, warn and monitor close. | */
       MAV_BATTERY_CHARGE_STATE_CRITICAL=3, /* Battery state is critical, return or abort immediately. | */
       MAV_BATTERY_CHARGE_STATE_EMERGENCY=4, /* Battery state is too low for ordinary abort sequence. Perform fastest possible emergency stop to prevent damage. | */
       MAV_BATTERY_CHARGE_STATE_FAILED=5, /* Battery failed, damage unavoidable. | */
       MAV_BATTERY_CHARGE_STATE_UNHEALTHY=6, /* Battery is diagnosed to be defective or an error occurred, usage is discouraged / prohibited. | */
       MAV_BATTERY_CHARGE_STATE_CHARGING=7, /* Battery is charging. | */
    };
    Q_ENUM(MAV_BATTERY_CHARGE_STATE)
};
//class MavlinkFTP {
//public:
//    /// This is the fixed length portion of the protocol data.
//    /// This needs to be packed, because it's typecasted from mavlink_file_transfer_protocol_t.payload, which starts
//    /// at a 3 byte offset, causing an unaligned access to seq_number and offset
//    PACKED_STRUCT(
//            typedef struct RequestHeader {
//                uint16_t    seqNumber;      ///< sequence number for message
//                uint8_t     session;        ///< Session id for read and write commands
//                uint8_t     opcode;         ///< Command opcode
//                uint8_t     size;           ///< Size of data
//                uint8_t     req_opcode;     ///< Request opcode returned in kRspAck, kRspNak message
//                uint8_t     burstComplete;  ///< Only used if req_opcode=kCmdBurstReadFile - 1: set of burst packets complete, 0: More burst packets coming.
//                uint8_t     paddng;        ///< 32 bit aligment padding
//                uint32_t    offset;         ///< Offsets for List and Read commands
//            }) RequestHeader;

//    PACKED_STRUCT(
//            typedef struct Request{
//                RequestHeader hdr;

//                // We use a union here instead of just casting (uint32_t)&payload[0] to not break strict aliasing rules
//                union {
//                    // The entire Request must fit into the payload member of the mavlink_file_transfer_protocol_t structure. We use as many leftover bytes
//                    // after we use up space for the RequestHeader for the data portion of the Request.
//                    uint8_t data[sizeof(((mavlink_file_transfer_protocol_t*)0)->payload) - sizeof(RequestHeader)];

//                    // File length returned by Open command
//                    uint32_t openFileLength;

//                    // Length of file chunk written by write command
//                    uint32_t writeFileLength;
//                };
//            }) Request;

//    typedef enum {
//        kCmdNone = 0,           ///< ignored, always acked
//        kCmdTerminateSession,	///< Terminates open Read session
//        kCmdResetSessions,		///< Terminates all open Read sessions
//        kCmdListDirectory,		///< List files in <path> from <offset>
//        kCmdOpenFileRO,			///< Opens file at <path> for reading, returns <session>
//        kCmdReadFile,			///< Reads <size> bytes from <offset> in <session>
//        kCmdCreateFile,			///< Creates file at <path> for writing, returns <session>
//        kCmdWriteFile,			///< Writes <size> bytes to <offset> in <session>
//        kCmdRemoveFile,			///< Remove file at <path>
//        kCmdCreateDirectory,	///< Creates directory at <path>
//        kCmdRemoveDirectory,	///< Removes Directory at <path>, must be empty
//        kCmdOpenFileWO,			///< Opens file at <path> for writing, returns <session>
//        kCmdTruncateFile,		///< Truncate file at <path> to <offset> length
//        kCmdRename,				///< Rename <path1> to <path2>
//        kCmdCalcFileCRC32,		///< Calculate CRC32 for file at <path>
//        kCmdBurstReadFile,      ///< Burst download session file

//        kRspAck = 128,          ///< Ack response
//        kRspNak,                ///< Nak response
//    } OpCode_t;

//    /// @brief Error codes returned in Nak response PayloadHeader.data[0].
//   typedef enum {
//        kErrNone = 0,
//        kErrFail,                   ///< Unknown failure
//        kErrFailErrno,              ///< errno sent back in PayloadHeader.data[1]
//        kErrInvalidDataSize,		///< PayloadHeader.size is invalid
//        kErrInvalidSession,         ///< Session is not currently open
//        kErrNoSessionsAvailable,	///< All available Sessions in use
//        kErrEOF,                    ///< Offset past end of file for List and Read commands
//        kErrUnknownCommand,         ///< Unknown command opcode
//        kErrFailFileExists,         ///< File exists already
//        kErrFailFileProtected,      ///< File is write protected
//        kErrFailFileNotFound
//    } ErrorCode_t;

//    static QString opCodeToString   (OpCode_t opCode);
//    static QString errorCodeToString(ErrorCode_t errorCode);
//};
