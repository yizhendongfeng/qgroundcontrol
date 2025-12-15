/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "MultiVehicleManager.h"
#include "GPSManager.h"
#include "GPSRtk.h"
#include "MAVLinkProtocol.h"
#include "QGCApplication.h"
#include "ParameterManager.h"
#include "SettingsManager.h"
#include "MavlinkSettings.h"
#include "FirmwareUpgradeSettings.h"
#include "CloudServerSettings.h"
#include "QGCCorePlugin.h"
#include "QGCOptions.h"
#include "LinkManager.h"
#include "Vehicle.h"
#include "VehicleBatteryFactGroup.h"
#include "VehicleLinkManager.h"
#include "Autotune.h"
#include "LinkInterface.h"
#include "RemoteIDManager.h"
#include "VehicleObjectAvoidance.h"
#include "TrajectoryPoints.h"
#include "QmlObjectListModel.h"
#include "VideoManager.h"

#ifdef Q_OS_IOS
#include "MobileScreenMgr.h"
#elif defined(Q_OS_ANDROID)
#include "AndroidInterface.h"
#endif
#include "QGCLoggingCategory.h"

#include <QtCore/qapplicationstatic.h>
#include <QtCore/QTimer>
#include <QtQml/QQmlEngine>
#include <QtMqtt/QMqttClient>
#include <QUuid>
#include <QDebug>

QGC_LOGGING_CATEGORY(MultiVehicleManagerLog, "qgc.vehicle.multivehiclemanager")

Q_APPLICATION_STATIC(MultiVehicleManager, _multiVehicleManagerInstance);

MultiVehicleManager::MultiVehicleManager(QObject *parent)
    : QObject(parent)
    , _gcsHeartbeatTimer(new QTimer(this))
    , _vehicles(new QmlObjectListModel(this))
    , _selectedVehicles(new QmlObjectListModel(this))
    , _mqttClient(new QMqttClient(this))
    , _timerSendOsd(new QTimer(this))
{
    // qCDebug(MultiVehicleManagerLog) << Q_FUNC_INFO << this;
}

MultiVehicleManager::~MultiVehicleManager()
{
    // qCDebug(MultiVehicleManagerLog) << Q_FUNC_INFO << this;
}

MultiVehicleManager *MultiVehicleManager::instance()
{
    return _multiVehicleManagerInstance();
}

void MultiVehicleManager::registerQmlTypes()
{
    (void) qmlRegisterUncreatableType<MultiVehicleManager>      ("QGroundControl.MultiVehicleManager",  1, 0, "MultiVehicleManager",    "Reference only");
    (void) qmlRegisterUncreatableType<Vehicle>                  ("QGroundControl.Vehicle",              1, 0, "Vehicle",                "Reference only");
    (void) qmlRegisterUncreatableType<VehicleLinkManager>       ("QGroundControl.Vehicle",              1, 0, "VehicleLinkManager",     "Reference only");
    (void) qmlRegisterUncreatableType<Autotune>                 ("QGroundControl.Vehicle",              1, 0, "Autotune",               "Reference only");
    (void) qmlRegisterUncreatableType<RemoteIDManager>          ("QGroundControl.Vehicle",              1, 0, "RemoteIDManager",        "Reference only");
    (void) qmlRegisterUncreatableType<TrajectoryPoints>         ("QGroundControl.FlightMap",            1, 0, "TrajectoryPoints",       "Reference only");
    (void) qmlRegisterUncreatableType<VehicleObjectAvoidance>   ("QGroundControl.Vehicle",              1, 0, "VehicleObjectAvoidance", "Reference only");
    (void) qRegisterMetaType<Vehicle::MavCmdResultFailureCode_t>("MavCmdResultFailureCode_t");
}

