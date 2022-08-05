/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include <QTime>
#include <QDateTime>
#include <QLocale>
#include <QQuaternion>

#include <Eigen/Eigen>

#include "Vehicle.h"
#include "FirmwarePluginManager.h"
#include "LinkManager.h"
#include "FirmwarePlugin.h"
#include "MissionManager.h"
#include "MissionController.h"
#include "PlanMasterController.h"
#include "GeoFenceManager.h"
#include "QGCApplication.h"
#include "QGCImageProvider.h"
//#include "MissionCommandTree.h"
#include "SettingsManager.h"
#include "QGCQGeoCoordinate.h"
#include "QGCCorePlugin.h"
#include "QGCOptions.h"
//#include "QGCCameraManager.h"
#include "VideoReceiver.h"
#include "VideoManager.h"
#include "VideoSettings.h"
#include "PositionManager.h"
#include "TrajectoryPoints.h"
#include "QGCGeo.h"
#include "ParameterManager.h"
#include "InitialConnectStateMachine.h"
//#include "VehicleBatteryFactGroup.h"

#if defined(QGC_AIRMAP_ENABLED)
#include "AirspaceVehicleManager.h"
#endif

QGC_LOGGING_CATEGORY(VehicleLog, "VehicleLog")

#define UPDATE_TIMER 50
#define DEFAULT_LAT  38.965767f
#define DEFAULT_LON -120.083923f

const QString guided_mode_not_supported_by_vehicle = QObject::tr("Guided mode not supported by Vehicle.");

const char* Vehicle::_settingsGroup =               "Vehicle%1";        // %1 replaced with mavlink system id
const char* Vehicle::_joystickEnabledSettingsKey =  "JoystickEnabled";

const char* Vehicle::_rollFactName =                "roll";
const char* Vehicle::_pitchFactName =               "pitch";
const char* Vehicle::_headingFactName =             "heading";
const char* Vehicle::_rollRateFactName =             "rollRate";
const char* Vehicle::_pitchRateFactName =           "pitchRate";
const char* Vehicle::_yawRateFactName =             "yawRate";
const char* Vehicle::_airSpeedFactName =            "airSpeed";
const char* Vehicle::_groundSpeedFactName =         "groundSpeed";
const char* Vehicle::_climbRateFactName =           "climbRate";
const char* Vehicle::_altitudeRelativeFactName =    "altitudeRelative";
const char* Vehicle::_altitudeAMSLFactName =        "altitudeAMSL";
const char* Vehicle::_flightDistanceFactName =      "flightDistance";
const char* Vehicle::_flightTimeFactName =          "flightTime";
const char* Vehicle::_distanceToHomeFactName =      "distanceToHome";
const char* Vehicle::_missionItemIndexFactName =    "missionItemIndex";
const char* Vehicle::_headingToNextWPFactName =     "headingToNextWP";
const char* Vehicle::_headingToHomeFactName =       "headingToHome";
const char* Vehicle::_distanceToGCSFactName =       "distanceToGCS";
const char* Vehicle::_hobbsFactName =               "hobbs";
const char* Vehicle::_throttlePctFactName =         "throttlePct";

const char* Vehicle::_gpsFactGroupName =                "gps";
const char* Vehicle::_windFactGroupName =               "wind";
const char* Vehicle::_vibrationFactGroupName =          "vibration";
const char* Vehicle::_temperatureFactGroupName =        "temperature";
const char* Vehicle::_clockFactGroupName =              "clock";
const char* Vehicle::_setpointFactGroupName =           "setpoint";
const char* Vehicle::_distanceSensorFactGroupName =     "distanceSensor";
const char* Vehicle::_escStatusFactGroupName =          "escStatus";
const char* Vehicle::_estimatorStatusFactGroupName =    "estimatorStatus";

static const struct Bit2Name FlightMode2Name[] = {
    { FlyMode::RC_ANGULAR_VEL,             "RcAngularVel" },
    { FlyMode::RC_ANGLE,                   "RcAngle" },
    { FlyMode::RADIO_ANGULAR_VEL,          "RadioAngularVel" },
    { FlyMode::RADIO_ANGLE,                "RadioAngle" },
    { FlyMode::ALTITUDE_RC_ANGLE,          "AltitudeRcAngle" },
    { FlyMode::ALTITUDE_RADIO_ANGLE,       "AltitudeRadioAngle" },
    { FlyMode::ALTITUDE_RC_SPEED_HEAD,     "AltitudeRcSpeedHead" },
    { FlyMode::ALTITUDE_RADIO_SPEED_HEAD,  "AltitudeRadioSpeedHead" },
    { FlyMode::PROGRAM_CONTROL,            "ProgramControl" }
};

// Standard connected vehicle
Vehicle::Vehicle(LinkInterface*             link,
                 int                        vehicleId,
                 MAV_AUTOPILOT              firmwareType,
                 MAV_TYPE                   vehicleType,
                 FirmwarePluginManager*     firmwarePluginManager)
    : FactGroup                     (_vehicleUIUpdateRateMSecs, ":/json/Vehicle/VehicleFact.json")
    , _id                           (vehicleId)
    , _firmwareType                 (firmwareType)
    , _vehicleType                  (vehicleType)
    , _toolbox                      (qgcApp()->toolbox())
    , _settingsManager              (_toolbox->settingsManager())
    , _defaultCruiseSpeed           (_settingsManager->appSettings()->offlineEditingCruiseSpeed()->rawValue().toDouble())
    , _defaultHoverSpeed            (_settingsManager->appSettings()->offlineEditingHoverSpeed()->rawValue().toDouble())
    , _firmwarePluginManager        (firmwarePluginManager)
    , _trajectoryPoints             (new TrajectoryPoints(this, this))
    , _rollFact                     (_rollFactName,              FactMetaData::valueTypeDouble)
    , _pitchFact                    (_pitchFactName,             FactMetaData::valueTypeDouble)
    , _headingFact                  (_headingFactName,           FactMetaData::valueTypeDouble)
    , _rollRateFact                 (_rollRateFactName,          FactMetaData::valueTypeDouble)
    , _pitchRateFact                (_pitchRateFactName,         FactMetaData::valueTypeDouble)
    , _yawRateFact                  (_yawRateFactName,           FactMetaData::valueTypeDouble)
    , _groundSpeedFact              (_groundSpeedFactName,       FactMetaData::valueTypeDouble)
    , _airSpeedFact                 (_airSpeedFactName,          FactMetaData::valueTypeDouble)
    , _climbRateFact                (_climbRateFactName,         FactMetaData::valueTypeDouble)
    , _altitudeRelativeFact         (_altitudeRelativeFactName,  FactMetaData::valueTypeDouble)
    , _altitudeAMSLFact             (_altitudeAMSLFactName,      FactMetaData::valueTypeDouble)
    , _flightDistanceFact           (_flightDistanceFactName,    FactMetaData::valueTypeDouble)
    , _flightTimeFact               (_flightTimeFactName,        FactMetaData::valueTypeElapsedTimeInSeconds)
    , _distanceToHomeFact           (_distanceToHomeFactName,    FactMetaData::valueTypeDouble)
    , _missionItemIndexFact         (_missionItemIndexFactName,  FactMetaData::valueTypeUint16)
    , _headingToNextWPFact          (_headingToNextWPFactName,   FactMetaData::valueTypeDouble)
    , _headingToHomeFact            (_headingToHomeFactName,     FactMetaData::valueTypeDouble)
    , _distanceToGCSFact            (_distanceToGCSFactName,     FactMetaData::valueTypeDouble)
    , _hobbsFact                    (_hobbsFactName,             FactMetaData::valueTypeString)
    , _throttlePctFact              (_throttlePctFactName,       FactMetaData::valueTypeUint16)
    , _gpsFactGroup                 (this)
    , _clockFactGroup               (this)
    , _setpointFactGroup            (this)
{
    qCDebug(VehicleLog) << "Vehicle::Vehicle()" << this->id() << this;
    _linkManager = _toolbox->linkManager();

    qCDebug(VehicleLog) << "vehicle() _shenHangProtocol" << _shenHangProtocol;
    _shenHangProtocol = _toolbox->shenHangProtocol();
    qCDebug(VehicleLog) << "vehicle() _shenHangProtocol" << _shenHangProtocol;
    qCDebug(VehicleLog) << "vehicle() _shenHangProtocol getSystemId" << _shenHangProtocol->getSystemId();

    connect(_shenHangProtocol, &ShenHangProtocol::shenHangMessageReceived, this, &Vehicle::_shenHangMessageReceived);
    connect(_shenHangProtocol, &ShenHangProtocol::shenHangMessageStatus,   this, &Vehicle::_shenHangMessageStatus);

    connect(this, &Vehicle::flightModeChanged,          this, &Vehicle::_handleFlightModeChanged);
    connect(this, &Vehicle::armedChanged,               this, &Vehicle::_announceArmedChanged);

    connect(_toolbox->multiVehicleManager(), &MultiVehicleManager::parameterReadyVehicleAvailableChanged, this, &Vehicle::_vehicleParamLoaded);


    _commonInit();

    _vehicleLinkManager->_addLink(link);

    // Set video stream to udp if running ArduSub and Video is disabled
    if (sub() && _settingsManager->videoSettings()->videoSource()->rawValue() == VideoSettings::videoDisabled) {
        _settingsManager->videoSettings()->videoSource()->setRawValue(VideoSettings::videoSourceUDPH264);
        _settingsManager->videoSettings()->lowLatencyMode()->setRawValue(true);
    }

    _autopilotPlugin = _firmwarePlugin->autopilotPlugin(this);
    _autopilotPlugin->setParent(this);

    // PreArm Error self-destruct timer
    connect(&_prearmErrorTimer, &QTimer::timeout, this, &Vehicle::_prearmErrorTimeout);
    _prearmErrorTimer.setInterval(_prearmErrorTimeoutMSecs);
    _prearmErrorTimer.setSingleShot(true);

    // Send ShenHang ack timer
    _shenHangCommandResponseCheckTimer.setSingleShot(false);
    _shenHangCommandResponseCheckTimer.setInterval(_shenHangCommandResponseCheckTimeoutMSecs);
    _shenHangCommandResponseCheckTimer.start();
    connect(&_shenHangCommandResponseCheckTimer, &QTimer::timeout, this, &Vehicle::_sendShenHangCommandResponseTimeoutCheck);

    // Chunked status text timeout timer
    _chunkedStatusTextTimer.setSingleShot(true);
    _chunkedStatusTextTimer.setInterval(1000);
    connect(&_chunkedStatusTextTimer, &QTimer::timeout, this, &Vehicle::_chunkedStatusTextTimeout);

    // MAV_TYPE_GENERIC is used by unit test for creating a vehicle which doesn't do the connect sequence. This
    // way we can test the methods that are used within the connect sequence.
    if (!qgcApp()->runningUnitTests() || _vehicleType != MAV_TYPE_GENERIC) {
        _initialConnectStateMachine->start();
    }

    _firmwarePlugin->initializeVehicle(this);
    for(auto& factName: factNames()) {
        _firmwarePlugin->adjustMetaData(vehicleType, getFact(factName)->metaData());
    }

    connect(&_orbitTelemetryTimer, &QTimer::timeout, this, &Vehicle::_orbitTelemetryTimeout);

    for (size_t i = 0; i < sizeof(FlightMode2Name) / sizeof(FlightMode2Name[0]); i++)
        shenHangFlightModes << FlightMode2Name[i].name;
    emit flightModesChanged();

//    PackWaypointAutoSw(1, 2, 3);
//    setArmed(true, true);
}

