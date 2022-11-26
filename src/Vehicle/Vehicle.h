﻿/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QVariantList>
#include <QGeoCoordinate>
#include <QTime>
#include <QQueue>

#include "FactGroup.h"
#include "QGCMAVLink.h"
#include "QmlObjectListModel.h"
#include "SettingsFact.h"
#include "QGCMapCircle.h"
#include "VehicleClockFactGroup.h"
#include "VehicleGPSFactGroup.h"
#include "VehicleSetpointFactGroup.h"
#include "VehicleLinkManager.h"
#include "MissionManager.h"
#include "GeoFenceManager.h"

#include "ShenHangVehicleData.h"
#include "ShenHangProtocol.h"

class UAS;
class UASInterface;
class FirmwarePlugin;
class FirmwarePluginManager;
class AutoPilotPlugin;
class ParameterManager;
class UASMessage;
class SettingsManager;
class QGCCameraManager;
class Joystick;
class TrajectoryPoints;
//class VehicleBatteryFactGroup;
class SendMavCommandWithSignallingTest;
class SendMavCommandWithHandlerTest;
class RequestMessageTest;
class LinkInterface;
class LinkManager;
class InitialConnectStateMachine;

#if defined(QGC_AIRMAP_ENABLED)
class AirspaceVehicleManager;
#endif

Q_DECLARE_LOGGING_CATEGORY(VehicleLog)


typedef enum MAV_SEVERITY
{
   MAV_SEVERITY_EMERGENCY=0, /* System is unusable. This is a "panic" condition. | */
   MAV_SEVERITY_ALERT=1, /* Action should be taken immediately. Indicates error in non-critical systems. | */
   MAV_SEVERITY_CRITICAL=2, /* Action must be taken immediately. Indicates failure in a primary system. | */
   MAV_SEVERITY_ERROR=3, /* Indicates an error in secondary/redundant systems. | */
   MAV_SEVERITY_WARNING=4, /* Indicates about a possible future error if this is not resolved within a given timeframe. Example would be a low battery warning. | */
   MAV_SEVERITY_NOTICE=5, /* An unusual event has occurred, though not an error condition. This should be investigated for the root cause. | */
   MAV_SEVERITY_INFO=6, /* Normal operational messages. Useful for logging. No action is required for these messages. | */
   MAV_SEVERITY_DEBUG=7, /* Useful non-operational messages that can assist in debugging. These should not occur during normal operation. | */
   MAV_SEVERITY_ENUM_END=8, /*  | */
} MAV_SEVERITY;

class Vehicle : public FactGroup
{
    Q_OBJECT

    friend class InitialConnectStateMachine;
    friend class VehicleLinkManager;
//    friend class VehicleBatteryFactGroup;           // Allow VehicleBatteryFactGroup to call _addFactGroup
    friend class SendMavCommandWithSignallingTest;  // Unit test
    friend class SendMavCommandWithHandlerTest;     // Unit test
    friend class RequestMessageTest;                // Unit test


public:
    Vehicle(LinkInterface*          link,
            int                     vehicleId,
            MAV_AUTOPILOT           firmwareType,
            MAV_TYPE                vehicleType,
            FirmwarePluginManager*  firmwarePluginManager);

    // Pass these into the offline constructor to create an offline vehicle which tracks the offline vehicle settings
    static const MAV_AUTOPILOT    MAV_AUTOPILOT_TRACK = static_cast<MAV_AUTOPILOT>(-1);
    static const MAV_TYPE         MAV_TYPE_TRACK = static_cast<MAV_TYPE>(-1);

    // The following is used to create a disconnected Vehicle for use while offline editing.
    Vehicle(MAV_AUTOPILOT           firmwareType,
            MAV_TYPE                vehicleType,
            FirmwarePluginManager*  firmwarePluginManager,
            QObject*                parent = nullptr);

    ~Vehicle();

    /// Sensor bits from sensors*Bits properties
    enum MavlinkSysStatus {
        SysStatusSensor3dGyro =                 MAV_SYS_STATUS_SENSOR_3D_GYRO,
        SysStatusSensor3dAccel =                MAV_SYS_STATUS_SENSOR_3D_ACCEL,
        SysStatusSensor3dMag =                  MAV_SYS_STATUS_SENSOR_3D_MAG,
        SysStatusSensorAbsolutePressure =       MAV_SYS_STATUS_SENSOR_ABSOLUTE_PRESSURE,
        SysStatusSensorDifferentialPressure =   MAV_SYS_STATUS_SENSOR_DIFFERENTIAL_PRESSURE,
        SysStatusSensorGPS =                    MAV_SYS_STATUS_SENSOR_GPS,
        SysStatusSensorOpticalFlow =            MAV_SYS_STATUS_SENSOR_OPTICAL_FLOW,
        SysStatusSensorVisionPosition =         MAV_SYS_STATUS_SENSOR_VISION_POSITION,
        SysStatusSensorLaserPosition =          MAV_SYS_STATUS_SENSOR_LASER_POSITION,
        SysStatusSensorExternalGroundTruth =    MAV_SYS_STATUS_SENSOR_EXTERNAL_GROUND_TRUTH,
        SysStatusSensorAngularRateControl =     MAV_SYS_STATUS_SENSOR_ANGULAR_RATE_CONTROL,
        SysStatusSensorAttitudeStabilization =  MAV_SYS_STATUS_SENSOR_ATTITUDE_STABILIZATION,
        SysStatusSensorYawPosition =            MAV_SYS_STATUS_SENSOR_YAW_POSITION,
        SysStatusSensorZAltitudeControl =       MAV_SYS_STATUS_SENSOR_Z_ALTITUDE_CONTROL,
        SysStatusSensorXYPositionControl =      MAV_SYS_STATUS_SENSOR_XY_POSITION_CONTROL,
        SysStatusSensorMotorOutputs =           MAV_SYS_STATUS_SENSOR_MOTOR_OUTPUTS,
        SysStatusSensorRCReceiver =             MAV_SYS_STATUS_SENSOR_RC_RECEIVER,
        SysStatusSensor3dGyro2 =                MAV_SYS_STATUS_SENSOR_3D_GYRO2,
        SysStatusSensor3dAccel2 =               MAV_SYS_STATUS_SENSOR_3D_ACCEL2,
        SysStatusSensor3dMag2 =                 MAV_SYS_STATUS_SENSOR_3D_MAG2,
        SysStatusSensorGeoFence =               MAV_SYS_STATUS_GEOFENCE,
        SysStatusSensorAHRS =                   MAV_SYS_STATUS_AHRS,
        SysStatusSensorTerrain =                MAV_SYS_STATUS_TERRAIN,
        SysStatusSensorReverseMotor =           MAV_SYS_STATUS_REVERSE_MOTOR,
        SysStatusSensorLogging =                MAV_SYS_STATUS_LOGGING,
        SysStatusSensorBattery =                MAV_SYS_STATUS_SENSOR_BATTERY,
    };
    Q_ENUM(MavlinkSysStatus)

    enum CheckList {
        CheckListNotSetup = 0,
        CheckListPassed,
        CheckListFailed,
    };
    Q_ENUM(CheckList)