void MultiVehicleManager::init()
{
    if (_initialized) {
        return;
    }

    _offlineEditingVehicle = new Vehicle(Vehicle::MAV_AUTOPILOT_TRACK, Vehicle::MAV_TYPE_TRACK, this);

    (void) connect(MAVLinkProtocol::instance(), &MAVLinkProtocol::vehicleHeartbeatInfo, this, &MultiVehicleManager::_vehicleHeartbeatInfo);

    _gcsHeartbeatTimer->setInterval(kGCSHeartbeatRateMSecs);
    _gcsHeartbeatTimer->setSingleShot(false);
    (void) connect(_gcsHeartbeatTimer, &QTimer::timeout, this, &MultiVehicleManager::_sendGCSHeartbeat);
    _gcsHeartbeatTimer->start();

    connect(_mqttClient, &QMqttClient::stateChanged, this, [&](QMqttClient::ClientState state){
        _mqttConnected = state == QMqttClient::Connected;
        qDebug() << "_mqttClient state:" << state;
        switch (state) {
            case QMqttClient::Connected:
                // 发送更新拓扑信息：地面站→无人机
                updateDevicesInCloudServer();

            case QMqttClient::Connecting:
                break;
            case QMqttClient::Disconnected:
                // connectToMqttHost(); // 重新继续连接
                break;
            default:
                break;
        }
        emit mqttConnectedChanged(_mqttConnected);
    });
    connect(_mqttClient, &QMqttClient::messageReceived, this, &MultiVehicleManager::_receiveMqttFromServer);

    _timerSendOsd->setInterval(500);
    connect(_timerSendOsd, &QTimer::timeout, this, &MultiVehicleManager::_sendOsdToServer);
    _initialized = true;
}