// Disconnected Vehicle for offline editing
Vehicle::Vehicle(MAV_AUTOPILOT              firmwareType,
                 MAV_TYPE                   vehicleType,
                 FirmwarePluginManager*     firmwarePluginManager,
                 QObject*                   parent)
    : FactGroup                         (_vehicleUIUpdateRateMSecs, ":/json/Vehicle/VehicleFact.json", parent)
    , _id                               (0)
    , _offlineEditingVehicle            (true)
    , _firmwareType                     (firmwareType)
    , _vehicleType                      (vehicleType)
    , _toolbox                          (qgcApp()->toolbox())
    , _settingsManager                  (_toolbox->settingsManager())
    , _defaultCruiseSpeed               (_settingsManager->appSettings()->offlineEditingCruiseSpeed()->rawValue().toDouble())
    , _defaultHoverSpeed                (_settingsManager->appSettings()->offlineEditingHoverSpeed()->rawValue().toDouble())
    , _mavlinkProtocolRequestComplete   (true)
    , _maxProtoVersion                  (200)
    , _capabilityBitsKnown              (true)
    , _firmwarePluginManager            (firmwarePluginManager)
    , _trajectoryPoints                 (new TrajectoryPoints(this, this))
    , _rollFact                         (_rollFactName,              FactMetaData::valueTypeDouble)
    , _pitchFact                        (_pitchFactName,             FactMetaData::valueTypeDouble)
    , _headingFact                      (_headingFactName,           FactMetaData::valueTypeDouble)
    , _rollRateFact                     (_rollRateFactName,          FactMetaData::valueTypeDouble)
    , _pitchRateFact                    (_pitchRateFactName,         FactMetaData::valueTypeDouble)
    , _yawRateFact                      (_yawRateFactName,           FactMetaData::valueTypeDouble)
    , _groundSpeedFact                  (_groundSpeedFactName,       FactMetaData::valueTypeDouble)
    , _airSpeedFact                     (_airSpeedFactName,          FactMetaData::valueTypeDouble)
    , _climbRateFact                    (_climbRateFactName,         FactMetaData::valueTypeDouble)
    , _altitudeRelativeFact             (_altitudeRelativeFactName,  FactMetaData::valueTypeDouble)
    , _altitudeAMSLFact                 (_altitudeAMSLFactName,      FactMetaData::valueTypeDouble)
    , _flightDistanceFact               (_flightDistanceFactName,    FactMetaData::valueTypeDouble)
    , _flightTimeFact                   (_flightTimeFactName,        FactMetaData::valueTypeElapsedTimeInSeconds)
    , _distanceToHomeFact               (_distanceToHomeFactName,    FactMetaData::valueTypeDouble)
    , _missionItemIndexFact             (_missionItemIndexFactName,  FactMetaData::valueTypeUint16)
    , _headingToNextWPFact              (_headingToNextWPFactName,   FactMetaData::valueTypeDouble)
    , _headingToHomeFact                (_headingToHomeFactName,     FactMetaData::valueTypeDouble)
    , _distanceToGCSFact                (_distanceToGCSFactName,     FactMetaData::valueTypeDouble)
    , _hobbsFact                        (_hobbsFactName,             FactMetaData::valueTypeString)
    , _throttlePctFact                  (_throttlePctFactName,       FactMetaData::valueTypeUint16)
    , _gpsFactGroup                     (this)
    , _clockFactGroup                   (this)
{
    qCDebug(VehicleLog) << "Disconnected Vehicle for offline editing";
    _linkManager = _toolbox->linkManager();
    _commonInit();
    _firmwarePlugin->initializeVehicle(this);
}

void Vehicle::_commonInit()
{
    _firmwarePlugin = _firmwarePluginManager->firmwarePluginForAutopilot(_firmwareType, _vehicleType);

    connect(_firmwarePlugin, &FirmwarePlugin::toolIndicatorsChanged, this, &Vehicle::toolIndicatorsChanged);
    connect(_firmwarePlugin, &FirmwarePlugin::modeIndicatorsChanged, this, &Vehicle::modeIndicatorsChanged);

    connect(this, &Vehicle::coordinateChanged,      this, &Vehicle::_updateDistanceHeadingToHome);
    connect(this, &Vehicle::coordinateChanged,      this, &Vehicle::_updateDistanceToGCS);
    connect(this, &Vehicle::homePositionChanged,    this, &Vehicle::_updateDistanceHeadingToHome);
    connect(this, &Vehicle::hobbsMeterChanged,      this, &Vehicle::_updateHobbsMeter);

    connect(_toolbox->qgcPositionManager(), &QGCPositionManager::gcsPositionChanged, this, &Vehicle::_updateDistanceToGCS);

    _missionManager = new MissionManager(this);
    connect(_missionManager, &MissionManager::error,                    this, &Vehicle::_missionManagerError);
    connect(_missionManager, &MissionManager::newMissionItemsAvailable, this, &Vehicle::_firstMissionLoadComplete);
    connect(_missionManager, &MissionManager::newMissionItemsAvailable, this, &Vehicle::_clearCameraTriggerPoints);
    connect(_missionManager, &MissionManager::sendComplete,             this, &Vehicle::_clearCameraTriggerPoints);
    connect(_missionManager, &MissionManager::currentIndexChanged,      this, &Vehicle::_updateHeadingToNextWP);
    connect(_missionManager, &MissionManager::currentIndexChanged,      this, &Vehicle::_updateMissionItemIndex);

    connect(_missionManager, &MissionManager::sendComplete,             _trajectoryPoints, &TrajectoryPoints::clear);
    connect(_missionManager, &MissionManager::newMissionItemsAvailable, _trajectoryPoints, &TrajectoryPoints::clear);

    _initialConnectStateMachine     = new InitialConnectStateMachine    (this);
    _vehicleLinkManager             = new VehicleLinkManager            (this);

    _parameterManager = new ParameterManager(this);
    connect(_parameterManager, &ParameterManager::parametersReadyChanged, this, &Vehicle::_parametersReady);

    connect(_parameterManager, &ParameterManager::loadProgressChanged, this, &Vehicle::_gotProgressUpdate);
    // GeoFenceManager needs to access ParameterManager so make sure to create after
    _geoFenceManager = new GeoFenceManager(this);
    connect(_geoFenceManager, &GeoFenceManager::error,          this, &Vehicle::_geoFenceManagerError);
    connect(_geoFenceManager, &GeoFenceManager::loadComplete,   this, &Vehicle::_firstGeoFenceLoadComplete);

    // Flight modes can differ based on advanced mode
    connect(_toolbox->corePlugin(), &QGCCorePlugin::showAdvancedUIChanged, this, &Vehicle::flightModesChanged);

    // Build FactGroup object model

    _addFact(&_rollFact,                _rollFactName);
    _addFact(&_pitchFact,               _pitchFactName);
    _addFact(&_headingFact,             _headingFactName);
    _addFact(&_rollRateFact,            _rollRateFactName);
    _addFact(&_pitchRateFact,           _pitchRateFactName);
    _addFact(&_yawRateFact,             _yawRateFactName);
    _addFact(&_groundSpeedFact,         _groundSpeedFactName);
    _addFact(&_airSpeedFact,            _airSpeedFactName);
    _addFact(&_climbRateFact,           _climbRateFactName);
    _addFact(&_altitudeRelativeFact,    _altitudeRelativeFactName);
    _addFact(&_altitudeAMSLFact,        _altitudeAMSLFactName);
    _addFact(&_flightDistanceFact,      _flightDistanceFactName);
    _addFact(&_flightTimeFact,          _flightTimeFactName);
    _addFact(&_distanceToHomeFact,      _distanceToHomeFactName);
    _addFact(&_missionItemIndexFact,    _missionItemIndexFactName);
    _addFact(&_headingToNextWPFact,     _headingToNextWPFactName);
    _addFact(&_headingToHomeFact,       _headingToHomeFactName);
    _addFact(&_distanceToGCSFact,       _distanceToGCSFactName);
    _addFact(&_throttlePctFact,         _throttlePctFactName);

    _hobbsFact.setRawValue(QVariant(QString("0000:00:00")));
    _addFact(&_hobbsFact,               _hobbsFactName);

    _addFactGroup(&_gpsFactGroup,               _gpsFactGroupName);
    _addFactGroup(&_clockFactGroup,             _clockFactGroupName);
    _addFactGroup(&_setpointFactGroup,          _setpointFactGroupName);

    // Add firmware-specific fact groups, if provided
    QMap<QString, FactGroup*>* fwFactGroups = _firmwarePlugin->factGroups();
    if (fwFactGroups) {
        QMapIterator<QString, FactGroup*> i(*fwFactGroups);
        while(i.hasNext()) {
            i.next();
            _addFactGroup(i.value(), i.key());
        }
    }

    _flightDistanceFact.setRawValue(0);
    _flightTimeFact.setRawValue(0);
    _flightTimeUpdater.setInterval(1000);
    _flightTimeUpdater.setSingleShot(false);
    connect(&_flightTimeUpdater, &QTimer::timeout, this, &Vehicle::_updateFlightTime);

    // Set video stream to udp if running ArduSub and Video is disabled
    if (sub() && _settingsManager->videoSettings()->videoSource()->rawValue() == VideoSettings::videoDisabled) {
        _settingsManager->videoSettings()->videoSource()->setRawValue(VideoSettings::videoSourceUDPH264);
        _settingsManager->videoSettings()->lowLatencyMode()->setRawValue(true);
    }
}