    Q_PROPERTY(int                  id                          READ id                                                             CONSTANT)
    Q_PROPERTY(AutoPilotPlugin*     autopilot                   MEMBER _autopilotPlugin                                             CONSTANT)
    Q_PROPERTY(QGeoCoordinate       coordinate                  READ coordinate                                                     NOTIFY coordinateChanged)
    Q_PROPERTY(QGeoCoordinate       homePosition                READ homePosition                                                   NOTIFY homePositionChanged)
    Q_PROPERTY(QGeoCoordinate       armedPosition               READ armedPosition                                                  NOTIFY armedPositionChanged)
    Q_PROPERTY(bool                 armed                       READ armed                      WRITE setArmedShowError             NOTIFY armedChanged)
    Q_PROPERTY(bool                 autoDisarm                  READ autoDisarm                                                     NOTIFY autoDisarmChanged)
    Q_PROPERTY(bool                 flightModeSetAvailable      READ flightModeSetAvailable                                         CONSTANT)
    Q_PROPERTY(QStringList          flightModes                 READ flightModes                                                    NOTIFY flightModesChanged)
    Q_PROPERTY(QStringList          extraJoystickFlightModes    READ extraJoystickFlightModes                                       NOTIFY flightModesChanged)
    Q_PROPERTY(QString              flightMode                  READ flightMode                 WRITE setFlightMode                 NOTIFY flightModeChanged)
    Q_PROPERTY(TrajectoryPoints*    trajectoryPoints            MEMBER _trajectoryPoints                                            CONSTANT)
    Q_PROPERTY(QmlObjectListModel*  cameraTriggerPoints         READ cameraTriggerPoints                                            CONSTANT)
    Q_PROPERTY(float                latitude                    READ latitude                                                       NOTIFY coordinateChanged)
    Q_PROPERTY(float                longitude                   READ longitude                                                      NOTIFY coordinateChanged)
    Q_PROPERTY(bool                 messageTypeNone             READ messageTypeNone                                                NOTIFY messageTypeChanged)
    Q_PROPERTY(bool                 messageTypeNormal           READ messageTypeNormal                                              NOTIFY messageTypeChanged)
    Q_PROPERTY(bool                 messageTypeWarning          READ messageTypeWarning                                             NOTIFY messageTypeChanged)
    Q_PROPERTY(bool                 messageTypeError            READ messageTypeError                                               NOTIFY messageTypeChanged)
    Q_PROPERTY(int                  newMessageCount             READ newMessageCount                                                NOTIFY newMessageCountChanged)
    Q_PROPERTY(int                  messageCount                READ messageCount                                                   NOTIFY messageCountChanged)
    Q_PROPERTY(QString              formattedMessages           READ formattedMessages                                              NOTIFY formattedMessagesChanged)
    Q_PROPERTY(QString              latestError                 READ latestError                                                    NOTIFY latestErrorChanged)
    Q_PROPERTY(int                  flowImageIndex              READ flowImageIndex                                                 NOTIFY flowImageIndexChanged)
    Q_PROPERTY(int                  rcRSSI                      READ rcRSSI                                                         NOTIFY rcRSSIChanged)
    Q_PROPERTY(bool                 shenHangFirmware            READ shenHangFirmware                                                    NOTIFY firmwareTypeChanged)
    Q_PROPERTY(bool                 genericFirmware             READ genericFirmware                                                CONSTANT)
    Q_PROPERTY(uint                 messagesReceived            READ messagesReceived                                               NOTIFY messagesReceivedChanged)
    Q_PROPERTY(uint                 messagesSent                READ messagesSent                                                   NOTIFY messagesSentChanged)
    Q_PROPERTY(uint                 messagesLost                READ messagesLost                                                   NOTIFY messagesLostChanged)
    Q_PROPERTY(bool                 airship                     READ airship                                                        NOTIFY vehicleTypeChanged)
    Q_PROPERTY(bool                 fixedWing                   READ fixedWing                                                      NOTIFY vehicleTypeChanged)
    Q_PROPERTY(bool                 multiRotor                  READ multiRotor                                                     NOTIFY vehicleTypeChanged)
    Q_PROPERTY(bool                 vtol                        READ vtol                                                           NOTIFY vehicleTypeChanged)
    Q_PROPERTY(bool                 rover                       READ rover                                                          NOTIFY vehicleTypeChanged)
    Q_PROPERTY(bool                 sub                         READ sub                                                            NOTIFY vehicleTypeChanged)
    Q_PROPERTY(bool        supportsThrottleModeCenterZero       READ supportsThrottleModeCenterZero                                 CONSTANT)
    Q_PROPERTY(bool                supportsNegativeThrust       READ supportsNegativeThrust                                         CONSTANT)
    Q_PROPERTY(bool                 supportsJSButton            READ supportsJSButton                                               CONSTANT)
    Q_PROPERTY(bool                 supportsRadio               READ supportsRadio                                                  CONSTANT)
    Q_PROPERTY(bool               supportsMotorInterference     READ supportsMotorInterference                                      CONSTANT)
    Q_PROPERTY(QString              prearmError                 READ prearmError                WRITE setPrearmError                NOTIFY prearmErrorChanged)
    Q_PROPERTY(bool                 coaxialMotors               READ coaxialMotors                                                  CONSTANT)
    Q_PROPERTY(bool                 xConfigMotors               READ xConfigMotors                                                  CONSTANT)
    Q_PROPERTY(bool                 isOfflineEditingVehicle     READ isOfflineEditingVehicle                                        CONSTANT)
    Q_PROPERTY(QString              brandImageIndoor            READ brandImageIndoor                                               NOTIFY firmwareTypeChanged)
    Q_PROPERTY(QString              brandImageOutdoor           READ brandImageOutdoor                                              NOTIFY firmwareTypeChanged)
    Q_PROPERTY(int                  sensorsPresentBits          READ sensorsPresentBits                                             NOTIFY sensorsPresentBitsChanged)
    Q_PROPERTY(int                  sensorsEnabledBits          READ sensorsEnabledBits                                             NOTIFY sensorsEnabledBitsChanged)
    Q_PROPERTY(int                  sensorsHealthBits           READ sensorsHealthBits                                              NOTIFY sensorsHealthBitsChanged)
    Q_PROPERTY(int                  sensorsUnhealthyBits        READ sensorsUnhealthyBits                                           NOTIFY sensorsUnhealthyBitsChanged) ///< Combination of enabled and health
    Q_PROPERTY(QString              missionFlightMode           READ missionFlightMode                                              CONSTANT)
    Q_PROPERTY(QString              pauseFlightMode             READ pauseFlightMode                                                CONSTANT)
    Q_PROPERTY(QString              rtlFlightMode               READ rtlFlightMode                                                  CONSTANT)
    Q_PROPERTY(QString              smartRTLFlightMode          READ smartRTLFlightMode                                             CONSTANT)
    Q_PROPERTY(bool                 supportsSmartRTL            READ supportsSmartRTL                                               CONSTANT)
    Q_PROPERTY(QString              landFlightMode              READ landFlightMode                                                 CONSTANT)
    Q_PROPERTY(QString              takeControlFlightMode       READ takeControlFlightMode                                          CONSTANT)
    Q_PROPERTY(QString              followFlightMode            READ followFlightMode                                               CONSTANT)
    Q_PROPERTY(QString              vehicleImageOpaque          READ vehicleImageOpaque                                             CONSTANT)
    Q_PROPERTY(QString              vehicleImageOutline         READ vehicleImageOutline                                            CONSTANT)
    Q_PROPERTY(QString              vehicleImageCompass         READ vehicleImageCompass                                            CONSTANT)
    Q_PROPERTY(int                  telemetryRRSSI              READ telemetryRRSSI                                                 NOTIFY telemetryRRSSIChanged)
    Q_PROPERTY(int                  telemetryLRSSI              READ telemetryLRSSI                                                 NOTIFY telemetryLRSSIChanged)
    Q_PROPERTY(unsigned int         telemetryRXErrors           READ telemetryRXErrors                                              NOTIFY telemetryRXErrorsChanged)
    Q_PROPERTY(unsigned int         telemetryFixed              READ telemetryFixed                                                 NOTIFY telemetryFixedChanged)
    Q_PROPERTY(unsigned int         telemetryTXBuffer           READ telemetryTXBuffer                                              NOTIFY telemetryTXBufferChanged)
    Q_PROPERTY(int                  telemetryLNoise             READ telemetryLNoise                                                NOTIFY telemetryLNoiseChanged)
    Q_PROPERTY(int                  telemetryRNoise             READ telemetryRNoise                                                NOTIFY telemetryRNoiseChanged)
    Q_PROPERTY(QVariantList         toolIndicators              READ toolIndicators                                                 NOTIFY toolIndicatorsChanged)
    Q_PROPERTY(QVariantList         modeIndicators              READ modeIndicators                                                 NOTIFY modeIndicatorsChanged)
    Q_PROPERTY(bool              initialPlanRequestComplete     READ initialPlanRequestComplete                                     NOTIFY initialPlanRequestCompleteChanged)
    Q_PROPERTY(QVariantList         staticCameraList            READ staticCameraList                                               CONSTANT)
//    Q_PROPERTY(QGCCameraManager*    cameraManager               READ cameraManager                                                  NOTIFY cameraManagerChanged)
    Q_PROPERTY(QString              hobbsMeter                  READ hobbsMeter                                                     NOTIFY hobbsMeterChanged)
    Q_PROPERTY(bool                 vtolInFwdFlight             READ vtolInFwdFlight            WRITE setVtolInFwdFlight            NOTIFY vtolInFwdFlightChanged)
    Q_PROPERTY(bool                 supportsTerrainFrame        READ supportsTerrainFrame                                           NOTIFY firmwareTypeChanged)
    Q_PROPERTY(quint64              mavlinkSentCount            READ mavlinkSentCount                                               NOTIFY mavlinkStatusChanged)
    Q_PROPERTY(quint64              mavlinkReceivedCount        READ mavlinkReceivedCount                                           NOTIFY mavlinkStatusChanged)
    Q_PROPERTY(quint64              mavlinkLossCount            READ mavlinkLossCount                                               NOTIFY mavlinkStatusChanged)
    Q_PROPERTY(float                mavlinkLossPercent          READ mavlinkLossPercent                                             NOTIFY mavlinkStatusChanged)