void MultiVehicleManager::updateDevicesInCloudServer()
{
    if (!_mqttConnected) {
        _timerSendOsd->stop();
        return;
    }
    QJsonObject jsonDevices;
    jsonDevices["tid"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
    jsonDevices["bid"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
    jsonDevices["timestamp"] = QDateTime::currentMSecsSinceEpoch();
    jsonDevices["method"] = "update_topo";
    // jsonDevices["gateway"] = SettingsManager::instance()->cloudServerSettings()->gcsSn()->rawValueString();
    QJsonObject jsonData;
    jsonData["domain"] = 2;
    jsonData["type"] = 144;
    jsonData["sub_type"] = 0;
    jsonData["device_secret"] = "device_secret";
    jsonData["nonce"] = "nonce";
    jsonData["version"] = 1;
    QJsonArray jsonArraySubDevices;
    QJsonObject jsonObjSubDevice;
    jsonObjSubDevice["sn"] = SettingsManager::instance()->cloudServerSettings()->droneSn()->rawValueString();
    jsonObjSubDevice["domain"] = 0;
    jsonObjSubDevice["type"] = 77;
    jsonObjSubDevice["sub_type"] = 0;
    jsonObjSubDevice["index"] = "A";
    jsonObjSubDevice["device_secret"] = "secret";
    jsonData["nonce"] = "nonce";
    jsonData["version"] = 1;
    jsonArraySubDevices.append(jsonObjSubDevice);
    jsonData["sub_devices"] = jsonArraySubDevices;
    jsonDevices["data"] = jsonData;
    QJsonDocument jsonDoc{jsonDevices};
    QString topic = "sys/product/" + SettingsManager::instance()->cloudServerSettings()->gcsSn()->rawValueString() + "/status";
    _mqttClient->subscribe("sys/product/" + SettingsManager::instance()->cloudServerSettings()->gcsSn()->rawValueString() + "/status_reply");
    qint32 result = _mqttClient->publish(QMqttTopicName(topic), jsonDoc.toJson(QJsonDocument::Compact));
    // qDebug() << "updateDevicesInCloudServer() topic:" << topic << ", json:" << jsonDoc.toJson(QJsonDocument::Compact);
}

void MultiVehicleManager::_vehicleHeartbeatInfo(LinkInterface* link, int vehicleId, int componentId, int vehicleFirmwareType, int vehicleType)
{
    if (componentId != MAV_COMP_ID_AUTOPILOT1) {
        // Don't create vehicles for components other than the autopilot
        qCDebug(MultiVehicleManagerLog) << "Ignoring heartbeat from unknown component port:vehicleId:componentId:fwType:vehicleType"
                                        << link->linkConfiguration()->name()
                                        << vehicleId
                                        << componentId
                                        << vehicleFirmwareType
                                        << vehicleType;
        return;
    }

#ifndef QGC_NO_ARDUPILOT_DIALECT
    // When you flash a new ArduCopter it does not set a FRAME_CLASS for some reason. This is the only ArduPilot variant which
    // works this way. Because of this the vehicle type is not known at first connection. In order to make QGC work reasonably
    // we assume ArduCopter for this case.
    if ((vehicleType == MAV_TYPE_GENERIC) && (vehicleFirmwareType == MAV_AUTOPILOT_ARDUPILOTMEGA)) {
        vehicleType = MAV_TYPE_QUADROTOR;
    }
#endif

    switch (vehicleType) {
    case MAV_TYPE_GCS:
    case MAV_TYPE_ONBOARD_CONTROLLER:
    case MAV_TYPE_GIMBAL:
    case MAV_TYPE_ADSB:
        // These are not vehicles, so don't create a vehicle for them
        return;
    default:
        break;
    }

    if ((_vehicles->count() > 0) && !QGCCorePlugin::instance()->options()->multiVehicleEnabled()) {
        return;
    }

    if (_ignoreVehicleIds.contains(vehicleId) || getVehicleById(vehicleId) || (vehicleId == 0)) {
        return;
    }

    qCDebug(MultiVehicleManagerLog) << "Adding new vehicle link:vehicleId:componentId:vehicleFirmwareType:vehicleType "
                                    << link->linkConfiguration()->name()
                                    << vehicleId
                                    << componentId
                                    << vehicleFirmwareType
                                    << vehicleType;

    if (vehicleId == MAVLinkProtocol::instance()->getSystemId()) {
        qgcApp()->showAppMessage(tr("Warning: A vehicle is using the same system id as %1: %2").arg(QCoreApplication::applicationName()).arg(vehicleId));
    }

    Vehicle *const vehicle = new Vehicle(link, vehicleId, componentId, (MAV_AUTOPILOT)vehicleFirmwareType, (MAV_TYPE)vehicleType, this);
    (void) connect(vehicle, &Vehicle::requestProtocolVersion, this, &MultiVehicleManager::_requestProtocolVersion);
    (void) connect(vehicle->vehicleLinkManager(), &VehicleLinkManager::allLinksRemoved, this, &MultiVehicleManager::_deleteVehiclePhase1);
    (void) connect(vehicle->parameterManager(), &ParameterManager::parametersReadyChanged, this, &MultiVehicleManager::_vehicleParametersReadyChanged);

    _vehicles->append(vehicle);

    // Send QGC heartbeat ASAP, this allows PX4 to start accepting commands
    _sendGCSHeartbeat();

    SettingsManager::instance()->firmwareUpgradeSettings()->defaultFirmwareType()->setRawValue(vehicleFirmwareType);

    emit vehicleAdded(vehicle);

    if (_vehicles->count() > 1) {
        qgcApp()->showAppMessage(tr("Connected to Vehicle %1").arg(vehicleId));
    } else {
        setActiveVehicle(vehicle);
    }

#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
    if (_vehicles->count() == 1) {
        qCDebug(MultiVehicleManagerLog) << "keepScreenOn";
        #if defined(Q_OS_ANDROID)
            AndroidInterface::setKeepScreenOn(true);
        #elif defined(Q_OS_IOS)
            MobileScreenMgr::setKeepScreenOn(true);
        #endif
    }
#endif
}

void MultiVehicleManager::_requestProtocolVersion(unsigned version) const
{
    if (_vehicles->count() == 0) {
        MAVLinkProtocol::instance()->setVersion(version);
        return;
    }

    unsigned maxversion = 0;
    for (int i = 0; i < _vehicles->count(); i++) {
        const Vehicle *const vehicle = qobject_cast<const Vehicle*>(_vehicles->get(i));
        if (vehicle && (vehicle->maxProtoVersion() > maxversion)) {
            maxversion = vehicle->maxProtoVersion();
        }
    }

    if (MAVLinkProtocol::instance()->getCurrentVersion() != maxversion) {
        MAVLinkProtocol::instance()->setVersion(maxversion);
    }
}

void MultiVehicleManager::_sendOsdToServer()
{
    if(!_mqttConnected)
        return;
    // 发送地面站状态信息
    QJsonObject jsonGcs;
    jsonGcs["tid"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
    jsonGcs["bid"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
    jsonGcs["timestamp"] = QDateTime::currentMSecsSinceEpoch();
    jsonGcs["gateway"] = SettingsManager::instance()->cloudServerSettings()->gcsSn()->rawValueString();
    QJsonObject jsonObjGcsData;
    jsonObjGcsData["capacity_percent"] = 100;

    // 发送无人机状态信息
    qDebug() << "vehicles->count:" << _vehicles->count();
    for(int i = 0; i < _vehicles->count(); i++) {
        QObject* obj = _vehicles->get(i);
        if (obj) {
            Vehicle* vehicle = qobject_cast<Vehicle*>(obj);
            QJsonObject jsonDrone;
            jsonDrone["tid"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
            jsonDrone["bid"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
            jsonDrone["timestamp"] = QDateTime::currentMSecsSinceEpoch();
            jsonDrone["gateway"] = SettingsManager::instance()->cloudServerSettings()->gcsSn()->rawValueString();
            QJsonObject jsonObjDroneData;
            if (!vehicle->flying()) {
                // {"0":"待机","1":"起飞准备","2":"起飞准备完毕","3":"手动飞行","4":"自动起飞","5":"航线飞行","6":"全景拍照","7":"智能跟随","8":"ADS-B 躲避","9":"自动返航","10":"自动降落","11":"强制降落","12":"三桨叶降落","13":"升级中","14":"未连接","15":"APAS","16":"虚拟摇杆状态","17":"指令飞行","18":"空中 RTK 收敛模式"}
                jsonObjDroneData["mode_code"] = 0;

            } else {
                if (vehicle->flightMode() == "Ready") {
                    jsonObjDroneData["mode_code"] = 2;
                } else if (vehicle->flightMode() == "Takeoff") {
                    jsonObjDroneData["mode_code"] = 3;
                } else if (vehicle->flightMode() == "Position") {
                    jsonObjDroneData["mode_code"] = 4;
                } else if (vehicle->flightMode() == "Mission") {
                    jsonObjDroneData["mode_code"] = 5;
                } else if (vehicle->flightMode() == "Return") {
                    jsonObjDroneData["mode_code"] = 9;
                } else if (vehicle->flightMode() == "Land") {
                    jsonObjDroneData["mode_code"] = 10;
                } else {
                    jsonObjDroneData["mode_code"] = 0;
                }
            }
            QJsonObject jsonPositionState;
            switch (vehicle->gpsFactGroup()->getFact("lock")->enumIndex()) {
                //"None,None,2D Lock,3D Lock,3D DGPS Lock,3D RTK GPS Lock (float),3D RTK GPS Lock (fixed),Static (fixed)",
                case 0:
                case 1:
                    jsonPositionState["is_fixed"] = 0;
                    break;
                case 2:
                    jsonPositionState["is_fixed"] = 1;
                    break;
                case 3:
                case 4:
                case 5:
                    jsonPositionState["is_fixed"] = 2;
                    break;
            }
            jsonPositionState["gps_number"] = vehicle->gpsFactGroup()->getFact("count")->rawValue().toInt();
            GPSRtk * gpsRtk = GPSManager::instance()->gpsRtk();
            jsonPositionState["rtk_number"] = gpsRtk->connected() ? gpsRtk->gpsRtkFactGroup()->getFact("numSatellites")->rawValue().toInt() : 0;
            jsonObjDroneData["position_state"] = jsonPositionState;
            QJsonObject jsonObjBattery;
            VehicleBatteryFactGroup *batteryFactGroup;
            if (vehicle->batteries()->count() > 0) { // 获取第一个电池组
                batteryFactGroup = qobject_cast<VehicleBatteryFactGroup *>(vehicle->batteries()->get(0));
                jsonObjBattery["capacity_percent"] = batteryFactGroup->percentRemaining()->rawValue().toDouble();
                jsonObjBattery["remain_flight_time"] = batteryFactGroup->timeRemaining()->rawValue().toDouble();
            }
            jsonObjDroneData["battery"] = jsonObjBattery;
            jsonObjDroneData["home_distance"] = vehicle->distanceToHome()->rawValue().toDouble();
            jsonObjDroneData["home_latitude"] = vehicle->homePosition().latitude();
            jsonObjDroneData["home_longitude"] = vehicle->homePosition().longitude();
            jsonObjDroneData["attitude_head"] = vehicle->heading()->rawValue().toInt();
            jsonObjDroneData["attitude_roll"] = vehicle->roll()->rawValue().toDouble();
            jsonObjDroneData["attitude_pitch"] = vehicle->pitch()->rawValue().toDouble();
            jsonObjDroneData["elevation"] = vehicle->altitudeRelative()->rawValue().toDouble();
            jsonObjDroneData["height"] = vehicle->altitudeAMSL()->rawValue().toDouble();
            jsonObjDroneData["latitude"] = vehicle->latitude();
            jsonObjDroneData["longitude"] = vehicle->longitude();
            jsonObjDroneData["vertical_speed"] = vehicle->climbRate()->rawValue().toDouble();
            jsonObjDroneData["horizontal_speed"] = vehicle->groundSpeed()->rawValue().toDouble();
            jsonObjDroneData["firmware_version"] = QString::number(vehicle->firmwareMajorVersion()) + "." +
                QString::number(vehicle->firmwareMinorVersion()) + "." +
                QString::number(vehicle->firmwarePatchVersion()) + ".";
            jsonObjDroneData["wind_direction"] = vehicle->windFactGroup()->getFact("direction")->rawValue().toDouble();
            jsonObjDroneData["wind_speed"] = vehicle->windFactGroup()->getFact("speed")->rawValue().toDouble();
            jsonObjGcsData["latitude"] = vehicle->homePosition().latitude();
            jsonObjGcsData["longitude"] = vehicle->homePosition().longitude();

            jsonDrone["data"] = jsonObjDroneData;
            QJsonDocument jsonDocDrone{jsonDrone};
            QString topic = "thing/product/" + SettingsManager::instance()->cloudServerSettings()->droneSn()->rawValueString() + "/osd";
            int result = _mqttClient->publish(QMqttTopicName(topic),
                // R"(
                //     {
                //         "bid": "df43a2cf-cc8c-4634-a958-ee808c260f23",
                //         "data": {
                //             "battery": {
                //                 "capacity_percent": 1
                //             },
                //             "mode_code": 0,
                //             "position_state": {
                //                 "gps_number": 8,
                //                 "is_fixed": 2
                //             }
                //         },
                //         "gateway": "dgcs001",
                //         "tid": "b5382804-e04f-4c7c-8517-62b381301080",
                //         "timestamp": 1762187092880
                //     }
                // )"); //
                jsonDocDrone.toJson(QJsonDocument::Compact));
            qDebug() << "drone publish result: " << result << "topic:" << topic << jsonDocDrone.toJson();
        }
    }

    jsonGcs["data"] = jsonObjGcsData;
    QJsonDocument jsonDocGcs{jsonGcs};
    QString topic = "thing/product/" + SettingsManager::instance()->cloudServerSettings()->gcsSn()->rawValueString() + "/osd";
    _mqttClient->publish(QMqttTopicName(topic), jsonDocGcs.toJson());
    // qDebug() << "dgcs: " << topic << jsonDocGcs.toJson();
}

void MultiVehicleManager::_receiveMqttFromServer(const QByteArray &message, const QMqttTopicName &topic)
{
    QJsonDocument jsonDocMsg = QJsonDocument::fromJson(message);
    qDebug() << "_receiveMqttFromServer: " << message << "topic:" << topic;
    if (topic == "sys/product/" + SettingsManager::instance()->cloudServerSettings()->gcsSn()->rawValueString() + "/status_reply") {
        // 收到拓扑更新成功信息
        _timerSendOsd->start();
    }

}

void MultiVehicleManager::_deleteVehiclePhase1(Vehicle *vehicle)
{
    qCDebug(MultiVehicleManagerLog) << Q_FUNC_INFO << vehicle;

    bool found = false;
    for (int i = 0; i < _vehicles->count(); i++) {
        if (_vehicles->get(i) == vehicle) {
            (void) _vehicles->removeAt(i);
            found = true;
            break;
        }
    }

    if (!found) {
        qCWarning(MultiVehicleManagerLog) << "Vehicle not found in map!";
    }

    deselectVehicle(vehicle->id());

    _setActiveVehicleAvailable(false);
    _setParameterReadyVehicleAvailable(false);
    emit vehicleRemoved(vehicle);
    vehicle->prepareDelete();

#if defined(Q_OS_ANDROID) || defined (Q_OS_IOS)
    if (_vehicles->count() == 0) {
        qCDebug(MultiVehicleManagerLog) << "restoreScreenOn";
        #if defined(Q_OS_ANDROID)
            AndroidInterface::setKeepScreenOn(false);
        #elif defined(Q_OS_IOS)
            MobileScreenMgr::setKeepScreenOn(false);
        #endif
    }
#endif

    // We must let the above signals flow through the system as well as get back to the main loop event queue
    // before we can actually delete the Vehicle. The reason is that Qml may be holding on to references to it.
    // Even though the above signals should unload any Qml which has references, that Qml will not be destroyed
    // until we get back to the main loop. So we set a short timer which will then fire after Qt has finished
    // doing all of its internal nastiness to clean up the Qml. This works for both the normal running case
    // as well as the unit testing case which of course has a different signal flow!
    QTimer::singleShot(20, this, [this, vehicle]() {
        _deleteVehiclePhase2(vehicle);
    });
}

void MultiVehicleManager::_deleteVehiclePhase2(Vehicle *vehicle)
{
    qCDebug(MultiVehicleManagerLog) << Q_FUNC_INFO << vehicle;

    /// Qml has been notified of vehicle about to go away and should be disconnected from it by now.
    /// This means we can now clear the active vehicle property and delete the Vehicle for real.

    Vehicle *newActiveVehicle = nullptr;
    if (_vehicles->count() > 0) {
        newActiveVehicle = qobject_cast<Vehicle*>(_vehicles->get(0));
    }

    _setActiveVehicle(newActiveVehicle);

    if (_activeVehicle) {
        _setActiveVehicleAvailable(true);
        if (_activeVehicle->parameterManager()->parametersReady()) {
            _setParameterReadyVehicleAvailable(true);
        }
    }

    vehicle->deleteLater();
}

void MultiVehicleManager::setActiveVehicle(Vehicle *vehicle)
{
    qCDebug(MultiVehicleManagerLog) << Q_FUNC_INFO << vehicle;

    if (vehicle != _activeVehicle) {
        if (_activeVehicle) {
            // The sequence of signals is very important in order to not leave Qml elements connected
            // to a non-existent vehicle.

            // First we must signal that there is no active vehicle available. This will disconnect
            // any existing ui from the currently active vehicle.
            _setActiveVehicleAvailable(false);
            _setParameterReadyVehicleAvailable(false);
            disconnect(_activeVehicle, &Vehicle::gcuRequiredDataChanged, VideoManager::instance()->gcu(), &GCU::receiveVehicleMessage);
        }

        QTimer::singleShot(20, this, [this, vehicle]() {
            _setActiveVehiclePhase2(vehicle);
        });
    }
}

void MultiVehicleManager::_setActiveVehiclePhase2(Vehicle *vehicle)
{
    qCDebug(MultiVehicleManagerLog) << Q_FUNC_INFO << vehicle;

    _setActiveVehicle(vehicle);

    if (_activeVehicle) {
        _setActiveVehicleAvailable(true);

        if (_activeVehicle->parameterManager()->parametersReady()) {
            _setParameterReadyVehicleAvailable(true);
        }
    }
}

void MultiVehicleManager::_vehicleParametersReadyChanged(bool parametersReady)
{
    ParameterManager *const paramMgr = qobject_cast<ParameterManager*>(sender());
    if (!paramMgr) {
        return;
    }

    if (paramMgr->vehicle() == _activeVehicle) {
        _setParameterReadyVehicleAvailable(parametersReady);
    }
}

void MultiVehicleManager::_sendGCSHeartbeat()
{
    if (!SettingsManager::instance()->mavlinkSettings()->sendGCSHeartbeat()->rawValue().toBool()) {
        return;
    }

    const QList<SharedLinkInterfacePtr> sharedLinks = LinkManager::instance()->links();
    for (const SharedLinkInterfacePtr link: sharedLinks) {
        if (!link->isConnected()) {
            continue;
        }

        const SharedLinkConfigurationPtr linkConfiguration = link->linkConfiguration();
        if (linkConfiguration->isHighLatency()) {
            continue;
        }

        mavlink_message_t message{};
        (void) mavlink_msg_heartbeat_pack_chan(
            MAVLinkProtocol::instance()->getSystemId(),
            MAVLinkProtocol::instance()->getComponentId(),
            link->mavlinkChannel(),
            &message,
            MAV_TYPE_GCS,
            MAV_AUTOPILOT_INVALID,
            MAV_MODE_MANUAL_ARMED,
            0,
            MAV_STATE_ACTIVE
        );

        uint8_t buffer[MAVLINK_MAX_PACKET_LEN];
        const uint16_t len = mavlink_msg_to_send_buffer(buffer, &message);
        (void) link->writeBytesThreadSafe(reinterpret_cast<const char*>(buffer), len);
    }
}

void MultiVehicleManager::selectVehicle(int vehicleId)
{
    if(!_vehicleSelected(vehicleId)) {
        Vehicle *const vehicle = getVehicleById(vehicleId);
        _selectedVehicles->append(vehicle);
        return;
    }
}

void MultiVehicleManager::deselectVehicle(int vehicleId)
{
    for (int i = 0; i < _selectedVehicles->count(); i++) {
        Vehicle *const vehicle = qobject_cast<Vehicle*>(_selectedVehicles->get(i));
        if (vehicle->id() == vehicleId) {
            _selectedVehicles->removeAt(i);
            return;
        }
    }
}

void MultiVehicleManager::deselectAllVehicles()
{
    _selectedVehicles->clear();
}

void MultiVehicleManager::connectToMqttHost()
{
    const QString hostNamePortTemp = SettingsManager::instance()->cloudServerSettings()->mqttHost()->rawValue().toString();
    const QString mqttUserNameTemp = SettingsManager::instance()->cloudServerSettings()->mqttUserName()->rawValueString();
    const QString mqttUserPassowrdTemp = SettingsManager::instance()->cloudServerSettings()->mqttUserPassword()->rawValueString();
    if (_mqttClient) {
        if (!hostNamePortTemp.contains(_mqttClient->hostname() + ":" + QString::number(_mqttClient->port()))
            || mqttUserNameTemp != _mqttClient->username()
            || mqttUserPassowrdTemp != _mqttClient->password()
            ) {
            if (_mqttConnected) {
                // _mqttClient->hostname();
                // _mqttClient->port();
                // _mqttClient->username();
                // _mqttClient->password();
                _mqttClient->disconnectFromHost();
                _timerSendOsd->stop();
            } else {
                int prefixEndIndex = hostNamePortTemp.indexOf("://") + 3; // "://" 长度为 3，加 3 得到前缀结束索引
                if (prefixEndIndex > 2) { // 确保找到 "://"
                    QString result = hostNamePortTemp.mid(prefixEndIndex);
                    qDebug() << "hostNamePortTemp.mid(prefixEndIndex):" << result; // 输出："192.168.0.1:5000"

                    QStringList strList = result.split(':');
                    if (strList.count() < 2)
                        return;
                    _mqttClient->setUsername(mqttUserNameTemp);
                    _mqttClient->setPassword(mqttUserPassowrdTemp);
                    _mqttClient->setHostname(strList[0]);
                    _mqttClient->setPort(strList[1].toInt());
                    qDebug() <<"hostNamePort:" <<  strList << _mqttClient->hostname() << _mqttClient->port();
                    _mqttClient->connectToHost();
                }
            }
        } else {
            qDebug() << "MultiVehicleManager::connectToMqttHost(): same host already connected!";
        }


    }
}

bool MultiVehicleManager::_vehicleSelected(int vehicleId)
{
    for (int i = 0; i < _selectedVehicles->count(); i++) {
        Vehicle *const vehicle = qobject_cast<Vehicle*>(_selectedVehicles->get(i));
        if (vehicle->id() == vehicleId) {
            return true;
        }
    }
    return false;
}

Vehicle *MultiVehicleManager::getVehicleById(int vehicleId) const
{
    for (int i = 0; i < _vehicles->count(); i++) {
        Vehicle *const vehicle = qobject_cast<Vehicle*>(_vehicles->get(i));
        if (vehicle->id() == vehicleId) {
            return vehicle;
        }
    }

    return nullptr;
}

void MultiVehicleManager::_setActiveVehicle(Vehicle *vehicle)
{
    // 连接新无人机的惯导数据到吊舱
    if (_activeVehicle) {
        // disconnect(_activeVehicle, )
    }
    if (vehicle != _activeVehicle) {
        _activeVehicle = vehicle;
        emit activeVehicleChanged(vehicle);
        connect(vehicle, &Vehicle::gcuRequiredDataChanged, VideoManager::instance()->gcu(), &GCU::receiveVehicleMessage);
    }
}

void MultiVehicleManager::_setActiveVehicleAvailable(bool activeVehicleAvailable)
{
    if (activeVehicleAvailable != _activeVehicleAvailable) {
        _activeVehicleAvailable = activeVehicleAvailable;
        emit activeVehicleAvailableChanged(activeVehicleAvailable);
    }
}

void MultiVehicleManager::_setParameterReadyVehicleAvailable(bool parametersReady)
{
    if (parametersReady != _parameterReadyVehicleAvailable) {
        _parameterReadyVehicleAvailable = parametersReady;
        parameterReadyVehicleAvailableChanged(parametersReady);
    }
}