Vehicle::~Vehicle()
{
    qCDebug(VehicleLog) << "~Vehicle" << this;

    delete _missionManager;
    _missionManager = nullptr;

    delete _autopilotPlugin;
    _autopilotPlugin = nullptr;

#if defined(QGC_AIRMAP_ENABLED)
    if (_airspaceVehicleManager) {
        delete _airspaceVehicleManager;
    }
#endif
}

void Vehicle::prepareDelete()
{
//    if(_cameraManager) {
//        // because of _cameraManager QML bindings check for nullptr won't work in the binding pipeline
//        // the dangling pointer access will cause a runtime fault
//        auto tmpCameras = _cameraManager;
//        _cameraManager = nullptr;
//        delete tmpCameras;
//        emit cameraManagerChanged();
//        qApp->processEvents();
//    }
}

//void Vehicle::_offlineFirmwareTypeSettingChanged(QVariant varFirmwareType)
//{
//    _firmwareType = static_cast<MAV_AUTOPILOT>(varFirmwareType.toInt());
//    _firmwarePlugin = _firmwarePluginManager->firmwarePluginForAutopilot(_firmwareType, _vehicleType);
//    if (_firmwareType == MAV_AUTOPILOT_ARDUPILOTMEGA) {
//        _capabilityBits |= MAV_PROTOCOL_CAPABILITY_TERRAIN;
//    } else {
//        _capabilityBits &= ~MAV_PROTOCOL_CAPABILITY_TERRAIN;
//    }
//    emit firmwareTypeChanged();
//    emit capabilityBitsChanged(_capabilityBits);
//}

//void Vehicle::_offlineVehicleTypeSettingChanged(QVariant varVehicleType)
//{
//    _vehicleType = static_cast<MAV_TYPE>(varVehicleType.toInt());
//    emit vehicleTypeChanged();
//}

void Vehicle::_offlineCruiseSpeedSettingChanged(QVariant value)
{
    _defaultCruiseSpeed = value.toDouble();
    emit defaultCruiseSpeedChanged(_defaultCruiseSpeed);
}

void Vehicle::_offlineHoverSpeedSettingChanged(QVariant value)
{
    _defaultHoverSpeed = value.toDouble();
    emit defaultHoverSpeedChanged(_defaultHoverSpeed);
}

void Vehicle::resetCounters()
{
    _messagesReceived   = 0;
    _messagesSent       = 0;
    _messagesLost       = 0;
    _messageSeq         = 0;
    _heardFrom          = false;
}

void Vehicle::_shenHangMessageReceived(LinkInterface* link, ShenHangProtocolMessage message)
{
    // We give the link manager first whack since it it reponsible for adding new links
    _vehicleLinkManager->shenHangMessageReceived(link, message);
    idTarget = message.idSource;
    _parameterManager->shenHangMessageReceived(message);
    switch (message.tyMsg0)
    {
    case WAYPOINT_INFO_SLOT:
        HandleWaypointInfo(message);
        break;
    case GENERAL_STATUS:
        HandleGeneralStatus(message);
        break;
    case POS_VEL_TIME:
        HandlePosVelTime(message);
        break;
    case ATTITUDE:
        HandleAttitude(message);
        break;
    case GENERAL_DATA:
        HandleGeneralData(message);
        break;

    /******************** 回复报文编号 ********************/
    case ACK_COMMAND_VEHICLE:
        break;
    case ACK_COMMAND_PARAM:
        HandleAckCommandParam(message);
        break;
    case ACK_COMMAND_BANK:
        HandleAckCommandBank(message);
        break;
    case ACK_COMMAND_PAYLOAD:
        break;
    case ACK_COMMAND_REMOTE_CONTROL:
        break;
    case ACK_COMMAND_COMM:
        break;
    case ACK_COMMAND_ORGANIZE:
        break;
    default:
        break;
    }
    emit shenHangMessageReceived(message);
}

void Vehicle::_orbitTelemetryTimeout()
{
    _orbitActive = false;
    emit orbitActiveChanged(false);
}

void Vehicle::_chunkedStatusTextTimeout(void)
{
    // Spit out all incomplete chunks
    QList<uint8_t> rgCompId = _chunkedStatusTextInfoMap.keys();
    for (uint8_t compId : rgCompId) {
        _chunkedStatusTextInfoMap[compId].rgMessageChunks.append(QString());
        _chunkedStatusTextCompleted(compId);
    }
}

void Vehicle::_chunkedStatusTextCompleted(uint8_t compId)
{
    ChunkedStatusTextInfo_t&    chunkedInfo =   _chunkedStatusTextInfoMap[compId];
    uint8_t                     severity =      chunkedInfo.severity;
    QStringList&                rgChunks =      chunkedInfo.rgMessageChunks;

    // Build up message from chunks
    QString messageText;
    for (const QString& chunk : rgChunks) {
        if (chunk.isEmpty()) {
            // Indicates missing chunk
            messageText += tr(" ... ", "Indicates missing chunk from chunked STATUS_TEXT");
        } else {
            messageText += chunk;
        }
    }

    _chunkedStatusTextInfoMap.remove(compId);

    bool skipSpoken = false;
    bool ardupilotPrearm = messageText.startsWith(QStringLiteral("PreArm"));
    bool px4Prearm = messageText.startsWith(QStringLiteral("preflight"), Qt::CaseInsensitive) && severity >= MAV_SEVERITY_CRITICAL;
    if (ardupilotPrearm || px4Prearm) {
        // Limit repeated PreArm message to once every 10 seconds
        if (_noisySpokenPrearmMap.contains(messageText) && _noisySpokenPrearmMap[messageText].msecsTo(QTime::currentTime()) < (10 * 1000)) {
            skipSpoken = true;
        } else {
            _noisySpokenPrearmMap[messageText] = QTime::currentTime();
            setPrearmError(messageText);
        }
    }

    // If the message is NOTIFY or higher severity, or starts with a '#',
    // then read it aloud.
    bool readAloud = false;

    if (messageText.startsWith("#")) {
        messageText.remove(0, 1);
        readAloud = true;
    }
//    else if (severity <= MAV_SEVERITY_NOTICE) {
//        readAloud = true;
//    }

    if (readAloud) {
        if (!skipSpoken) {
            qgcApp()->toolbox()->audioOutput()->say(messageText);
        }
    }
    emit textMessageReceived(id(), compId, severity, messageText);
}

// Ignore warnings from mavlink headers for both GCC/Clang and MSVC
#ifdef __GNUC__

#if __GNUC__ > 8
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
#elif defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Waddress-of-packed-member"
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#endif

#else
#pragma warning(push, 0)
#endif

//void Vehicle::_setCapabilities(uint64_t capabilityBits)
//{
//    _capabilityBits = capabilityBits;
//    _capabilityBitsKnown = true;
//    emit capabilitiesKnownChanged(true);
//    emit capabilityBitsChanged(_capabilityBits);

//    QString supports("supports");
//    QString doesNotSupport("does not support");

//    qCDebug(VehicleLog) << QString("Vehicle %1 Mavlink 2.0").arg(_capabilityBits & MAV_PROTOCOL_CAPABILITY_MAVLINK2 ? supports : doesNotSupport);
//    qCDebug(VehicleLog) << QString("Vehicle %1 MISSION_ITEM_INT").arg(_capabilityBits & MAV_PROTOCOL_CAPABILITY_MISSION_INT ? supports : doesNotSupport);
//    qCDebug(VehicleLog) << QString("Vehicle %1 MISSION_COMMAND_INT").arg(_capabilityBits & MAV_PROTOCOL_CAPABILITY_COMMAND_INT ? supports : doesNotSupport);
//    qCDebug(VehicleLog) << QString("Vehicle %1 GeoFence").arg(_capabilityBits & MAV_PROTOCOL_CAPABILITY_MISSION_FENCE ? supports : doesNotSupport);
//    qCDebug(VehicleLog) << QString("Vehicle %1 RallyPoints").arg(_capabilityBits & MAV_PROTOCOL_CAPABILITY_MISSION_RALLY ? supports : doesNotSupport);
//    qCDebug(VehicleLog) << QString("Vehicle %1 Terrain").arg(_capabilityBits & MAV_PROTOCOL_CAPABILITY_TERRAIN ? supports : doesNotSupport);

//    _setMaxProtoVersionFromBothSources();
//}

void Vehicle::_setMaxProtoVersion(unsigned version) {

    // Set only once or if we need to reduce the max version
    if (_maxProtoVersion == 0 || version < _maxProtoVersion) {
        qCDebug(VehicleLog) << "_setMaxProtoVersion before:after" << _maxProtoVersion << version;
        _maxProtoVersion = version;
        emit requestProtocolVersion(_maxProtoVersion);
    }
}

QString Vehicle::vehicleUIDStr()
{
    QString uid;
    uint8_t* pUid = (uint8_t*)(void*)&_uid;
    uid.asprintf("%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
                 pUid[0] & 0xff,
            pUid[1] & 0xff,
            pUid[2] & 0xff,
            pUid[3] & 0xff,
            pUid[4] & 0xff,
            pUid[5] & 0xff,
            pUid[6] & 0xff,
            pUid[7] & 0xff);
    return uid;
}