    Q_PROPERTY(quint64              shenHangSentCount           READ shenHangSentCount                                              NOTIFY shenHangStatusChanged)
    Q_PROPERTY(quint64              shenHangReceivedCount       READ shenHangReceivedCount                                          NOTIFY shenHangStatusChanged)
    Q_PROPERTY(quint64              shenHangLossCount           READ shenHangLossCount                                              NOTIFY shenHangStatusChanged)
    Q_PROPERTY(float                shenHangLossPercent         READ shenHangLossPercent                                            NOTIFY shenHangStatusChanged)

    Q_PROPERTY(qreal                gimbalRoll                  READ gimbalRoll                                                     NOTIFY gimbalRollChanged)
    Q_PROPERTY(qreal                gimbalPitch                 READ gimbalPitch                                                    NOTIFY gimbalPitchChanged)
    Q_PROPERTY(qreal                gimbalYaw                   READ gimbalYaw                                                      NOTIFY gimbalYawChanged)
    Q_PROPERTY(bool                 gimbalData                  READ gimbalData                                                     NOTIFY gimbalDataChanged)
    Q_PROPERTY(bool                 isROIEnabled                READ isROIEnabled                                                   NOTIFY isROIEnabledChanged)
    Q_PROPERTY(CheckList            checkListState              READ checkListState             WRITE setCheckListState             NOTIFY checkListStateChanged)
    Q_PROPERTY(bool                 readyToFlyAvailable         READ readyToFlyAvailable                                            NOTIFY readyToFlyAvailableChanged)  ///< true: readyToFly signalling is available on this vehicle
    Q_PROPERTY(bool                 readyToFly                  READ readyToFly                                                     NOTIFY readyToFlyChanged)
    Q_PROPERTY(bool                 allSensorsHealthy           READ allSensorsHealthy                                              NOTIFY allSensorsHealthyChanged)    //< true: all sensors in SYS_STATUS reported as healthy

    // The following properties relate to Orbit status
    Q_PROPERTY(bool             orbitActive     READ orbitActive        NOTIFY orbitActiveChanged)
    Q_PROPERTY(QGCMapCircle*    orbitMapCircle  READ orbitMapCircle     CONSTANT)

    // Vehicle state used for guided control
    Q_PROPERTY(bool     flying                  READ flying                                         NOTIFY flyingChanged)       ///< Vehicle is flying
    Q_PROPERTY(bool     landing                 READ landing                                        NOTIFY landingChanged)      ///< Vehicle is in landing pattern (DO_LAND_START)
    Q_PROPERTY(bool     guidedMode              READ guidedMode                 WRITE setGuidedMode NOTIFY guidedModeChanged)   ///< Vehicle is in Guided mode and can respond to guided commands
    Q_PROPERTY(bool     guidedModeSupported     READ guidedModeSupported                            CONSTANT)                   ///< Guided mode commands are supported by this vehicle
    Q_PROPERTY(bool     pauseVehicleSupported   READ pauseVehicleSupported                          CONSTANT)                   ///< Pause vehicle command is supported
    Q_PROPERTY(bool     orbitModeSupported      READ orbitModeSupported                             CONSTANT)                   ///< Orbit mode is supported by this vehicle
    Q_PROPERTY(bool     roiModeSupported        READ roiModeSupported                               CONSTANT)                   ///< Orbit mode is supported by this vehicle
    Q_PROPERTY(bool     takeoffVehicleSupported READ takeoffVehicleSupported                        CONSTANT)                   ///< Guided takeoff supported
    Q_PROPERTY(QString  gotoFlightMode          READ gotoFlightMode                                 CONSTANT)                   ///< Flight mode vehicle is in while performing goto

    Q_PROPERTY(ParameterManager*        parameterManager    READ parameterManager   CONSTANT)
    Q_PROPERTY(VehicleLinkManager*      vehicleLinkManager  READ vehicleLinkManager CONSTANT)

    // FactGroup object model properties

    Q_PROPERTY(Fact* roll               READ roll               CONSTANT)
    Q_PROPERTY(Fact* pitch              READ pitch              CONSTANT)
    Q_PROPERTY(Fact* heading            READ heading            CONSTANT)
    Q_PROPERTY(Fact* rollRate           READ rollRate           CONSTANT)
    Q_PROPERTY(Fact* pitchRate          READ pitchRate          CONSTANT)
    Q_PROPERTY(Fact* yawRate            READ yawRate            CONSTANT)
    Q_PROPERTY(Fact* groundSpeed        READ groundSpeed        CONSTANT)
    Q_PROPERTY(Fact* airSpeed           READ airSpeed           CONSTANT)
    Q_PROPERTY(Fact* climbRate          READ climbRate          CONSTANT)
    Q_PROPERTY(Fact* altitudeRelative   READ altitudeRelative   CONSTANT)
    Q_PROPERTY(Fact* altitudeAMSL       READ altitudeAMSL       CONSTANT)
    Q_PROPERTY(Fact* flightDistance     READ flightDistance     CONSTANT)
    Q_PROPERTY(Fact* distanceToHome     READ distanceToHome     CONSTANT)
    Q_PROPERTY(Fact* missionItemIndex   READ missionItemIndex   CONSTANT)
    Q_PROPERTY(Fact* headingToNextWP    READ headingToNextWP    CONSTANT)
    Q_PROPERTY(Fact* headingToHome      READ headingToHome      CONSTANT)
    Q_PROPERTY(Fact* distanceToGCS      READ distanceToGCS      CONSTANT)
    Q_PROPERTY(Fact* hobbs              READ hobbs              CONSTANT)
    Q_PROPERTY(Fact* throttlePct        READ throttlePct        CONSTANT)

    Q_PROPERTY(FactGroup*           gps             READ gpsFactGroup               CONSTANT)
    Q_PROPERTY(FactGroup*           clock           READ clockFactGroup             CONSTANT)
    Q_PROPERTY(FactGroup*           setpoint        READ setpointFactGroup          CONSTANT)
//    Q_PROPERTY(QmlObjectListModel*  batteries       READ batteries                  CONSTANT)

    Q_PROPERTY(QString  gitHash                     READ gitHash                    NOTIFY gitHashChanged)
    Q_PROPERTY(quint64  vehicleUID                  READ vehicleUID                 NOTIFY vehicleUIDChanged)
    Q_PROPERTY(QString  vehicleUIDStr               READ vehicleUIDStr              NOTIFY vehicleUIDChanged)

    /// Resets link status counters
    Q_INVOKABLE void resetCounters  ();

    // Called when the message drop-down is invoked to clear current count
    Q_INVOKABLE void resetMessages();

    /// Command vehicle to return to launch
    Q_INVOKABLE void guidedModeRTL(bool smartRTL);

    /// Command vehicle to land at current location
    Q_INVOKABLE void guidedModeLand();

    /// Command vehicle to takeoff from current location
    Q_INVOKABLE void guidedModeTakeoff(double altitudeRelative);

    /// @return The minimum takeoff altitude (relative) for guided takeoff.
    Q_INVOKABLE double minimumTakeoffAltitude();

    /// Command vehicle to move to specified location (altitude is included and relative)
    Q_INVOKABLE void guidedModeGotoLocation(const QGeoCoordinate& gotoCoord);

    /// Command vehicle to change altitude
    ///     @param altitudeChange If > 0, go up by amount specified, if < 0, go down by amount specified
    ///     @param pauseVehicle true: pause vehicle prior to altitude change
    Q_INVOKABLE void guidedModeChangeAltitude(double altitudeChange, bool pauseVehicle);

    /// Command vehicle to pause at current location. If vehicle supports guide mode, vehicle will be left
    /// in guided mode after pause.
    Q_INVOKABLE void pauseVehicle();