void Vehicle::_setHomePosition(QGeoCoordinate& homeCoord)
{
    if (homeCoord != _homePosition) {
        _homePosition = homeCoord;
        emit homePositionChanged(_homePosition);
    }
}

void Vehicle::_updateArmed(bool armed)
{
    if (_armed != armed) {
        _armed = armed;
        emit armedChanged(_armed);
        // We are transitioning to the armed state, begin tracking trajectory points for the map
        if (_armed) {
            _trajectoryPoints->start();
            _flightTimerStart();
            _clearCameraTriggerPoints();
            // Reset battery warning
            _lowestBatteryChargeStateAnnouncedMap.clear();
        } else {
            _trajectoryPoints->stop();
            _flightTimerStop();
            // Also handle Video Streaming
            if(qgcApp()->toolbox()->videoManager()->videoReceiver()) {
                if(_settingsManager->videoSettings()->disableWhenDisarmed()->rawValue().toBool()) {
                    _settingsManager->videoSettings()->streamEnabled()->setRawValue(false);
                    qgcApp()->toolbox()->videoManager()->videoReceiver()->stop();
                }
            }
        }
    }
}

// Pop warnings ignoring for mavlink headers for both GCC/Clang and MSVC
#ifdef __GNUC__
#if defined(__clang__)
#pragma clang diagnostic pop
#else
#pragma GCC diagnostic pop
#endif
#else
#pragma warning(pop, 0)
#endif

bool Vehicle::sendShenHangMessageOnLinkThreadSafe(LinkInterface* link, ShenHangProtocolMessage message)
{
    if (!link->isConnected()) {
        qCDebug(VehicleLog) << "sendShenHangMessageOnLinkThreadSafe" << link << "not connected!";
        return false;
    }

    // Write message into buffer, prepending start sign
    message.idSource = idSource;
    message.idTarget = idTarget;
    uint16_t len = _shenHangProtocol->Encode(message, bufferSend);

    link->writeBytesThreadSafe((const char*)bufferSend, len);
    _messagesSent++;
    emit messagesSentChanged();
    return true;
}

bool Vehicle::coaxialMotors()
{
    return _firmwarePlugin->multiRotorCoaxialMotors(this);
}

bool Vehicle::xConfigMotors()
{
    return _firmwarePlugin->multiRotorXConfig(this);
}

QString Vehicle::formattedMessages()
{
    QString messages;

    return messages;
}

void Vehicle::clearMessages()
{
}

void Vehicle::_handletextMessageReceived(UASMessage* message)
{
    if (message) {
//        emit newFormattedMessage(message->getFormatedText());
    }
}

void Vehicle::_handleTextMessage(int newCount)
{
    // Reset?
    if(!newCount) {
        _currentMessageCount = 0;
        _currentNormalCount  = 0;
        _currentWarningCount = 0;
        _currentErrorCount   = 0;
        _messageCount        = 0;
        _currentMessageType  = MessageNone;
        emit newMessageCountChanged();
        emit messageTypeChanged();
        emit messageCountChanged();
        return;
    }

//    UASMessageHandler* pMh = _toolbox->uasMessageHandler();
    MessageType_t type = newCount ? _currentMessageType : MessageNone;
    int errorCount     = _currentErrorCount;
    int warnCount      = _currentWarningCount;
    int normalCount    = _currentNormalCount;
    //-- Add current message counts
//    errorCount  += pMh->getErrorCount();
//    warnCount   += pMh->getWarningCount();
//    normalCount += pMh->getNormalCount();
    //-- See if we have a higher level
    if(errorCount != _currentErrorCount) {
        _currentErrorCount = errorCount;
        type = MessageError;
    }
    if(warnCount != _currentWarningCount) {
        _currentWarningCount = warnCount;
        if(_currentMessageType != MessageError) {
            type = MessageWarning;
        }
    }
    if(normalCount != _currentNormalCount) {
        _currentNormalCount = normalCount;
        if(_currentMessageType != MessageError && _currentMessageType != MessageWarning) {
            type = MessageNormal;
        }
    }
    int count = _currentErrorCount + _currentWarningCount + _currentNormalCount;
    if(count != _currentMessageCount) {
        _currentMessageCount = count;
        // Display current total new messages count
        emit newMessageCountChanged();
    }
    if(type != _currentMessageType) {
        _currentMessageType = type;
        // Update message level
        emit messageTypeChanged();
    }
    // Update message count (all messages)
    if(newCount != _messageCount) {
        _messageCount = newCount;
        emit messageCountChanged();
    }
}

void Vehicle::resetMessages()
{
    // Reset Counts
    int count = _currentMessageCount;
    MessageType_t type = _currentMessageType;
    _currentErrorCount   = 0;
    _currentWarningCount = 0;
    _currentNormalCount  = 0;
    _currentMessageCount = 0;
    _currentMessageType = MessageNone;
    if(count != _currentMessageCount) {
        emit newMessageCountChanged();
    }
    if(type != _currentMessageType) {
        emit messageTypeChanged();
    }
}


QGeoCoordinate Vehicle::homePosition()
{
    return _homePosition;
}

void Vehicle::setArmed(bool armed, bool showError)
{

}

void Vehicle::forceArm(void)
{

}

bool Vehicle::flightModeSetAvailable()
{
    return true;            // _firmwarePlugin->isCapable(this, FirmwarePlugin::SetFlightModeCapability);
}

QStringList Vehicle::flightModes()
{
    return shenHangFlightModes;     //_firmwarePlugin->flightModes(this);
}

QStringList Vehicle::extraJoystickFlightModes()
{
    return _firmwarePlugin->extraJoystickFlightModes(this);
}

QString Vehicle::flightMode() const
{
    return shenHangFlightMode;      //_firmwarePlugin->flightMode(_base_mode, _custom_mode);
}

void Vehicle::setFlightMode(const QString& flightMode)
{
    uint8_t     base_mode;
    uint32_t    custom_mode;

    if (_firmwarePlugin->setFlightMode(flightMode, &base_mode, &custom_mode)) {
        SharedLinkInterfacePtr sharedLink = vehicleLinkManager()->primaryLink().lock();
        if (!sharedLink) {
            qCDebug(VehicleLog) << "setFlightMode: primary link gone!";
            return;
        }
    } else {
        qCWarning(VehicleLog) << "FirmwarePlugin::setFlightMode failed, flightMode:" << flightMode;
    }
}

#if 0
QVariantList Vehicle::links() const {
    QVariantList ret;

    for( const auto &item: _links )
        ret << QVariant::fromValue(item);

    return ret;
}
#endif

void Vehicle::_missionManagerError(int errorCode, const QString& errorMsg)
{
    Q_UNUSED(errorCode);
    qgcApp()->showAppMessage(tr("Mission transfer failed. Error: %1").arg(errorMsg));
}

void Vehicle::_geoFenceManagerError(int errorCode, const QString& errorMsg)
{
    Q_UNUSED(errorCode);
    qgcApp()->showAppMessage(tr("GeoFence transfer failed. Error: %1").arg(errorMsg));
}

void Vehicle::_clearCameraTriggerPoints()
{
    _cameraTriggerPoints.clearAndDeleteContents();
}

void Vehicle::_flightTimerStart()
{
    _flightTimer.start();
    _flightTimeUpdater.start();
    _flightDistanceFact.setRawValue(0);
    _flightTimeFact.setRawValue(0);
}

void Vehicle::_flightTimerStop()
{
    _flightTimeUpdater.stop();
}

void Vehicle::_updateFlightTime()
{
    _flightTimeFact.setRawValue((double)_flightTimer.elapsed() / 1000.0);
}

void Vehicle::_gotProgressUpdate(float progressValue)
{
    if (sender() != _initialConnectStateMachine && _initialConnectStateMachine->active()) {
        return;
    }
    if (sender() == _initialConnectStateMachine && !_initialConnectStateMachine->active()) {
        progressValue = 0.f;
    }
    _loadProgress = progressValue;
    emit loadProgressChanged(progressValue);
}

void Vehicle::_firstMissionLoadComplete()
{
    disconnect(_missionManager, &MissionManager::newMissionItemsAvailable, this, &Vehicle::_firstMissionLoadComplete);
//    _initialConnectStateMachine->advance();
}

void Vehicle::_firstGeoFenceLoadComplete()
{
    disconnect(_geoFenceManager, &GeoFenceManager::loadComplete, this, &Vehicle::_firstGeoFenceLoadComplete);
//    _initialConnectStateMachine->advance();
}

void Vehicle::_parametersReady(bool parametersReady)
{
    qCDebug(VehicleLog) << "_parametersReady" << parametersReady;

    if (parametersReady) {
        disconnect(_parameterManager, &ParameterManager::parametersReadyChanged, this, &Vehicle::_parametersReady);
        _setupAutoDisarmSignalling();
        _initialConnectStateMachine->advance();
    }
}

void Vehicle::_imageReady(UASInterface*)
{

}

void Vehicle::_say(const QString& text)
{
    _toolbox->audioOutput()->say(text.toLower());
}

bool Vehicle::airship() const
{
    return QGCMAVLink::isHelicopter(vehicleType());
}

bool Vehicle::fixedWing() const
{
    return QGCMAVLink::isFixedWing(vehicleType());
}

bool Vehicle::rover() const
{
    return QGCMAVLink::isJustaposedVector(vehicleType());
}

bool Vehicle::sub() const
{
    return QGCMAVLink::isTandemVector(vehicleType());
}

bool Vehicle::multiRotor() const
{
    return QGCMAVLink::isMultiRotor(vehicleType());
}

bool Vehicle::vtol() const
{
    return QGCMAVLink::TiltRotor(vehicleType());
}

bool Vehicle::supportsThrottleModeCenterZero() const
{
    return _firmwarePlugin->supportsThrottleModeCenterZero();
}