    /// Command vehicle to kill all motors no matter what state
    Q_INVOKABLE void emergencyStop();

    /// Command vehicle to abort landing
    Q_INVOKABLE void abortLanding(double climbOutAltitude);

    Q_INVOKABLE void startMission();

    /// Alter the current mission item on the vehicle
    Q_INVOKABLE void setCurrentMissionSequence(int seq);

    /// Reboot vehicle
    Q_INVOKABLE void rebootVehicle();

    /// Clear Messages
    Q_INVOKABLE void clearMessages();

    Q_INVOKABLE void sendPlan(QString planFile);


    Q_INVOKABLE void gimbalControlValue (double pitch, double yaw);
    Q_INVOKABLE void gimbalPitchStep    (int direction);
    Q_INVOKABLE void gimbalYawStep      (int direction);
    Q_INVOKABLE void centerGimbal       ();
    Q_INVOKABLE void forceArm           ();
    /// Removes the vehicle from the system
    Q_INVOKABLE void closeVehicle(void) { _vehicleLinkManager->closeVehicle(); }

    /// Trigger camera using MAV_CMD_DO_DIGICAM_CONTROL command
    Q_INVOKABLE void triggerSimpleCamera(void);

    bool    isInitialConnectComplete() const;
    bool    guidedModeSupported     () const;
    bool    pauseVehicleSupported   () const;
    bool    orbitModeSupported      () const;
    bool    roiModeSupported        () const;
    bool    takeoffVehicleSupported () const;
    QString gotoFlightMode          () const;

    // Property accessors

    QGeoCoordinate coordinate() { return _coordinate; }
    QGeoCoordinate armedPosition    () { return _armedPosition; }

    void updateFlightDistance(double distance);

    // Property accesors
    int id() { return _id; }
    MAV_AUTOPILOT firmwareType() const { return _firmwareType; }
    MAV_TYPE vehicleType() const { return _vehicleType; }
    QGCMAVLink::VehicleClass_t vehicleClass(void) const { return QGCMAVLink::vehicleClass(_vehicleType); }
    Q_INVOKABLE QString vehicleTypeName() const;

    /**
     * @brief sendShenHangMessageOnLinkThreadSafe 通过链接发送消息
     * @param link 与无人机的链接
     * @param message 要发送的消息
     * @return 成功发送为真，链接不存在为假
     */
    bool sendShenHangMessageOnLinkThreadSafe(LinkInterface* link, ShenHangProtocolMessage message);

    /// Provides access to uas from vehicle. Temporary workaround until UAS is fully phased out.

    /// Provides access to uas from vehicle. Temporary workaround until AutoPilotPlugin is fully phased out.
    AutoPilotPlugin* autopilotPlugin() { return _autopilotPlugin; }

    /// Provides access to the Firmware Plugin for this Vehicle
    FirmwarePlugin* firmwarePlugin() { return _firmwarePlugin; }


    QGeoCoordinate homePosition();

    bool armed              () { return _armed; }
    void setArmed           (bool armed, bool showError);
    void setArmedShowError  (bool armed) { setArmed(armed, true); }

    bool flightModeSetAvailable             ();

    QStringList shenHangFlightModes;

    QString shenHangFlightMode{"RcAngle"};
    QStringList flightModes                 ();
    QStringList extraJoystickFlightModes    ();
    QString flightMode                      () const;
    void setFlightMode                      (const QString& flightMode);

    bool airship() const;
    bool fixedWing() const;
    bool multiRotor() const;
    bool vtol() const;
    bool rover() const;
    bool sub() const;

    bool supportsThrottleModeCenterZero () const;
    bool supportsNegativeThrust         ();
    bool supportsRadio                  () const;
    bool supportsJSButton               () const;
    bool supportsMotorInterference      () const;
    bool supportsTerrainFrame           () const;

    void setGuidedMode(bool guidedMode);

    QString prearmError() const { return _prearmError; }
    void setPrearmError(const QString& prearmError);

    QmlObjectListModel* cameraTriggerPoints () { return &_cameraTriggerPoints; }

    int  flowImageIndex() { return _flowImageIndex; }

    typedef enum {
        MessageNone,
        MessageNormal,
        MessageWarning,
        MessageError
    } MessageType_t;

    bool            messageTypeNone             () { return _currentMessageType == MessageNone; }
    bool            messageTypeNormal           () { return _currentMessageType == MessageNormal; }
    bool            messageTypeWarning          () { return _currentMessageType == MessageWarning; }
    bool            messageTypeError            () { return _currentMessageType == MessageError; }
    int             newMessageCount             () { return _currentMessageCount; }
    int             messageCount                () { return _messageCount; }
    QString         formattedMessages           ();
    QString         latestError                 () { return _latestError; }
    float           latitude                    () { return static_cast<float>(_coordinate.latitude()); }
    float           longitude                   () { return static_cast<float>(_coordinate.longitude()); }
    int             rcRSSI                      () { return _rcRSSI; }
    bool            shenHangFirmware            () const { return _firmwareType == MAV_AUTOPILOT_SHEN_HANG; }
    bool            genericFirmware             () const { return _firmwareType == MAV_AUTOPILOT_GENERIC; }
    uint            messagesReceived            () { return _messagesReceived; }
    uint            messagesSent                () { return _messagesSent; }
    uint            messagesLost                () { return _messagesLost; }
    bool            flying                      () const { return _flying; }
    bool            landing                     () const { return _landing; }
    bool            guidedMode                  () const;
    bool            vtolInFwdFlight             () const { return _vtolInFwdFlight; }
    uint8_t         baseMode                    () const { return _base_mode; }
    uint32_t        customMode                  () const { return _custom_mode; }
    bool            isOfflineEditingVehicle     () const { return _offlineEditingVehicle; }
    QString         brandImageIndoor            () const;
    QString         brandImageOutdoor           () const;
    int             sensorsPresentBits          () const { return static_cast<int>(_onboardControlSensorsPresent); }
    int             sensorsEnabledBits          () const { return static_cast<int>(_onboardControlSensorsEnabled); }
    int             sensorsHealthBits           () const { return static_cast<int>(_onboardControlSensorsHealth); }
    int             sensorsUnhealthyBits        () const { return static_cast<int>(_onboardControlSensorsUnhealthy); }
    QString         missionFlightMode           () const;
    QString         pauseFlightMode             () const;
    QString         rtlFlightMode               () const;
    QString         smartRTLFlightMode          () const;
    bool            supportsSmartRTL            () const;
    QString         landFlightMode              () const;
    QString         takeControlFlightMode       () const;
    QString         followFlightMode            () const;
    double          defaultCruiseSpeed          () const { return _defaultCruiseSpeed; }
    double          defaultHoverSpeed           () const { return _defaultHoverSpeed; }
    int             telemetryRRSSI              () { return _telemetryRRSSI; }
    int             telemetryLRSSI              () { return _telemetryLRSSI; }
    unsigned int    telemetryRXErrors           () { return _telemetryRXErrors; }
    unsigned int    telemetryFixed              () { return _telemetryFixed; }
    unsigned int    telemetryTXBuffer           () { return _telemetryTXBuffer; }
    int             telemetryLNoise             () { return _telemetryLNoise; }
    int             telemetryRNoise             () { return _telemetryRNoise; }
    bool            autoDisarm                  ();
    bool            orbitActive                 () const { return _orbitActive; }
    QGCMapCircle*   orbitMapCircle              () { return &_orbitMapCircle; }
    bool            readyToFlyAvailable         () { return _readyToFlyAvailable; }
    bool            readyToFly                  () { return _readyToFly; }
    bool            allSensorsHealthy           () { return _allSensorsHealthy; }
    bool            requiresGpsFix              () const { return static_cast<bool>(_onboardControlSensorsPresent & SysStatusSensorGPS); }

    /// Get the maximum MAVLink protocol version supported
    /// @return the maximum version
    unsigned        maxProtoVersion         () const { return _maxProtoVersion; }

    enum CalibrationType {
        CalibrationRadio,
        CalibrationGyro,
        CalibrationMag,
        CalibrationAccel,
        CalibrationLevel,
        CalibrationEsc,
        CalibrationCopyTrims,
        CalibrationAPMCompassMot,
        CalibrationAPMPressureAirspeed,
        CalibrationAPMPreFlight,
        CalibrationPX4Airspeed,
        CalibrationPX4Pressure,
    };

    void startCalibration   (CalibrationType calType);
    void stopCalibration    (void);

    Fact* roll                              () { return &_rollFact; }
    Fact* pitch                             () { return &_pitchFact; }
    Fact* heading                           () { return &_headingFact; }
    Fact* rollRate                          () { return &_rollRateFact; }
    Fact* pitchRate                         () { return &_pitchRateFact; }
    Fact* yawRate                           () { return &_yawRateFact; }
    Fact* airSpeed                          () { return &_airSpeedFact; }
    Fact* groundSpeed                       () { return &_groundSpeedFact; }
    Fact* climbRate                         () { return &_climbRateFact; }
    Fact* altitudeRelative                  () { return &_altitudeRelativeFact; }
    Fact* altitudeAMSL                      () { return &_altitudeAMSLFact; }
    Fact* flightDistance                    () { return &_flightDistanceFact; }
    Fact* distanceToHome                    () { return &_distanceToHomeFact; }
    Fact* missionItemIndex                  () { return &_missionItemIndexFact; }
    Fact* headingToNextWP                   () { return &_headingToNextWPFact; }
    Fact* headingToHome                     () { return &_headingToHomeFact; }
    Fact* distanceToGCS                     () { return &_distanceToGCSFact; }
    Fact* hobbs                             () { return &_hobbsFact; }
    Fact* throttlePct                       () { return &_throttlePctFact; }

    FactGroup* gpsFactGroup                 () { return &_gpsFactGroup; }
    FactGroup* clockFactGroup               () { return &_clockFactGroup; }
    FactGroup* setpointFactGroup            () { return &_setpointFactGroup; }
    QmlObjectListModel* batteries           () { return &_batteryFactGroupListModel; }

    MissionManager*                 missionManager      () { return _missionManager; }
    GeoFenceManager*                geoFenceManager     () { return _geoFenceManager; }
    ParameterManager*               parameterManager    () { return _parameterManager; }
    ParameterManager*               parameterManager    () const { return _parameterManager; }
    VehicleLinkManager*             vehicleLinkManager  () { return _vehicleLinkManager; }

    static const int cMaxRcChannels = 18;

    typedef enum {
        MavCmdResultCommandResultOnly,          ///< commandResult specifies full success/fail info
        MavCmdResultFailureNoResponseToCommand, ///< No response from vehicle to command
        MavCmdResultFailureDuplicateCommand,    ///< Unable to send command since duplicate is already being waited on for response
    } MavCmdResultFailureCode_t;

    /// Callback for sendMavCommandWithHandler
    ///     @param resultHandleData     Opaque data passed in to sendMavCommand call
    ///     @param commandResult        Ack result for command send
    ///     @param failureCode          Failure reason
    typedef void (*MavCmdResultHandler)(void* resultHandlerData, int compId, MAV_RESULT commandResult, MavCmdResultFailureCode_t failureCode);

    typedef enum {
        RequestMessageNoFailure,
        RequestMessageFailureCommandError,
        RequestMessageFailureCommandNotAcked,
        RequestMessageFailureMessageNotReceived,
        RequestMessageFailureDuplicateCommand,    ///< Unabled to send command since duplicate is already being waited on for response
    } RequestMessageResultHandlerFailureCode_t;

    static const int versionNotSetValue = -1;

    QString gitHash() const { return _gitHash; }
    quint64 vehicleUID() const { return _uid; }
    QString vehicleUIDStr();

    bool soloFirmware() const { return _soloFirmware; }
    void setSoloFirmware(bool soloFirmware);

    int defaultComponentId() { return _defaultComponentId; }

    /// Sets the default component id for an offline editing vehicle
    void setOfflineEditingDefaultComponentId(int defaultComponentId);

    /// @return true: Motors are coaxial like an X8 config, false: Quadcopter for example
    bool coaxialMotors();

    /// @return true: X confiuration, false: Plus configuration
    bool xConfigMotors();

    /// @return Firmware plugin instance data associated with this Vehicle
    QObject* firmwarePluginInstanceData() { return _firmwarePluginInstanceData; }

    /// Sets the firmware plugin instance data associated with this Vehicle. This object will be parented to the Vehicle
    /// and destroyed when the vehicle goes away.
    void setFirmwarePluginInstanceData(QObject* firmwarePluginInstanceData);

    QString vehicleImageOpaque  () const;
    QString vehicleImageOutline () const;
    QString vehicleImageCompass () const;

    const QVariantList&         toolIndicators      ();
    const QVariantList&         modeIndicators      ();
    const QVariantList&         staticCameraList    () const;

    QString                     hobbsMeter          ();

    /// The vehicle is responsible for making the initial request for the Plan.
    /// @return: true: initial request is complete, false: initial request is still in progress;
    bool initialPlanRequestComplete() const { return _initialPlanRequestComplete; }

    void forceInitialPlanRequestComplete();

    void _setFlying(bool flying);
    void _setLanding(bool landing);
    void _setHomePosition(QGeoCoordinate& homeCoord);
    void _setMaxProtoVersion(unsigned version);

    /// Vehicle is about to be deleted
    void prepareDelete();

    quint64     mavlinkSentCount        () { return _mavlinkSentCount; }        /// Calculated total number of messages sent to us
    quint64     mavlinkReceivedCount    () { return _mavlinkReceivedCount; }    /// Total number of sucessful messages received
    quint64     mavlinkLossCount        () { return _mavlinkLossCount; }        /// Total number of lost messages
    float       mavlinkLossPercent      () { return _mavlinkLossPercent; }      /// Running loss rate

    quint64     shenHangSentCount        () { return _shenHangSentCount; }        /// Calculated total number of messages sent to us
    quint64     shenHangReceivedCount    () { return _shenHangReceivedCount; }    /// Total number of sucessful messages received
    quint64     shenHangLossCount        () { return _shenHangLossCount; }        /// Total number of lost messages
    float       shenHangLossPercent      () { return _shenHangLossPercent; }      /// Running loss rate

    qreal       gimbalRoll              () { return static_cast<qreal>(_curGimbalRoll);}
    qreal       gimbalPitch             () { return static_cast<qreal>(_curGimbalPitch); }
    qreal       gimbalYaw               () { return static_cast<qreal>(_curGinmbalYaw); }
    bool        gimbalData              () { return _haveGimbalData; }
    bool        isROIEnabled            () { return _isROIEnabled; }

    CheckList   checkListState          () { return _checkListState; }
    void        setCheckListState       (CheckList cl)  { _checkListState = cl; emit checkListStateChanged(); }

public slots:
    void setVtolInFwdFlight                 (bool vtolInFwdFlight);
//    void _offlineFirmwareTypeSettingChanged (QVariant varFirmwareType); // Should only be used by MissionControler to set firmware from Plan file
//    void _offlineVehicleTypeSettingChanged  (QVariant varVehicleType);  // Should only be used by MissionController to set vehicle type from Plan file

signals:    
    void coordinateChanged              (QGeoCoordinate coordinate);
    void shenHangMessageReceived        (const ShenHangProtocolMessage& message);
    void homePositionChanged            (const QGeoCoordinate& homePosition);
    void armedPositionChanged();
    void armedChanged                   (bool armed);
    void flightModeChanged              (const QString& flightMode);
    void flyingChanged                  (bool flying);
    void landingChanged                 (bool landing);
    void guidedModeChanged              (bool guidedMode);
    void vtolInFwdFlightChanged         (bool vtolInFwdFlight);
    void prearmErrorChanged             (const QString& prearmError);
    void soloFirmwareChanged            (bool soloFirmware);
    void defaultCruiseSpeedChanged      (double cruiseSpeed);
    void defaultHoverSpeedChanged       (double hoverSpeed);
    void firmwareTypeChanged            ();
    void vehicleTypeChanged             ();
    void cameraManagerChanged           ();
    void hobbsMeterChanged              ();
    void capabilitiesKnownChanged       (bool capabilitiesKnown);
    void initialPlanRequestCompleteChanged(bool initialPlanRequestComplete);
    void capabilityBitsChanged          (uint64_t capabilityBits);
    void toolIndicatorsChanged          ();
    void modeIndicatorsChanged          ();
    void textMessageReceived            (int uasid, int componentid, int severity, QString text);
    void checkListStateChanged          ();
    void messagesReceivedChanged        ();
    void messagesSentChanged            ();
    void messagesLostChanged            ();
    void messageTypeChanged             ();
    void newMessageCountChanged         ();
    void messageCountChanged            ();
    void formattedMessagesChanged       ();
    void newFormattedMessage            (QString formattedMessage);
    void latestErrorChanged             ();
    void longitudeChanged               ();
    void currentConfigChanged           ();
    void flowImageIndexChanged          ();
    void rcRSSIChanged                  (int rcRSSI);
    void telemetryRRSSIChanged          (int value);
    void telemetryLRSSIChanged          (int value);
    void telemetryRXErrorsChanged       (unsigned int value);
    void telemetryFixedChanged          (unsigned int value);
    void telemetryTXBufferChanged       (unsigned int value);
    void telemetryLNoiseChanged         (int value);
    void telemetryRNoiseChanged         (int value);
    void autoDisarmChanged              ();
    void flightModesChanged             ();
    void sensorsPresentBitsChanged      (int sensorsPresentBits);
    void sensorsEnabledBitsChanged      (int sensorsEnabledBits);
    void sensorsHealthBitsChanged       (int sensorsHealthBits);
    void sensorsUnhealthyBitsChanged    (int sensorsUnhealthyBits);
    void orbitActiveChanged             (bool orbitActive);
    void readyToFlyAvailableChanged     (bool readyToFlyAvailable);
    void readyToFlyChanged              (bool readyToFy);
    void allSensorsHealthyChanged       (bool allSensorsHealthy);
    void requiresGpsFixChanged          ();