bool Vehicle::supportsNegativeThrust()
{
    return _firmwarePlugin->supportsNegativeThrust(this);
}

bool Vehicle::supportsRadio() const
{
    return _firmwarePlugin->supportsRadio();
}

bool Vehicle::supportsJSButton() const
{
    return _firmwarePlugin->supportsJSButton();
}

bool Vehicle::supportsMotorInterference() const
{
    return _firmwarePlugin->supportsMotorInterference();
}

bool Vehicle::supportsTerrainFrame() const
{
    return !shenHangFirmware();
}

QString Vehicle::vehicleTypeName() const {
    static QMap<int, QString> typeNames = {
        { MAV_TYPE_GENERIC,           tr("Generic micro air vehicle" )},
        { MAV_TYPE_HELICOPTER,        tr("Normal helicopter with tail rotor.")},
        { MAV_TYPE_FIXED_WING,        tr("Fixed wing aircraft")},
        { MAV_TYPE_MULTIROTOR,        tr("Quadrotor")},
        { MAV_TYPE_TANDEM_VECTOR,     tr("Tandem vector helicopter")},
        { MAV_TYPE_JUXTAPOSED_VECTOR, tr("Juxtaposed vector helicopter")},
        { MAV_TYPE_TILT_ROTOR,        tr("Tilt rotor")}
    };
    return typeNames[_vehicleType];
}

/// Returns the string to speak to identify the vehicle
QString Vehicle::_vehicleIdSpeech()
{
    if (_toolbox->multiVehicleManager()->vehicles()->count() > 1) {
        return tr("Vehicle %1 ").arg(id());
    } else {
        return QString();
    }
}

void Vehicle::_handleFlightModeChanged(const QString& flightMode)
{
    _say(tr("%1 %2 flight mode").arg(_vehicleIdSpeech()).arg(flightMode));
    emit guidedModeChanged(_firmwarePlugin->isGuidedMode(this));
}

void Vehicle::_announceArmedChanged(bool armed)
{
    _say(QString("%1 %2").arg(_vehicleIdSpeech()).arg(armed ? tr("armed") : tr("disarmed")));
    if(armed) {
        //-- Keep track of armed coordinates
        _armedPosition = _coordinate;
        emit armedPositionChanged();
    }
}

void Vehicle::_setFlying(bool flying)
{
    if (_flying != flying) {
        _flying = flying;
        emit flyingChanged(flying);
    }
}

void Vehicle::_setLanding(bool landing)
{
    if (armed() && _landing != landing) {
        _landing = landing;
        emit landingChanged(landing);
    }
}

bool Vehicle::guidedModeSupported() const
{
    return _firmwarePlugin->isCapable(this, FirmwarePlugin::GuidedModeCapability);
}

bool Vehicle::pauseVehicleSupported() const
{
    return _firmwarePlugin->isCapable(this, FirmwarePlugin::PauseVehicleCapability);
}

bool Vehicle::orbitModeSupported() const
{
    return _firmwarePlugin->isCapable(this, FirmwarePlugin::OrbitModeCapability);
}

bool Vehicle::roiModeSupported() const
{
    return _firmwarePlugin->isCapable(this, FirmwarePlugin::ROIModeCapability);
}

bool Vehicle::takeoffVehicleSupported() const
{
    return _firmwarePlugin->isCapable(this, FirmwarePlugin::TakeoffVehicleCapability);
}

QString Vehicle::gotoFlightMode() const
{
    return _firmwarePlugin->gotoFlightMode();
}

void Vehicle::guidedModeRTL(bool smartRTL)
{
    if (!guidedModeSupported()) {
        qgcApp()->showAppMessage(guided_mode_not_supported_by_vehicle);
        return;
    }
    _firmwarePlugin->guidedModeRTL(this, smartRTL);
}

void Vehicle::guidedModeLand()
{
    if (!guidedModeSupported()) {
        qgcApp()->showAppMessage(guided_mode_not_supported_by_vehicle);
        return;
    }
    _firmwarePlugin->guidedModeLand(this);
}

void Vehicle::guidedModeTakeoff(double altitudeRelative)
{
    if (!guidedModeSupported()) {
        qgcApp()->showAppMessage(guided_mode_not_supported_by_vehicle);
        return;
    }
    _firmwarePlugin->guidedModeTakeoff(this, altitudeRelative);
}

double Vehicle::minimumTakeoffAltitude()
{
    return _firmwarePlugin->minimumTakeoffAltitude(this);
}

void Vehicle::startMission()
{
    _firmwarePlugin->startMission(this);
}

void Vehicle::setCurrentMissionSequence(int seq)
{

}

void Vehicle::guidedModeGotoLocation(const QGeoCoordinate& gotoCoord)
{
    if (!guidedModeSupported()) {
        qgcApp()->showAppMessage(guided_mode_not_supported_by_vehicle);
        return;
    }
    if (!coordinate().isValid()) {
        return;
    }
    double maxDistance = _settingsManager->flyViewSettings()->maxGoToLocationDistance()->rawValue().toDouble();
    if (coordinate().distanceTo(gotoCoord) > maxDistance) {
        qgcApp()->showAppMessage(QString("New location is too far. Must be less than %1 %2.").arg(qRound(FactMetaData::metersToAppSettingsHorizontalDistanceUnits(maxDistance).toDouble())).arg(FactMetaData::appSettingsHorizontalDistanceUnitsString()));
        return;
    }
    _firmwarePlugin->guidedModeGotoLocation(this, gotoCoord);
}

void Vehicle::guidedModeChangeAltitude(double altitudeChange, bool pauseVehicle)
{
    if (!guidedModeSupported()) {
        qgcApp()->showAppMessage(guided_mode_not_supported_by_vehicle);
        return;
    }
    _firmwarePlugin->guidedModeChangeAltitude(this, altitudeChange, pauseVehicle);
}

void Vehicle::pauseVehicle()
{
    if (!pauseVehicleSupported()) {
        qgcApp()->showAppMessage(QStringLiteral("Pause not supported by vehicle."));
        return;
    }
    _firmwarePlugin->pauseVehicle(this);
}

void Vehicle::abortLanding(double climbOutAltitude)
{

}

bool Vehicle::guidedMode() const
{
    return _firmwarePlugin->isGuidedMode(this);
}

void Vehicle::setGuidedMode(bool guidedMode)
{
    return _firmwarePlugin->setGuidedMode(this, guidedMode);
}

void Vehicle::emergencyStop()
{

}

void Vehicle::_sendShenHangCommandResponseTimeoutCheck()
{
    if (_shenHangCommandList.isEmpty()) {
        return;
    }

    // Walk the list backwards since _sendMavCommandFromList can remove entries
    for (int i=_shenHangCommandList.count()-1; i>=0; i--) {
        ShenHangCommandListEntry& commandEntry = _shenHangCommandList[i];
        if (commandEntry.elapsedTimer.elapsed() > commandEntry.ackTimeoutMSecs) {
            qCDebug(VehicleLog) << "*************" << QDateTime::currentDateTime() << commandEntry.elapsedTimer.elapsed();

            // Try sending command again
            _sendShenHangCommandFromList(i);
        }
    }
}

QString Vehicle::getShenHangCommandName(uint8_t tyMsg0, uint8_t tyMsg1)
{
    switch (tyMsg0)
    {
    case COMMAND_VEHICLE:
        return "Command Vehicle";
    case COMMAND_PARAM:
        // 设置参数（ty_msg0=129,ty_msg1=4）
        switch (tyMsg1)
        {
        case COMMAND_RESET_PARAM: return "Reset Parameter";
        case COMMAND_LOAD_PARAM: return "Load Parameter";
        case COMMAND_SAVE_PARAM: return "Save Parameter";
        case COMMAND_QUERY_PARAM: return "Query Parameter";
        case COMMAND_SET_PARAM: return "Set Parameter";
        default: return "Unknown Parameter Command";
        }
    case COMMAND_BANK:
        switch (tyMsg1)
        {
        case QUERY_ALL_BANK: return "Query All Banks";
        case QUERY_SINGLE_BANK: return "Query Single Bank";
        case SET_SINGLE_BANK: return "Set Single Bank";
        case REFACTOR_INFO_SLOT: return "Refactor Single Infoslot";
        case QUERY_SINGLE_INFO_SLOT: return "Set Single Infoslot";
        case ENABLE_BANK_AUTO_SW: return "Bank Auto Switch";
        case WAYPOINT_AUTO_SW: return "Waypoint Auto Switch";
        default: return "Unknown Bank Command";
        }
    case COMMAND_PAYLOAD:   return "Command Poyload";
    case COMMAND_REMOTE_CONTROL:    return "Command Remote Control";
    case COMMAND_COMM:      return "Command Communication";
    case COMMAND_ORGANIZE:  return "Command Organize";
    default:                return "Unknown command";
    }
}

int Vehicle::_findShenHangCommandListEntryIndex(uint8_t tyMsg0, uint8_t tyMsg1)
{
    for (int i=0; i<_shenHangCommandList.count(); i++) {
        const ShenHangCommandListEntry& entry = _shenHangCommandList[i];
        if (entry.tyMsg0 == tyMsg0 && entry.tyMsg1 == tyMsg1) {
            return i;
        }
    }
    return -1;
}

void Vehicle::_sendShenHangCommandWorker(bool showError)
{
    int entryIndex = _findShenHangCommandListEntryIndex(msgSend.tyMsg0, msgSend.tyMsg1);
    if (entryIndex != -1) { // 相同命令已发送直接返回
        QString rawCommandName  = getShenHangCommandName(msgSend.tyMsg0, msgSend.tyMsg1);
        qCDebug(VehicleLog) << QStringLiteral("_sendShenHangCommandWorker failing duplicate command") << rawCommandName;
        if (showError) {
            qgcApp()->showAppMessage(tr("Unable to send command: Waiting on previous response to same command.."));
        }
        return;
    }
    SharedLinkInterfacePtr sharedLink = vehicleLinkManager()->primaryLink().lock();
    if (!sharedLink) {
        qCDebug(VehicleLog) << "_sendMavCommandWorker: primary link gone!";
        return;
    }
    ShenHangCommandListEntry entry;
    entry.tyMsg0 = msgSend.tyMsg0;
    entry.tyMsg1 = msgSend.tyMsg1;
    entry.showError = showError;
    entry.maxTries =  _shenHangCommandMaxRetryCount;
    entry.ackTimeoutMSecs =  _mavCommandAckTimeoutMSecs;
    entry.idTarget = idTarget;
    entry.elapsedTimer.start();
    memcpy(entry.data, msgSend.payload, sizeof(entry.data));
    _shenHangCommandList.append(entry);
    _sendShenHangCommandFromList(_shenHangCommandList.count() - 1);
}

void Vehicle::_sendShenHangCommandFromList(int index)
{
    ShenHangCommandListEntry commandEntry = _shenHangCommandList[index];
    QString rawCommandName  = getShenHangCommandName(commandEntry.tyMsg0, commandEntry.tyMsg1);

    if (++_shenHangCommandList[index].tryCount > commandEntry.maxTries) {
        qCDebug(VehicleLog) << "_sendMavCommandFromList giving up after max retries" << rawCommandName;
        _shenHangCommandList.removeAt(index);
        if (commandEntry.showError) {
            qgcApp()->showAppMessage(tr("Vehicle did not respond to command: %1").arg(rawCommandName));
        }
        return;
    }

//    if (commandEntry.requestMessage) {
//        RequestMessageInfo_t* pInfo = static_cast<RequestMessageInfo_t*>(commandEntry.resultHandlerData);
//        _waitForMavlinkMessage(_requestMessageWaitForMessageResultHandler, pInfo, pInfo->msgId, 1000);
//    }

    qCDebug(VehicleLog) << "_sendMavCommandFromList command:tryCount" << rawCommandName << commandEntry.tryCount;

    SharedLinkInterfacePtr sharedLink = vehicleLinkManager()->primaryLink().lock();
    if (!sharedLink) {
        qCDebug(VehicleLog) << "_sendShenHangCommandFromList: primary link gone!";
        return;
    }

    ShenHangProtocolMessage msg = {};
    msg.tyMsg0 = commandEntry.tyMsg0;
    msg.tyMsg1 = commandEntry.tyMsg1;
    msg.idSource = static_cast<uint16_t>(_shenHangProtocol->getSystemId());
    msg.idTarget = idTarget;
    memcpy(msg.payload, commandEntry.data, sizeof(msg.payload));
    sendShenHangMessageOnLinkThreadSafe(sharedLink.get(), msg);
}

void Vehicle::setPrearmError(const QString& prearmError)
{
    _prearmError = prearmError;
    emit prearmErrorChanged(_prearmError);
    if (!_prearmError.isEmpty()) {
        _prearmErrorTimer.start();
    }
}

void Vehicle::_prearmErrorTimeout()
{
    setPrearmError(QString());
}

void Vehicle::_rebootCommandResultHandler(void* resultHandlerData, int /*compId*/, MAV_RESULT commandResult, MavCmdResultFailureCode_t failureCode)
{
    Vehicle* vehicle = static_cast<Vehicle*>(resultHandlerData);

    if (commandResult != MAV_RESULT_ACCEPTED) {
        switch (failureCode) {
        case MavCmdResultCommandResultOnly:
            qCDebug(VehicleLog) << QStringLiteral("MAV_CMD_PREFLIGHT_REBOOT_SHUTDOWN error(%1)").arg(commandResult);
            break;
        case MavCmdResultFailureNoResponseToCommand:
            qCDebug(VehicleLog) << "MAV_CMD_PREFLIGHT_REBOOT_SHUTDOWN failed: no response from vehicle";
            break;
        case MavCmdResultFailureDuplicateCommand:
            qCDebug(VehicleLog) << "MAV_CMD_PREFLIGHT_REBOOT_SHUTDOWN failed: duplicate command";
            break;
        }
        qgcApp()->showAppMessage(tr("Vehicle reboot failed."));
    } else {
        vehicle->closeVehicle();
    }
}

void Vehicle::rebootVehicle()
{
}

void Vehicle::startCalibration(Vehicle::CalibrationType calType)
{
}

void Vehicle::stopCalibration(void)
{

}

void Vehicle::setSoloFirmware(bool soloFirmware)
{
    if (soloFirmware != _soloFirmware) {
        _soloFirmware = soloFirmware;
        emit soloFirmwareChanged(soloFirmware);
    }
}

QString Vehicle::brandImageIndoor() const
{
    return _firmwarePlugin->brandImageIndoor(this);
}

QString Vehicle::brandImageOutdoor() const
{
    return _firmwarePlugin->brandImageOutdoor(this);
}

void Vehicle::setOfflineEditingDefaultComponentId(int defaultComponentId)
{
    if (_offlineEditingVehicle) {
        _defaultComponentId = defaultComponentId;
    } else {
        qCWarning(VehicleLog) << "Call to Vehicle::setOfflineEditingDefaultComponentId on vehicle which is not offline";
    }
}

void Vehicle::setVtolInFwdFlight(bool vtolInFwdFlight)
{
    if (_vtolInFwdFlight != vtolInFwdFlight) {

    }
}

void Vehicle::setFirmwarePluginInstanceData(QObject* firmwarePluginInstanceData)
{
    firmwarePluginInstanceData->setParent(this);
    _firmwarePluginInstanceData = firmwarePluginInstanceData;
}

QString Vehicle::missionFlightMode() const
{
    return _firmwarePlugin->missionFlightMode();
}

QString Vehicle::pauseFlightMode() const
{
    return _firmwarePlugin->pauseFlightMode();
}

QString Vehicle::rtlFlightMode() const
{
    return _firmwarePlugin->rtlFlightMode();
}

QString Vehicle::smartRTLFlightMode() const
{
    return _firmwarePlugin->smartRTLFlightMode();
}

bool Vehicle::supportsSmartRTL() const
{
    return _firmwarePlugin->supportsSmartRTL();
}

QString Vehicle::landFlightMode() const
{
    return _firmwarePlugin->landFlightMode();
}

QString Vehicle::takeControlFlightMode() const
{
    return _firmwarePlugin->takeControlFlightMode();
}

QString Vehicle::followFlightMode() const
{
    return _firmwarePlugin->followFlightMode();
}

QString Vehicle::vehicleImageOpaque() const
{
    if(_firmwarePlugin)
        return _firmwarePlugin->vehicleImageOpaque(this);
    else
        return QString();
}

QString Vehicle::vehicleImageOutline() const
{
    if(_firmwarePlugin)
        return _firmwarePlugin->vehicleImageOutline(this);
    else
        return QString();
}

QString Vehicle::vehicleImageCompass() const
{
    if(_firmwarePlugin)
        return _firmwarePlugin->vehicleImageCompass(this);
    else
        return QString();
}

const QVariantList& Vehicle::toolIndicators()
{
    if(_firmwarePlugin) {
        return _firmwarePlugin->toolIndicators(this);
    }
    static QVariantList emptyList;
    return emptyList;
}

const QVariantList& Vehicle::modeIndicators()
{
    if(_firmwarePlugin) {
        return _firmwarePlugin->modeIndicators(this);
    }
    static QVariantList emptyList;
    return emptyList;
}

const QVariantList& Vehicle::staticCameraList() const
{
    if (_firmwarePlugin) {
        return _firmwarePlugin->cameraList(this);
    }
    static QVariantList emptyList;
    return emptyList;
}

void Vehicle::_setupAutoDisarmSignalling()
{
    QString param = _firmwarePlugin->autoDisarmParameter(this);

    if (!param.isEmpty() && _parameterManager->parameterExists(param)) {
        Fact* fact = _parameterManager->getParameter(param);
        connect(fact, &Fact::rawValueChanged, this, &Vehicle::autoDisarmChanged);
        emit autoDisarmChanged();
    }
}

bool Vehicle::autoDisarm()
{
    QString param = _firmwarePlugin->autoDisarmParameter(this);

    if (!param.isEmpty() && _parameterManager->parameterExists(param)) {
        Fact* fact = _parameterManager->getParameter(param);
        return fact->rawValue().toDouble() > 0;
    }

    return false;
}

void Vehicle::_updateDistanceHeadingToHome()
{
    if (coordinate().isValid() && homePosition().isValid()) {
        _distanceToHomeFact.setRawValue(coordinate().distanceTo(homePosition()));
        if (_distanceToHomeFact.rawValue().toDouble() > 1.0) {
            _headingToHomeFact.setRawValue(coordinate().azimuthTo(homePosition()));
        } else {
            _headingToHomeFact.setRawValue(qQNaN());
        }
    } else {
        _distanceToHomeFact.setRawValue(qQNaN());
        _headingToHomeFact.setRawValue(qQNaN());
    }
}

void Vehicle::_updateHeadingToNextWP()
{
//    const int currentIndex = _missionManager->currentIndex();
//    QList<MissionItem*> llist = _missionManager->missionItems();

//    if(llist.size()>currentIndex && currentIndex!=-1
//            && llist[currentIndex]->coordinate().longitude()!=0.0
//            && coordinate().distanceTo(llist[currentIndex]->coordinate())>5.0 ){

//        _headingToNextWPFact.setRawValue(coordinate().azimuthTo(llist[currentIndex]->coordinate()));
//    }
//    else{
//        _headingToNextWPFact.setRawValue(qQNaN());
//    }
}

void Vehicle::_updateMissionItemIndex()
{
    const int currentIndex = _missionManager->currentIndex();

    unsigned offset = 0;
    if (!_firmwarePlugin->sendHomePositionToVehicle()) {
        offset = 1;
    }

    _missionItemIndexFact.setRawValue(currentIndex + offset);
}

void Vehicle::_updateDistanceToGCS()
{
    QGeoCoordinate gcsPosition = _toolbox->qgcPositionManager()->gcsPosition();
    if (coordinate().isValid() && gcsPosition.isValid()) {
        _distanceToGCSFact.setRawValue(coordinate().distanceTo(gcsPosition));
    } else {
        _distanceToGCSFact.setRawValue(qQNaN());
    }
}