    void firmwareVersionChanged         ();
    void firmwareCustomVersionChanged   ();
    void gitHashChanged                 (QString hash);
    void vehicleUIDChanged              ();
    void loadProgressChanged            (float value);

    /// New RC channel values coming from RC_CHANNELS message
    ///     @param channelCount Number of available channels, cMaxRcChannels max
    ///     @param pwmValues -1 signals channel not available
    void rcChannelsChanged              (int channelCount, int pwmValues[cMaxRcChannels]);

    /// Remote control RSSI changed  (0% - 100%)
    void remoteControlRSSIChanged       (uint8_t rssi);

    // Mavlink Log Download
    void mavlinkLogData                 (Vehicle* vehicle, uint8_t target_system, uint8_t target_component, uint16_t sequence, uint8_t first_message, QByteArray data, bool acked);

    /// Signalled in response to usage of sendMavCommand
    ///     @param vehicleId        Vehicle which command was sent to
    ///     @param targetComponent  Component which command was sent to
    ///     @param command          Command which was sent
    ///     @param ackResult        MAV_RESULT returned in ack
    ///     @param failureCode      More detailed failure code Vehicle::MavCmdResultFailureCode_t
    void mavCommandResult               (int vehicleId, int targetComponent, int command, int ackResult, int failureCode);

    // MAVlink Serial Data
    void mavlinkSerialControl           (uint8_t device, uint8_t flags, uint16_t timeout, uint32_t baudrate, QByteArray data);

    // MAVLink protocol version
    void requestProtocolVersion         (unsigned version);
    void mavlinkStatusChanged           ();
    void shenHangStatusChanged          ();

    void gimbalRollChanged              ();
    void gimbalPitchChanged             ();
    void gimbalYawChanged               ();
    void gimbalDataChanged              ();
    void isROIEnabledChanged            ();
    void initialConnectComplete         ();

private slots:
    void _shenHangMessageReceived           (LinkInterface* link, ShenHangProtocolMessage message);
    void _parametersReady                   (bool parametersReady);
    void _handleFlightModeChanged           (const QString& flightMode);
    void _announceArmedChanged              (bool armed);
    void _offlineCruiseSpeedSettingChanged  (QVariant value);
    void _offlineHoverSpeedSettingChanged   (QVariant value);
    void _handleTextMessage                 (int newCount);
    void _handletextMessageReceived         (UASMessage* message);
    void _imageReady                        (UASInterface* uas);    ///< A new camera image has arrived
    void _prearmErrorTimeout                ();
    void _firstMissionLoadComplete          ();
    void _firstGeoFenceLoadComplete         ();
    void _sendShenHangCommandResponseTimeoutCheck();
    void _clearCameraTriggerPoints          ();
    void _updateDistanceHeadingToHome       ();
    void _updateMissionItemIndex            ();
    void _updateHeadingToNextWP             ();
    void _updateDistanceToGCS               ();
    void _updateHobbsMeter                  ();
    void _vehicleParamLoaded                (bool ready);
    void _shenHangMessageStatus             (int uasId, uint64_t totalSent, uint64_t totalReceived, uint64_t totalLoss, float lossPercent);
    void _trafficUpdate                     (bool alert, QString traffic_id, QString vehicle_id, QGeoCoordinate location, float heading);
    void _orbitTelemetryTimeout             ();
    void _updateFlightTime                  ();
    void _gotProgressUpdate                 (float progressValue);

private:
    void _missionManagerError           (int errorCode, const QString& errorMsg);
    void _geoFenceManagerError          (int errorCode, const QString& errorMsg);
    void _say                           (const QString& text);
    QString _vehicleIdSpeech            ();
    void _commonInit                    ();
    void _setupAutoDisarmSignalling     ();
//    void _setCapabilities               (uint64_t capabilityBits);
    void _updateArmed                   (bool armed);
    void _flightTimerStart              ();
    void _flightTimerStop               ();
    void _chunkedStatusTextTimeout      (void);
    void _chunkedStatusTextCompleted    (uint8_t compId);

    static void _rebootCommandResultHandler(void* resultHandlerData, int compId, MAV_RESULT commandResult, MavCmdResultFailureCode_t failureCode);

    int     _id;                    ///< Mavlink system id
    int     _defaultComponentId;
    bool    _offlineEditingVehicle = false; ///< true: This Vehicle is a "disconnected" vehicle for ui use while offline editing

    MAV_AUTOPILOT       _firmwareType;
    MAV_TYPE            _vehicleType;
    FirmwarePlugin*     _firmwarePlugin = nullptr;
    QObject*            _firmwarePluginInstanceData = nullptr;
    AutoPilotPlugin*    _autopilotPlugin = nullptr;
    ShenHangProtocol*   _shenHangProtocol = nullptr;
    bool                _soloFirmware = false;
    QGCToolbox*         _toolbox = nullptr;
    SettingsManager*    _settingsManager = nullptr;

    QFile               _csvLogFile;

    UAS* _uas = nullptr;

    QGeoCoordinate  _coordinate;
    QGeoCoordinate  _homePosition;
    QGeoCoordinate  _armedPosition;

    int             _currentMessageCount = 0;
    int             _messageCount = 0;
    int             _currentErrorCount = 0;
    int             _currentWarningCount = 0;
    int             _currentNormalCount = 0;
    MessageType_t   _currentMessageType = MessageNone;
    QString         _latestError;
    int             _updateCount = 0;
    int             _rcRSSI = 255;
    double          _rcRSSIstore = 255;
    bool            _flying = false;
    bool            _landing = false;
    bool            _vtolInFwdFlight = false;
    uint32_t        _onboardControlSensorsPresent = 0;
    uint32_t        _onboardControlSensorsEnabled = 0;
    uint32_t        _onboardControlSensorsHealth = 0;
    uint32_t        _onboardControlSensorsUnhealthy = 0;
    bool            _gpsRawIntMessageAvailable              = false;
    bool            _globalPositionIntMessageAvailable      = false;
    bool            _altitudeMessageAvailable               = false;
    double          _defaultCruiseSpeed = qQNaN();
    double          _defaultHoverSpeed = qQNaN();
    int             _telemetryRRSSI = 0;
    int             _telemetryLRSSI = 0;
    uint32_t        _telemetryRXErrors = 0;
    uint32_t        _telemetryFixed = 0;
    uint32_t        _telemetryTXBuffer = 0;
    int             _telemetryLNoise = 0;
    int             _telemetryRNoise = 0;
    bool            _mavlinkProtocolRequestComplete         = false;
    unsigned        _mavlinkProtocolRequestMaxProtoVersion  = 0;
    unsigned        _maxProtoVersion                        = 0;
    bool            _capabilityBitsKnown                    = false;
//    uint64_t        _capabilityBits                         = 0;
    bool            _receivingAttitudeQuaternion = false;
    CheckList       _checkListState                         = CheckListNotSetup;
    bool            _readyToFlyAvailable                    = false;
    bool            _readyToFly                             = false;
    bool            _allSensorsHealthy                      = true;