void Vehicle::_updateHobbsMeter()
{
    _hobbsFact.setRawValue(hobbsMeter());
}

void Vehicle::forceInitialPlanRequestComplete()
{
    _initialPlanRequestComplete = true;
    emit initialPlanRequestCompleteChanged(true);
}

void Vehicle::sendPlan(QString planFile)
{
    PlanMasterController::sendPlanToVehicle(this, planFile);
}

QString Vehicle::hobbsMeter()
{
    static const char* HOOBS_HI = "LND_FLIGHT_T_HI";
    static const char* HOOBS_LO = "LND_FLIGHT_T_LO";
    //-- TODO: Does this exist on non PX4?
    if (_parameterManager->parameterExists(HOOBS_HI) &&
            _parameterManager->parameterExists(HOOBS_LO)) {
        Fact* factHi = _parameterManager->getParameter(HOOBS_HI);
        Fact* factLo = _parameterManager->getParameter(HOOBS_LO);
        uint64_t hobbsTimeSeconds = ((uint64_t)factHi->rawValue().toUInt() << 32 | (uint64_t)factLo->rawValue().toUInt()) / 1000000;
        int hours   = hobbsTimeSeconds / 3600;
        int minutes = (hobbsTimeSeconds % 3600) / 60;
        int seconds = hobbsTimeSeconds % 60;
        QString timeStr = QString::asprintf("%04d:%02d:%02d", hours, minutes, seconds);
        qCDebug(VehicleLog) << "Hobbs Meter:" << timeStr << "(" << factHi->rawValue().toUInt() << factLo->rawValue().toUInt() << ")";
        return timeStr;
    }
    return QString("0000:00:00");
}

void Vehicle::_vehicleParamLoaded(bool ready)
{
    //-- TODO: This seems silly but can you think of a better
    //   way to update this?
    if(ready) {
        emit hobbsMeterChanged();
    }
}

void Vehicle::_trafficUpdate(bool /*alert*/, QString /*traffic_id*/, QString /*vehicle_id*/, QGeoCoordinate /*location*/, float /*heading*/)
{
#if 0
    // This is ifdef'ed out for now since this code doesn't mesh with the recent ADSB manager changes. Also airmap isn't in any
    // released build. So not going to waste time trying to fix up unused code.
    if (_trafficVehicleMap.contains(traffic_id)) {
        _trafficVehicleMap[traffic_id]->update(alert, location, heading);
    } else {
        ADSBVehicle* vehicle = new ADSBVehicle(location, heading, alert, this);
        _trafficVehicleMap[traffic_id] = vehicle;
        _adsbVehicles.append(vehicle);
    }
#endif
}

void Vehicle::_shenHangMessageStatus(int uasId, uint64_t totalSent, uint64_t totalReceived, uint64_t totalLoss, float lossPercent)
{
    if(uasId == _id) {
        _mavlinkSentCount       = totalSent;
        _mavlinkReceivedCount   = totalReceived;
        _mavlinkLossCount       = totalLoss;
        _mavlinkLossPercent     = lossPercent;
        emit mavlinkStatusChanged();
    }
}

void Vehicle::gimbalControlValue(double pitch, double yaw)
{
    //qCDebug(VehicleLog) << "Gimbal:" << pitch << yaw;

}

void Vehicle::gimbalPitchStep(int direction)
{
    if(_haveGimbalData) {
        //qCDebug(VehicleLog) << "Pitch:" << _curGimbalPitch << direction << (_curGimbalPitch + direction);
        double p = static_cast<double>(_curGimbalPitch + direction);
        gimbalControlValue(p, static_cast<double>(_curGinmbalYaw));
    }
}

void Vehicle::gimbalYawStep(int direction)
{
    if(_haveGimbalData) {
        //qCDebug(VehicleLog) << "Yaw:" << _curGinmbalYaw << direction << (_curGinmbalYaw + direction);
        double y = static_cast<double>(_curGinmbalYaw + direction);
        gimbalControlValue(static_cast<double>(_curGimbalPitch), y);
    }
}

void Vehicle::centerGimbal()
{
    if(_haveGimbalData) {
        gimbalControlValue(0.0, 0.0);
    }
}

void Vehicle::updateFlightDistance(double distance)
{
    _flightDistanceFact.setRawValue(_flightDistanceFact.rawValue().toDouble() + distance);
}


void Vehicle::triggerSimpleCamera()
{
}

bool Vehicle::isInitialConnectComplete() const
{
    return !_initialConnectStateMachine->active();
}


void Vehicle::HandleWaypointInfo(ShenHangProtocolMessage& msg)
{
    WaypointInfoSlot waypointInfoSlot;
//    if (msg.tyMsg1 == 0)
    memcpy(&waypointInfoSlot, msg.payload, sizeof(waypointInfoSlot));

}

void Vehicle::HandleGeneralStatus(ShenHangProtocolMessage& msg)
{
    GeneralStatus generalStatus;
    memcpy(&generalStatus, msg.payload, sizeof(generalStatus));

    /******************** 处理飞行模式 ********************/
    QString shenHangPreviousFlightMode = shenHangFlightMode;
    for (size_t i=0; i<sizeof(FlightMode2Name)/sizeof(FlightMode2Name[0]); i++) {
        if (generalStatus.modeOp == FlightMode2Name[i].flightMode) {
            shenHangFlightMode = FlightMode2Name[i].name;
            break;
        }
    }
    if (shenHangPreviousFlightMode != shenHangFlightMode) {
        emit flightModeChanged(shenHangFlightMode);
    }

    /******************** 处理GPS信息 ********************/
    gpsRawInt.satellitesUsed = generalStatus.modeGnssSv & 0x1f;
    gpsRawInt.status = (generalStatus.modeGnssSv & 0b01100000) >> 5;
    gpsRawInt.dualAntenna = generalStatus.modeGnssSv >> 7;
    gpsRawInt.alt = generalStatus.alt * 1000;
//    gpsRawInt.velG = generalStatus.vG * 100;
//    gpsRawInt.velV = generalStatus.vV * 100;
//    gpsRawInt.yaw = generalStatus.crsDeg100x;
    _gpsFactGroup.setShenHangGpsInfo(gpsRawInt);
    _headingFact.setRawValue(generalStatus.crsDeg100x / 100.f);
}

void Vehicle::HandlePosVelTime(ShenHangProtocolMessage& msg)
{
    PosVelTime posVelTime;
    memcpy(&posVelTime, msg.payload, sizeof(posVelTime));

    /******************** 处理GPS信息 ********************/
    gpsRawInt.lat = static_cast<int32_t>(posVelTime.latDeg * 1e7);
    gpsRawInt.lon = static_cast<int32_t>(posVelTime.lonDeg * 1e7);
    gpsRawInt.alt = static_cast<int32_t>(posVelTime.alt * 1e3);
    gpsRawInt.velG = static_cast<uint16_t>(posVelTime.vG * 1e3f);
    gpsRawInt.velV = static_cast<uint16_t>(posVelTime.vUp * 1e3f);
    gpsRawInt.cog = static_cast<int16_t>(qRadiansToDegrees(posVelTime.crsRad) * 100);
    _climbRateFact.setRawValue(posVelTime.vUp);

    _coordinate.setLongitude(posVelTime.lonDeg);
    _coordinate.setLatitude(posVelTime.latDeg);
    _coordinate.setAltitude(posVelTime.alt);
    _altitudeAMSLFact.setRawValue(posVelTime.alt);
    _altitudeRelativeFact.setRawValue(posVelTime.alt - 5);
    emit coordinateChanged(_coordinate);
    if (_flying == false) {
        _flying = true;
        emit flyingChanged(true);
        _updateArmed(true);
    }
}

void Vehicle::HandleAttitude(ShenHangProtocolMessage& msg)
{
    Attitude attitude;
    memcpy(&attitude, msg.payload, sizeof(attitude));
    
//    Eigen::Quaternionf quat(attitude.q1Int16 / 32767.f, attitude.q2Int16 / 32767.f, attitude.q3Int16 / 32767.f, attitude.q4Int16 / 32767.f);
//    float roll, pitch, yaw;
//    float q[] = { quat.w(), quat.x(), quat.y(), quat.z() };
//    mavlink_quaternion_to_euler(q, &roll, &pitch, &yaw);
//    Fact _rollFact;
//    Fact _pitchFact;
//    Fact _headingFact;
    // FIXME 此处直接使用四元数作为姿态角来测试，不知道怎么把三个四元数转为姿态角
    _rollFact.setRawValue(attitude.q1Int16);
    _pitchFact.setRawValue(attitude.q2Int16);
    _headingFact.setRawValue(attitude.q3Int16 );
    _rollRateFact.setRawValue(qRadiansToDegrees(attitude.p1000x / 1000.f));
    _pitchRateFact.setRawValue(qRadiansToDegrees(attitude.q1000x / 1000.f));
    _yawRateFact.setRawValue(qRadiansToDegrees(attitude.r1000x / 1000.f));
}

void Vehicle::HandleGeneralData(ShenHangProtocolMessage& msg)
{

}

void Vehicle::HandleAckReset(ShenHangProtocolMessage& msg)
{

}