    QString             _prearmError;
    QTimer              _prearmErrorTimer;
    static const int    _prearmErrorTimeoutMSecs = 35 * 1000;   ///< Take away prearm error after 35 seconds

    bool                _initialPlanRequestComplete = false;

    LinkManager*                    _linkManager                    = nullptr;
    ParameterManager*               _parameterManager               = nullptr;
    FirmwarePluginManager*          _firmwarePluginManager          = nullptr;

    bool    _armed = false;         ///< true: vehicle is armed
    uint8_t _base_mode = 0;     ///< base_mode from HEARTBEAT
    uint32_t _custom_mode = 0;  ///< custom_mode from HEARTBEAT

    static const int _sendMessageMultipleRetries = 5;
    static const int _sendMessageMultipleIntraMessageDelay = 500;

    QElapsedTimer                   _flightTimer;
    QTimer                          _flightTimeUpdater;
    TrajectoryPoints*               _trajectoryPoints = nullptr;
    QmlObjectListModel              _cameraTriggerPoints;
    //QMap<QString, ADSBVehicle*>     _trafficVehicleMap;

    // Toolbox references

    int                         _flowImageIndex = 0;

    bool _allLinksRemovedSent = false; ///< true: allLinkRemoved signal already sent one time

    uint                _messagesReceived = 0;
    uint                _messagesSent = 0;
    uint                _messagesLost = 0;
    uint8_t             _messageSeq = 0;
    uint8_t             _compID = 0;
    bool                _heardFrom = false;

    float               _curGimbalRoll  = 0.0f;
    float               _curGimbalPitch = 0.0f;
    float               _curGinmbalYaw  = 0.0f;
    bool                _haveGimbalData = false;
    bool                _isROIEnabled   = false;
    Joystick*           _activeJoystick = nullptr;

    bool _checkLatestStableFWDone = false;
    int _firmwareMajorVersion = versionNotSetValue;
    int _firmwareMinorVersion = versionNotSetValue;
    int _firmwarePatchVersion = versionNotSetValue;
    int _firmwareCustomMajorVersion = versionNotSetValue;
    int _firmwareCustomMinorVersion = versionNotSetValue;
    int _firmwareCustomPatchVersion = versionNotSetValue;

    QString _gitHash;
    quint64 _uid = 0;

    uint64_t    _mavlinkSentCount       = 0;
    uint64_t    _mavlinkReceivedCount   = 0;
    uint64_t    _mavlinkLossCount       = 0;
    float       _mavlinkLossPercent     = 0.0f;

    uint64_t    _shenHangSentCount       = 0;
    uint64_t    _shenHangReceivedCount   = 0;
    uint64_t    _shenHangLossCount       = 0;
    float       _shenHangLossPercent     = 0.0f;

    float       _loadProgress           = 0.0f;

    QMap<QString, QTime> _noisySpokenPrearmMap; ///< Used to prevent PreArm messages from being spoken too often

    // Orbit status values
    bool            _orbitActive = false;
    QGCMapCircle    _orbitMapCircle;
    QTimer          _orbitTelemetryTimer;
    static const int _orbitTelemetryTimeoutMsecs = 3000; // No telemetry for this amount and orbit will go inactive

    // PID Tuning telemetry mode
    bool            _pidTuningTelemetryMode = false;
    bool            _pidTuningWaitingForRates = false;
    int             _pidTuningNextAdjustIndex = -1;
    QMap<int, int>  _pidTuningMessageRatesUsecs;

    // Chunked status text support
    typedef struct {
        uint16_t    chunkId;
        uint8_t     severity;
        QStringList rgMessageChunks;
    } ChunkedStatusTextInfo_t;
    QMap<uint8_t /* compId */, ChunkedStatusTextInfo_t> _chunkedStatusTextInfoMap;
    QTimer _chunkedStatusTextTimer;
    int                                 _waitForMavlinkMessageId                = 0;

    typedef struct MavCommandListEntry {
        bool                useCommandInt       = false;
        float               rgParam[7]          = { 0 };
        bool                showError           = true;
        bool                requestMessage      = false;                        // true: this is from a requestMessage call
        MavCmdResultHandler resultHandler;
        void*               resultHandlerData   = nullptr;
        int                 maxTries            = _mavCommandMaxRetryCount;
        int                 tryCount            = 0;
        QElapsedTimer       elapsedTimer;
        int                 ackTimeoutMSecs     = _mavCommandAckTimeoutMSecs;
    } MavCommandListEntry_t;

    QList<MavCommandListEntry_t>    _mavCommandList;
    static const int                _mavCommandMaxRetryCount                = 3;
    static const int                _mavCommandResponseCheckTimeoutMSecs    = 500;
    static const int                _mavCommandAckTimeoutMSecs              = 3000;
    static const int                _mavCommandAckTimeoutMSecsHighLatency   = 120000;


    /******************** 沈航协议命令收发相关变量 ********************/
    /**
     * @brief getShenHangCommandName 获取命令名，界面显示用
     * @param tyMsg0
     * @param tyMsg1
     * @return
     */
    QString getShenHangCommandName(uint8_t tyMsg0, uint8_t tyMsg1);

    struct ShenHangCommandListEntry {
        uint8_t tyMsg0;
        uint8_t tyMsg1;
        uint16_t idTarget;      // 目标编号
        bool showError  = true;
        int maxTries  = _shenHangCommandMaxRetryCount;
        int tryCount  = 0;
        QElapsedTimer elapsedTimer;
        int ackTimeoutMSecs  = _shenHangCommandAckTimeoutMSecs;
        uint8_t data[20] = {};
    };

    QList<ShenHangCommandListEntry> _shenHangCommandList;
    QTimer                          _shenHangCommandResponseCheckTimer;
    static const int                _shenHangCommandMaxRetryCount                = 3;
    static const int                _shenHangCommandResponseCheckTimeoutMSecs    = 500;
    static const int                _shenHangCommandAckTimeoutMSecs              = 3000;
    ShenHangProtocolMessage msgSend;
    int  _findShenHangCommandListEntryIndex(uint8_t tyMsg0, uint8_t tyMsg1);
    void _sendShenHangCommandWorker (bool showError = true);
    void _sendShenHangCommandFromList(int index);
    /********************  ********************/

    QMap<uint8_t /* batteryId */, uint8_t /* MAV_BATTERY_CHARGE_STATE_OK */> _lowestBatteryChargeStateAnnouncedMap;

    // FactGroup facts

    Fact _rollFact;
    Fact _pitchFact;
    Fact _headingFact;
    Fact _rollRateFact;
    Fact _pitchRateFact;
    Fact _yawRateFact;
    Fact _groundSpeedFact;
    Fact _airSpeedFact;
    Fact _climbRateFact;
    Fact _altitudeRelativeFact;
    Fact _altitudeAMSLFact;
    Fact _flightDistanceFact;
    Fact _flightTimeFact;
    Fact _distanceToHomeFact;
    Fact _missionItemIndexFact;
    Fact _headingToNextWPFact;
    Fact _headingToHomeFact;
    Fact _distanceToGCSFact;
    Fact _hobbsFact;
    Fact _throttlePctFact;

    VehicleGPSFactGroup             _gpsFactGroup;
    VehicleClockFactGroup           _clockFactGroup;
    VehicleSetpointFactGroup        _setpointFactGroup;
    QmlObjectListModel              _batteryFactGroupListModel;

    MissionManager*                 _missionManager             = nullptr;
    GeoFenceManager*                _geoFenceManager            = nullptr;
    VehicleLinkManager*             _vehicleLinkManager         = nullptr;
    InitialConnectStateMachine*     _initialConnectStateMachine = nullptr;

    static const char* _rollFactName;
    static const char* _pitchFactName;
    static const char* _headingFactName;
    static const char* _rollRateFactName;
    static const char* _pitchRateFactName;
    static const char* _yawRateFactName;
    static const char* _groundSpeedFactName;
    static const char* _airSpeedFactName;
    static const char* _climbRateFactName;
    static const char* _altitudeRelativeFactName;
    static const char* _altitudeAMSLFactName;
    static const char* _flightDistanceFactName;
    static const char* _flightTimeFactName;
    static const char* _distanceToHomeFactName;
    static const char* _missionItemIndexFactName;
    static const char* _headingToNextWPFactName;
    static const char* _headingToHomeFactName;
    static const char* _distanceToGCSFactName;
    static const char* _hobbsFactName;
    static const char* _throttlePctFactName;