void Vehicle::HandleAckCommandParam(ShenHangProtocolMessage& msg)
{
    uint8_t idCfgGroup0;
    uint8_t execuateState;

    // 单个参数查询返回（ty_msg0=193,ty_msg1=3）；单个参数设置返回（ty_msg0=193,ty_msg1=4）
    uint8_t lenCfg;         // 查询的设置参数的字节数，根据xml文件确定
    uint16_t addrOffset;    // 查询的设置参数在对应组内的偏移地址，根据xml文件确定
    uint8_t data[16];       // 具体设置值，按内存顺序排列，低字节在前，高字节在后

    // 错误信息返回（ty_msg0=193,ty_msg1=255）
    uint8_t errTyMsg1;      // 出错的命令对应的ty_msg1编号
    uint8_t dgnCode;        // 错误编码，2：编号超界，4：无效指令，其他：保留
    switch (msg.tyMsg1)
    {
    case ACK_RESET_PARAM:
        idCfgGroup0 = msg.payload[0];
        execuateState = msg.payload[1];
        break;
    case ACK_LOAD_PARAM:
        idCfgGroup0 = msg.payload[0];
        execuateState = msg.payload[1];
        break;
    case ACK_SAVE_PARAM:
        idCfgGroup0 = msg.payload[0];
        execuateState = msg.payload[1];
        break;
    case ACK_QUERY_PARAM:
        idCfgGroup0 = msg.payload[0];
        lenCfg = msg.payload[1];
        memcpy(&addrOffset, msg.payload + 2, 2);
        memcpy(data, msg.payload + 4, sizeof(data));
        break;
    case ACK_SET_PARAM:
        idCfgGroup0 = msg.payload[0];
        lenCfg = msg.payload[1];
        memcpy(&addrOffset, msg.payload + 2, 2);
        memcpy(data, msg.payload + 4, sizeof(data));
        break;
    case ACK_ERROR_PARAM:
        errTyMsg1 = msg.payload[0];
        dgnCode = msg.payload[1];
        break;
    default:
        break;
    }
    qCDebug(VehicleLog) << "AckCommandParam msg.tyMsg1" << "idCfgGroup0" << idCfgGroup0
                        << "execuateState" << execuateState << "lenCfg" << lenCfg << "errTyMsg1" << errTyMsg1 << "dgnCode" << dgnCode;
}

void Vehicle::HandleAckCommandBank(ShenHangProtocolMessage& msg)
{
    TotalBankInfo totalBankInfo = {};
    SingleBankInfo singleBankInfo = {};
    SingleBankCheckInfo refactorInfoSlot = {};
    struct WaypointInfoSlot waypointInfoSlot = {};
    ErrorBankInfo errorBankInfo = {};
    switch (msg.tyMsg1)
    {
    case ACK_QUERY_ALL_BANK:
        memcpy(&totalBankInfo, msg.payload, sizeof(totalBankInfo));
        break;
    case ACK_QUERY_SINGLE_BANK:
    case ACK_SET_SINGLE_BANK:
    case ACK_BANK_AUTO_SW:
    case ACK_WAYPOINT_AUTO_SW:
        memcpy(&singleBankInfo, msg.payload, sizeof(singleBankInfo));
        break;
    case REFACTOR_INFO_SLOT:
        memcpy(&refactorInfoSlot, msg.payload, sizeof(refactorInfoSlot));
        break;
    case ACK_QUERY_SINGLE_INFO_SLOT:
        memcpy(&waypointInfoSlot, msg.payload, sizeof(waypointInfoSlot));
        break;
    case ACK_BANK_ERROR:
        memcpy(&errorBankInfo, msg.payload, sizeof(errorBankInfo));
        break;
    default:
        break;
    }
}

void Vehicle::PackVehicleCommand()
{

}

void Vehicle::PackSetParamCommand(ShenHangProtocolMessage& msg, CommandParamType typeMsg1, uint8_t idGroup, uint8_t length, uint16_t addrOffset, uint8_t* data, uint8_t dataLength)
{
    msg.tyMsg0 = COMMAND_PARAM;
    msg.tyMsg1 = typeMsg1;
    msg.idSource = idSource;
    msg.idTarget = idTarget;
    memset(msg.payload, 0, sizeof(msg.payload));
    msg.payload[0] = idGroup;
    msg.payload[1] = length;
    memcpy(msg.payload + 2, &addrOffset, 2);
    memcpy(msg.payload + 4, &data, sizeof(dataLength));
}

void Vehicle::PackCommandParamReset(ShenHangProtocolMessage& msg, uint8_t idGroup)
{
    msg.tyMsg0 = COMMAND_PARAM;
    msg.tyMsg1 = ACK_RESET_PARAM;
    msg.idSource = idSource;
    msg.idTarget = idTarget;
    memset(msg.payload, 0, sizeof(msg.payload));
    msg.payload[0] = idGroup;
}

void Vehicle::PackCommandParamLoad(ShenHangProtocolMessage& msg, uint8_t idGroup)
{
    msg.tyMsg0 = COMMAND_PARAM;
    msg.tyMsg1 = COMMAND_LOAD_PARAM;
    msg.idSource = idSource;
    msg.idTarget = idTarget;
    memset(msg.payload, 0, sizeof(msg.payload));
    msg.payload[0] = idGroup;
}

void Vehicle::PackCommandParamSave(ShenHangProtocolMessage& msg, uint8_t idGroup)
{
    msg.tyMsg0 = COMMAND_PARAM;
    msg.tyMsg1 = COMMAND_SAVE_PARAM;
    msg.idSource = idSource;
    msg.idTarget = idTarget;
    memset(msg.payload, 0, sizeof(msg.payload));
    msg.payload[0] = idGroup;
}

void Vehicle::PackCommandParamQuery(ShenHangProtocolMessage& msg, uint8_t idGroup, uint8_t length, uint16_t addrOffset)
{
    msg.tyMsg0 = COMMAND_PARAM;
    msg.tyMsg1 = COMMAND_QUERY_PARAM;
    msg.idSource = idSource;
    msg.idTarget = idTarget;
    memset(msg.payload, 0, sizeof(msg.payload));
    msg.payload[0] = idGroup;
    msg.payload[1] = length;
    memcpy(msg.payload + 2, &addrOffset, 2);
}

void Vehicle::PackCommandSet(ShenHangProtocolMessage& msg, uint8_t idGroup, uint8_t length, uint16_t addrOffset, uint8_t* data)
{
    msg.tyMsg0 = COMMAND_PARAM;
    msg.tyMsg1 = COMMAND_SET_PARAM;
    msg.idSource = idSource;
    msg.idTarget = idTarget;
    memset(msg.payload, 0, sizeof(msg.payload));
    msg.payload[0] = idGroup;
    msg.payload[1] = length;
    memcpy(msg.payload + 2, &addrOffset, 2);
    memcpy(msg.payload + 4, data, 16);
}

void Vehicle::PackCommandBankQueryAll(ShenHangProtocolMessage& msg)
{
    msg.tyMsg0 = COMMAND_BANK;
    msg.tyMsg1 = QUERY_ALL_BANK;
    msg.idSource = idSource;
    msg.idTarget = idTarget;
    memset(msg.payload, 0, sizeof(msg.payload));
}

void Vehicle::PackCommandBankQuerySingle(ShenHangProtocolMessage& msg, uint16_t idBank)
{
    msg.tyMsg0 = COMMAND_BANK;
    msg.tyMsg1 = QUERY_SINGLE_BANK;
    msg.idSource = idSource;
    msg.idTarget = idTarget;
    memset(msg.payload, 0, sizeof(msg.payload));
    memcpy(msg.payload, &idBank, 2);
}

void Vehicle::PackCommandBankSetSingle(ShenHangProtocolMessage& msg, uint16_t idBank, uint16_t idBankSuc, uint16_t idBankIwpSuc, uint16_t actBankEnd, uint8_t flagBankVerified, uint8_t flagBankLock)
{
    msg.tyMsg0 = COMMAND_BANK;
    msg.tyMsg1 = SET_SINGLE_BANK;
    msg.idSource = idSource;
    msg.idTarget = idTarget;
    memset(msg.payload, 0, sizeof(msg.payload));
    memcpy(msg.payload, &idBank, 2);
    memcpy(msg.payload + 2, &idBankSuc, 2);
    memcpy(msg.payload + 4, &idBankIwpSuc, 2);
    memcpy(msg.payload + 6, &actBankEnd, 2);
    msg.payload[8] = flagBankVerified;
    msg.payload[9] = flagBankLock;
}

void Vehicle::PackRefactorInfoSlot(ShenHangProtocolMessage& msg, uint16_t idBank, uint16_t nWp, uint16_t nInfoSlot)
{
    msg.tyMsg0 = COMMAND_BANK;
    msg.tyMsg1 = REFACTOR_INFO_SLOT;
    msg.idSource = idSource;
    msg.idTarget = idTarget;
    memset(msg.payload, 0, sizeof(msg.payload));
    memcpy(msg.payload, &idBank, 2);
    memcpy(msg.payload + 2, &nWp, 2);
    memcpy(msg.payload + 4, &nInfoSlot, 2);
}

void Vehicle::PackQuerySingleInfoSlot(ShenHangProtocolMessage& msg, uint16_t idBank, uint16_t idInfoSlot)
{
    msg.tyMsg0 = COMMAND_BANK;
    msg.tyMsg1 = QUERY_SINGLE_INFO_SLOT;
    msg.idSource = idSource;
    msg.idTarget = idTarget;
    memset(msg.payload, 0, sizeof(msg.payload));
    memcpy(msg.payload, &idBank, 2);
    memcpy(msg.payload + 2, &idInfoSlot, 2);
}

void Vehicle::PackEnableBankAutoSw(ShenHangProtocolMessage& msg, uint8_t flagHalt)
{
    msg.tyMsg0 = COMMAND_BANK;
    msg.tyMsg1 = ENABLE_BANK_AUTO_SW;
    msg.idSource = idSource;
    msg.idTarget = idTarget;
    msg.payload[0] = flagHalt;
}

void Vehicle::PackWaypointAutoSw(uint16_t idBank, uint16_t idWp, uint8_t switchMode)
{
    msgSend.tyMsg0 = COMMAND_BANK;
    msgSend.tyMsg1 = WAYPOINT_AUTO_SW;
    msgSend.idSource = idSource;
    msgSend.idTarget = idTarget;
    memset(msgSend.payload, 0, sizeof(msgSend.payload));
    memcpy(msgSend.payload, &idBank, 2);
    memcpy(msgSend.payload + 2, &idWp, 2);
    msgSend.payload[4] = switchMode;
    _sendShenHangCommandWorker();
}