    static const char* _gpsFactGroupName;
    static const char* _windFactGroupName;
    static const char* _vibrationFactGroupName;
    static const char* _temperatureFactGroupName;
    static const char* _clockFactGroupName;
    static const char* _setpointFactGroupName;
    static const char* _distanceSensorFactGroupName;
    static const char* _escStatusFactGroupName;
    static const char* _estimatorStatusFactGroupName;

    static const int _vehicleUIUpdateRateMSecs      = 100;

    // Settings keys
    static const char* _settingsGroup;
    static const char* _joystickEnabledSettingsKey;

private:
    #define MAX_SEND_BUFFER_LENGTH 64
    uint16_t idSource = 255;
    uint16_t idTarget = 1;
    uint8_t counter = 0;
    GpsRawInt gpsRawInt = {};
    uint8_t bufferSend[MAX_SEND_BUFFER_LENGTH] = {};
public:
    /******************** 解析数据包中的数据 ********************/
    /******************** 常规报文 ********************/
    /**
     * @brief HandleWaypointInfo 解析航路点信息报文（ty_msg0=1）
     * @param msg
     */
    void HandleWaypointInfo(ShenHangProtocolMessage& msg);

    /**
     * @brief HandleGeneralStatus 解析常规状态信息报文（ty_msg0=2）
     * @param msg
     */
    void HandleGeneralStatus(ShenHangProtocolMessage& msg);

    /**
     * @brief HandlePosVelTime 解析位置-速度-时间（ty_msg0=3）
     * @param msg
     */
    void HandlePosVelTime(ShenHangProtocolMessage& msg);

    /**
     * @brief HandleAttitude 解析姿态信息（ty_msg0=4）
     * @param msg
     */
    void HandleAttitude(ShenHangProtocolMessage& msg);

    /**
     * @brief HandleGeneralData 解析一般数据（ty_msg0=5）
     * @param msg
     */
    void HandleGeneralData(ShenHangProtocolMessage& msg);




    /******************** 无人机应答报文 ********************/
    /**
     * @brief HandleAckReset 待定 载具操纵命令回复（ty_msg0=192）
     * @param msg
     */
    void HandleAckReset(ShenHangProtocolMessage& msg);

    /**
     * @brief HandleAckCommandBank 该航线相关命令（ty_msg0=130）的返回报文
     * @param msg
     */
    void HandleAckCommandBank(ShenHangProtocolMessage& msg);

    /******************** 命令报文 ********************/
    /**
     * @brief PackVehicleCommand 载具操纵命令（ty_msg0=128）具体内容未定，暂且保留
     */
    void PackVehicleCommand();
#if 0 // WARNING To be deleted
    /**
     * @brief PackSetParamCommand 设置相关命令（ty_msg0=129）
     * @param msg
     * @param typeMsg1
     * @param idGroup
     * @param length
     * @param addrOffset
     * @param data
     * @param dataLength
     */
    void PackSetParamCommand(ShenHangProtocolMessage& msg, CommandParamType typeMsg1, uint8_t idGroup, uint8_t length = 0, uint16_t addrOffset = 0, uint8_t* data = nullptr, uint8_t dataLength = 0);

    /**
     * @brief PackCommandParamReset 重置设置（ty_msg0=129,ty_msg1=0）
     * @param msg 传出参数
     * @param idGroup 0~0xFE对应各设置组id；0xFF重置所有设置组
     */
    void PackCommandParamReset(ShenHangProtocolMessage& msg, uint8_t idGroup);

    /**
     * @brief PackCommandParamLoad 载入设置（ty_msg0=129,ty_msg1=1）
     * @param msg 传出参数
     * @param idGroup 0~0xFE对应各设置组id；0xFF载入所有设置组
     */
    void PackCommandParamLoad(ShenHangProtocolMessage& msg, uint8_t idGroup);

    /**
     * @brief PackCommandParamSave 保存设置（ty_msg0=129,ty_msg1=2）
     * @param msg 传出参数
     * @param idGroup 0~0xFE对应各设置组id；0xFF保存所有设置组
     */
    void PackCommandParamSave(ShenHangProtocolMessage& msg, uint8_t idGroup);

    /**
     * @brief PackCommandParamQuery 查询设置（ty_msg0=129,ty_msg1=3）
     * @param msg 传出参数
     * @param idGroup 0~0xFE对应各设置组id
     * @param length 要查询的设置参数的字节数，根据xml文件确定
     * @param addrOffset 要查询的设置参数在对应组内的偏移地址，根据xml文件确定
     */
    void PackCommandParamQuery(ShenHangProtocolMessage& msg, uint8_t idGroup, uint8_t length, uint16_t addrOffset);

    /**
     * @brief PackCommandSet  设置参数（ty_msg0=129,ty_msg1=4）
     * @param msg 传出参数
     * @param idGroup  0~0xFE对应各设置组id
     * @param length 要设置的参数的字节数，根据xml文件确定
     * @param addrOffset 要设置的参数在对应组内的偏移地址，根据xml文件确定
     * @param data 具体设置值，按内存顺序排列，低字节在前，高字节在后
     */
    void PackCommandSet(ShenHangProtocolMessage& msg, uint8_t idGroup, uint8_t length, uint16_t addrOffset, uint8_t* data);

    /******************** 航线相关命令（ty_msg0=130） ********************/
    /**
     * @brief PackCommandBankQueryAll 查询整体航路信息（ty_msg0=130,ty_msg1=0）
     * @param msg
     */
    void PackCommandBankQueryAll(ShenHangProtocolMessage& msg);

    /**
     * @brief PackCommandBankQuerySingle 查询单个bank信息（ty_msg0=130,ty_msg1=1）
     * @param msg
     * @param idBank
     */
    void PackCommandBankQuerySingle(ShenHangProtocolMessage& msg, uint16_t idBank);

    /**
     * @brief PackCommandBankSetSingle 设置单个bank（ty_msg0=130,ty_msg1=2）
     * @param msg 传出参数
     * @param idBank 对应需要设置的bank编号
     * @param idBankSuc 对应接续航线bank编号
     * @param idBankIwpSuc 对应接续航线中航点编号
     * @param actBankEnd 设置航线完成动作，暂未定义，保留
     * @param flagBankVerified 设置航线校验位，0：校验未通过，1：校验通过
     * @param flagBankLock 设置航线锁定位，0：解除锁定，1：设置锁定
     */
    void PackCommandBankSetSingle(ShenHangProtocolMessage& msg, uint16_t idBank, uint16_t idBankSuc, uint16_t idBankIwpSuc, uint16_t actBankEnd, uint8_t flagBankVerified, uint8_t flagBankLock);

    /**
     * @brief PackRefactorInfoSlot 重构infoslot表单（ty_msg0=130,ty_msg1=3）
     * @param msg 传出参数
     * @param idBank 对应需要重构的bank编号
     * @param nWp 对应bank内的航路点数目
     * @param nInfoSlot 对应bank内的infoslot数目
     */
    void PackRefactorInfoSlot(ShenHangProtocolMessage& msg, uint16_t idBank, uint16_t nWp, uint16_t nInfoSlot);

    /**
     * @brief PackQuerySingleInfoSlot 查询单个infoslot（ty_msg0=130,ty_msg1=4）
     * @param msg 传出参数
     * @param idBank 对应要查询的infoslot所在的bank编号
     * @param idInfoSlot 对应需要查询的infoslot编号
     */
    void PackQuerySingleInfoSlot(ShenHangProtocolMessage& msg, uint16_t idBank, uint16_t idInfoSlot);
#endif
    /**
     * @brief PackEnableBankAutoSw 航线自动跳转控制命令（ty_msg0=130,ty_msg1=5）
     * @param msg 传出参数
     * @param flagHalt 0：航路点自动切换使能，1：航路点自动切换失能
     */
    void PackEnableBankAutoSw(ShenHangProtocolMessage& msg, uint8_t flagHalt);

    /**
     * @brief PackWaypointAutoSw 航路点跳转命令（ty_msg0=130,ty_msg1=6）
     * @param idBank 对应跳转目标bank编号
     * @param idWp 对应跳转目标bank内的路点
     * @param switchMode 跳转时机，0：立即跳转，1：满足当前航点切换条件后跳转
     */
    void PackWaypointAutoSw(uint16_t idBank, uint16_t idWp, uint8_t switchMode);
};

Q_DECLARE_METATYPE(Vehicle::MavCmdResultFailureCode_t)
