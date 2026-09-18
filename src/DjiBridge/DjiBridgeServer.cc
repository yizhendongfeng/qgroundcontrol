/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

/// @file
/// @brief DjiBridge 本地模拟服务实现
///
/// 通过本地 HTTP 服务接收网页同步 XHR 请求，模拟 window.djiBridge 全部方法。
/// 管理 QWebEngineProfile 注入脚本，以及 MQTT 5.0 连接和 C++→JS 回调。

#include "DjiBridgeServer.h"

#include <QtCore/QFile>
#include <QtCore/QTextStream>
#include <QtCore/QDir>
#include <QtCore/QDebug>
#include <QtCore/QCoreApplication>
#include <QtCore/QTimer>
#include <QtWebEngineCore/QWebEngineScriptCollection>
#include "SettingsManager.h"
#include "CloudServerSettings.h"

#include "Vehicle.h"
#include "VideoSettings.h"
#include "VideoManager.h"
#include "GPSManager.h"
#include "GPSRtk.h"
#include "VehicleBatteryFactGroup.h"

DjiBridgeServer::DjiBridgeServer(QObject* parent) :
    QObject(parent),
    _server(new QTcpServer(this)),
    _port(0),
    _timerSendOsd(new QTimer(this)),
    _mqttClient(new QMqttClient(this))
{
    connect(_server, &QTcpServer::newConnection,
            this, &DjiBridgeServer::onNewConnection);

    connect(_mqttClient, &QMqttClient::stateChanged,
            this, &DjiBridgeServer::onMqttStateChanged);
    connect(_mqttClient, &QMqttClient::errorChanged,
            this, &DjiBridgeServer::onMqttErrorChanged);
    connect(_mqttClient, &QMqttClient::messageReceived,
            this, &DjiBridgeServer::_receiveMqttFromServer);
    _timerSendOsd->setInterval(500);
    connect(_timerSendOsd, &QTimer::timeout, this, &DjiBridgeServer::_sendOsdToServer);
}

DjiBridgeServer::~DjiBridgeServer()
{
    if (_mqttClient->state() == QMqttClient::Connected) {
        _mqttClient->disconnectFromHost();
    }
    if (_server->isListening()) {
        _server->close();
    }
}

void DjiBridgeServer::init()
{
    if (_initialized) {
        return;
    }
    _initialized = true;

    // 初始化管理器引用（此时 SettingsManager、VideoManager 已在 _initForNormalAppBoot 中就绪）
    _videoSettings = SettingsManager::instance()->videoSettings();
    _videoManager = VideoManager::instance();

    // 1. 启动本地 HTTP 服务
    if (!start()) {
        qWarning() << "[DjiBridge] Failed to start HTTP server";
        return;
    }

    // 2. 创建 WebEngineProfile（无痕模式，不含 storageName 即无痕）
    // ---------- 向默认 WebEngineProfile 注入脚本 ----------
    // 注意1：QML 中 WebEngineScript 是不可创建类型（uncreatable type），
    //        必须在 C++ 端创建 QWebEngineScript 并插入 profile 的 script collection。
    // 注意2：直接使用 defaultProfile()，WebEngineView 默认就用它，避免自定义 profile
    //        作为 context property 传入 QML 后绑定失败导致脚本不生效的问题。
    _profile = QWebEngineProfile::defaultProfile();// new QWebEngineProfile(QStringLiteral("DjiBridgeProfile"), this);
    // _profile->setPersistentStoragePath(QDir::tempPath() + "/qgc_dji_webengine");
    // _profile->setPersistentCookiesPolicy(QWebEngineProfile::ForcePersistentCookies);

    // 3. 创建注入脚本，在 DocumentCreation 阶段执行
    QWebEngineScript script;
    script.setName(QStringLiteral("djiBridgeInject"));
    script.setInjectionPoint(QWebEngineScript::DocumentCreation);
    script.setWorldId(QWebEngineScript::MainWorld);
    script.setRunsOnSubFrames(false);
    script.setSourceCode(injectionScript());

    _profile->scripts()->insert(script);

    qInfo() << "[DjiBridge] Initialized. HTTP port:" << _port
            << "Profile:" << _profile->storageName();
}

bool DjiBridgeServer::start()
{
    if (!_server->listen(QHostAddress::LocalHost, 0)) {
        qWarning() << "[DjiBridge] Failed to start server:" << _server->errorString();
        return false;
    }
    _port = _server->serverPort();
    qInfo() << "[DjiBridge] Server started on 127.0.0.1:" << _port;
    return true;
}

quint16 DjiBridgeServer::port() const
{
    return _port;
}

QString DjiBridgeServer::injectionScript() const
{
    QFile file(":/djibridge/bridge_inject.js");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "[DjiBridge] Failed to open injection script resource";
        return QString();
    }
    QString script = QTextStream(&file).readAll();
    file.close();
    script.replace("__PORT__", QString::number(_port));
    return script;
}

void DjiBridgeServer::updateDevicesInCloudServer()
{
    if (!_thingConnected) {
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
    jsonData["workspace_id"] = _workspaceId;//SettingsManager::instance()->cloudServerSettings()->workSpaceId()->rawValueString();
    QJsonArray jsonArraySubDevices;
    if (_activeVehicle) {
        QJsonObject jsonObjSubDevice;
        jsonObjSubDevice["sn"] = SettingsManager::instance()->cloudServerSettings()->droneSn()->rawValueString();
        jsonObjSubDevice["domain"] = 0;
        jsonObjSubDevice["type"] = 77;
        jsonObjSubDevice["sub_type"] = 0;
        jsonObjSubDevice["index"] = "A";
        jsonObjSubDevice["device_secret"] = "secret";
        jsonArraySubDevices.append(jsonObjSubDevice);
    }
    jsonData["nonce"] = "nonce";
    jsonData["version"] = 1;
    jsonData["sub_devices"] = jsonArraySubDevices;
    jsonDevices["data"] = jsonData;
    QJsonDocument jsonDoc{jsonDevices};
    QString topic = "sys/product/" + SettingsManager::instance()->cloudServerSettings()->gcsSn()->rawValueString() + "/status";
    _mqttClient->subscribe("sys/product/" + SettingsManager::instance()->cloudServerSettings()->gcsSn()->rawValueString() + "/status_reply");
    _mqttClient->subscribe("thing/product/" + SettingsManager::instance()->cloudServerSettings()->gcsSn()->rawValueString() + "/services");
    qint32 result = _mqttClient->publish(QMqttTopicName(topic), jsonDoc.toJson(QJsonDocument::Compact));
    qDebug() << "updateDevicesInCloudServer() topic:" << topic << ", json:" << jsonDoc.toJson(QJsonDocument::Compact);
}

void DjiBridgeServer::sendMqttReply(const QString &topicPrefix, const QString &topicSuffix, const QString &tid, const QString &bid, const QString &method, const int &result)
{
    if (!_thingConnected) {
        qDebug() << "sendMqttReply mqtt not connected";
        return;
    }
    QJsonObject jsonObjectReply;
    jsonObjectReply["tid"] = tid;
    jsonObjectReply["bid"] = bid;
    jsonObjectReply["timestamp"] = QDateTime::currentMSecsSinceEpoch();
    jsonObjectReply["method"] = method;
    QJsonObject jsonObjectData;
    jsonObjectData["result"] = result;
    jsonObjectReply["data"] = jsonObjectData;
    QString topic = topicPrefix + "/product/" + SettingsManager::instance()->cloudServerSettings()->gcsSn()->rawValueString() + "/" + topicSuffix;
    QJsonDocument jsonDoc(jsonObjectReply);
    qint32 writeCount = _mqttClient->publish(QMqttTopicName(topic), jsonDoc.toJson(QJsonDocument::Compact), 1);
    qDebug() << "sendMqttReply() topic" << topic << jsonDoc.toJson(QJsonDocument::Compact) << "writeCount:" << writeCount;
}

void DjiBridgeServer::_sendOsdToServer()
{
    if (!_thingConnected) {
        return;
    }
    // 发送地面站状态信息
    QJsonObject jsonGcsOsd;
    jsonGcsOsd["tid"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
    jsonGcsOsd["bid"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
    jsonGcsOsd["timestamp"] = QDateTime::currentMSecsSinceEpoch();
    jsonGcsOsd["gateway"] = SettingsManager::instance()->cloudServerSettings()->gcsSn()->rawValueString();
    QJsonObject jsonObjGcsData;
    // // 直播能力
    // QJsonObject liveCapacity;
    // liveCapacity["available_video_number"] = 1;
    // liveCapacity["coexist_video_number_max"] = 1;
    // QJsonArray deviceList;
    // QJsonObject device;
    // device["sn"] = "D80-pro";
    // device["available_video_number"] = 1;
    // device["coexist_video_number_max"] = 1;

    // QJsonArray cameraList;
    // QJsonObject camera;
    // camera["camera_index"] = "66-0-0";
    // camera["available_video_number"] = 1;
    // camera["coexist_video_number_max"] = 1;

    // QJsonArray videoList;
    // QJsonObject video;
    // video["video_index"] = "1";
    // video["video_type"] = "HD";
    // video["switchable_video_types"] = QJsonArray{"visual light", "infrared camera"};
    // videoList.append(video);
    // camera["video_list"] = videoList;
    // cameraList.append(camera);
    // device["camera_list"] = cameraList;
    // deviceList.append(device);
    // liveCapacity["device_list"] = deviceList;
    // jsonObjGcsData["live_capacity"] = liveCapacity;

    jsonObjGcsData["capacity_percent"] = 100;

    // 直播信息
    QJsonArray liveStatus;
    QJsonObject videoLive;
    //{sn}/{camera_index}/{video_index}
    videoLive["video_id"] = SettingsManager::instance()->cloudServerSettings()->gcsSn()->rawValueString() + "/66-0-0/" + "normal-0" ;
    videoLive["video_type"] = "normal"; // 表明视频镜头的类型，如normal/wide/zoom/infrared等
    videoLive["video_quality"] = 3;     //{"0":"自适应","1":"流畅","2":"标清","3":"高清","4":"超清"}
    videoLive["status"] = _videoSettings->streamingOut();            //{"0":"未直播","1":"在直播"}
    videoLive["error_status"] = 0;      // 错误码{"length":6}
    liveStatus.append(videoLive);
    jsonObjGcsData["live_status"] = liveStatus;
    jsonObjGcsData["capacity_percent"] = 100;
    jsonObjGcsData["drc_state"] = 0;    // 远程遥控链路状态{"0":"未连接","1":"连接中","2":"已连接"}


    // 发送无人机状态信息
    if (_activeVehicle) {
        QJsonObject jsonDrone;
        jsonDrone["tid"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
        jsonDrone["bid"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
        jsonDrone["timestamp"] = QDateTime::currentMSecsSinceEpoch();
        jsonDrone["gateway"] = SettingsManager::instance()->cloudServerSettings()->gcsSn()->rawValueString();
        QJsonObject jsonObjDroneData;
        if (!_activeVehicle->flying()) {
            // {"0":"待机","1":"起飞准备","2":"起飞准备完毕","3":"手动飞行","4":"自动起飞","5":"航线飞行","6":"全景拍照","7":"智能跟随","8":"ADS-B 躲避","9":"自动返航","10":"自动降落","11":"强制降落","12":"三桨叶降落","13":"升级中","14":"未连接","15":"APAS","16":"虚拟摇杆状态","17":"指令飞行","18":"空中 RTK 收敛模式"}
            jsonObjDroneData["mode_code"] = 0;

        } else {
            if (_activeVehicle->flightMode() == "Ready") {
                jsonObjDroneData["mode_code"] = 2;
            } else if (_activeVehicle->flightMode() == "Takeoff") {
                jsonObjDroneData["mode_code"] = 3;
            } else if (_activeVehicle->flightMode() == "Position") {
                jsonObjDroneData["mode_code"] = 4;
            } else if (_activeVehicle->flightMode() == "Mission") {
                jsonObjDroneData["mode_code"] = 5;
            } else if (_activeVehicle->flightMode() == "Return") {
                jsonObjDroneData["mode_code"] = 9;
            } else if (_activeVehicle->flightMode() == "Land") {
                jsonObjDroneData["mode_code"] = 10;
            } else {
                jsonObjDroneData["mode_code"] = 0;
            }
        }
        QJsonObject jsonPositionState;
        switch (_activeVehicle->gpsFactGroup()->getFact("lock")->enumIndex()) {
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
        jsonPositionState["gps_number"] = _activeVehicle->gpsFactGroup()->getFact("count")->rawValue().toInt();
        GPSRtk * gpsRtk = GPSManager::instance()->gpsRtk();
        jsonPositionState["rtk_number"] = gpsRtk->connected() ? gpsRtk->gpsRtkFactGroup()->getFact("numSatellites")->rawValue().toInt() : 0;
        jsonObjDroneData["position_state"] = jsonPositionState;
        QJsonObject jsonObjBattery;
        VehicleBatteryFactGroup *batteryFactGroup;
        if (_activeVehicle->batteries()->count() > 0) { // 获取第一个电池组
            batteryFactGroup = qobject_cast<VehicleBatteryFactGroup *>(_activeVehicle->batteries()->get(0));
            jsonObjBattery["capacity_percent"] = batteryFactGroup->percentRemaining()->rawValue().toDouble();
            jsonObjBattery["remain_flight_time"] = batteryFactGroup->timeRemaining()->rawValue().toDouble();
        }
        jsonObjDroneData["battery"] = jsonObjBattery;
        jsonObjDroneData["home_distance"] = _activeVehicle->distanceToHome()->rawValue().toDouble();
        jsonObjDroneData["home_latitude"] = _activeVehicle->homePosition().latitude();
        jsonObjDroneData["home_longitude"] = _activeVehicle->homePosition().longitude();
        jsonObjDroneData["attitude_head"] = _activeVehicle->heading()->rawValue().toInt();
        jsonObjDroneData["attitude_roll"] = _activeVehicle->roll()->rawValue().toDouble();
        jsonObjDroneData["attitude_pitch"] = _activeVehicle->pitch()->rawValue().toDouble();
        jsonObjDroneData["elevation"] = _activeVehicle->altitudeRelative()->rawValue().toDouble();
        jsonObjDroneData["height"] = _activeVehicle->altitudeAMSL()->rawValue().toDouble();
        jsonObjDroneData["latitude"] = _activeVehicle->latitude();
        jsonObjDroneData["longitude"] = _activeVehicle->longitude();
        jsonObjDroneData["vertical_speed"] = _activeVehicle->climbRate()->rawValue().toDouble();
        jsonObjDroneData["horizontal_speed"] = _activeVehicle->groundSpeed()->rawValue().toDouble();
        jsonObjDroneData["firmware_version"] = QString::number(_activeVehicle->firmwareMajorVersion()) + "." +
                                               QString::number(_activeVehicle->firmwareMinorVersion()) + "." +
                                               QString::number(_activeVehicle->firmwarePatchVersion()) + ".";
        jsonObjDroneData["wind_direction"] = _activeVehicle->windFactGroup()->getFact("direction")->rawValue().toDouble();
        jsonObjDroneData["wind_speed"] = _activeVehicle->windFactGroup()->getFact("speed")->rawValue().toDouble();
        jsonObjGcsData["latitude"] = _activeVehicle->homePosition().latitude();
        jsonObjGcsData["longitude"] = _activeVehicle->homePosition().longitude();
        jsonObjGcsData["height"] = _activeVehicle->homePosition().altitude();

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
        // qDebug() << "drone publish result: " << result << "topic:" << topic << jsonDocDrone.toJson();
    }

    jsonGcsOsd["data"] = jsonObjGcsData;
    QJsonDocument jsonDocGcs{jsonGcsOsd};
    QString topic = "thing/product/" + SettingsManager::instance()->cloudServerSettings()->gcsSn()->rawValueString() + "/osd";
    _mqttClient->publish(QMqttTopicName(topic), jsonDocGcs.toJson());
    // qDebug() << "dgcs: " << topic << jsonDocGcs.toJson();
}

void DjiBridgeServer::_sendStateLiveCapacityToServer()
{
    qDebug() << "_sendStateLiveCapacityToServer()";
    QJsonObject jsonGcsState;
    jsonGcsState["tid"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
    jsonGcsState["bid"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
    jsonGcsState["timestamp"] = QDateTime::currentMSecsSinceEpoch();
    jsonGcsState["gateway"] = SettingsManager::instance()->cloudServerSettings()->gcsSn()->rawValueString();
    QJsonObject jsonObjGcsData;
    // 直播能力
    QJsonObject liveCapacity;
    liveCapacity["available_video_number"] = 1;
    liveCapacity["coexist_video_number_max"] = 1;
    QJsonArray deviceList;
    QJsonObject device;
    device["sn"] = SettingsManager::instance()->cloudServerSettings()->droneSn()->rawValueString();
    device["available_video_number"] = 1;
    device["coexist_video_number_max"] = 1;

    QJsonArray cameraList;
    QJsonObject camera;
    camera["camera_index"] = "66-0-0";
    camera["available_video_number"] = 1;
    camera["coexist_video_number_max"] = 1;

    QJsonArray videoList;
    QJsonObject video;
    video["video_index"] = "1";
    video["video_type"] = "normal";
    video["switchable_video_types"] = QJsonArray{"zoom", "wide", "thermal", "normal", "ir"};
    videoList.append(video);
    camera["video_list"] = videoList;
    cameraList.append(camera);
    device["camera_list"] = cameraList;
    deviceList.append(device);
    liveCapacity["device_list"] = deviceList;
    jsonObjGcsData["live_capacity"] = liveCapacity;
    jsonGcsState["data"] = jsonObjGcsData;
    QJsonDocument jsonDocGcs{jsonGcsState};
    QString topic = "thing/product/" + SettingsManager::instance()->cloudServerSettings()->gcsSn()->rawValueString() + "/state";
    _mqttClient->publish(QMqttTopicName(topic), jsonDocGcs.toJson(), 1);
}

void DjiBridgeServer::_receiveMqttFromServer(const QByteArray &message, const QMqttTopicName &topic)
{
    QJsonDocument jsonDocMsg = QJsonDocument::fromJson(message);
    QJsonObject jsonObjectMsg = jsonDocMsg.object();
    qDebug() << "_receiveMqttFromServer topic: " << topic << "message:" << jsonObjectMsg;
    QString tid = jsonObjectMsg["tid"].toString();
    QString bid = jsonObjectMsg["bid"].toString();
    if (topic == "sys/product/" + SettingsManager::instance()->cloudServerSettings()->gcsSn()->rawValueString() + "/status_reply") {
        // 收到拓扑更新成功信息
        _timerSendOsd->start();
    } else if (topic == "thing/product/" + SettingsManager::instance()->cloudServerSettings()->gcsSn()->rawValueString() + "/services") { // 服务器下发指令

        QString method = jsonObjectMsg.contains("method") ? jsonObjectMsg["method"].toString() : "";
        if (method == "live_start_push") {
            QJsonObject jsonObjectData = jsonObjectMsg.contains("data") ? jsonObjectMsg["data"].toObject() : QJsonObject();
            if (!jsonObjectData.isEmpty()) {
                int urlType = jsonObjectData.contains("url_type") ? jsonObjectData["url_type"].toInt() : -1;
                QString url = jsonObjectData.contains("url") ? jsonObjectData["url"].toString() : "";
                QString videoId = jsonObjectData.contains("video_id") ? jsonObjectData["video_id"].toString() : "";
                int videoQuality = jsonObjectData.contains("video_quality") ? jsonObjectData["video_quality"].toInt() : -1;
                if (urlType == -1 || url.isEmpty()) {
                    qDebug() << "live_start_push wrong parameter";
                    return;
                }
                _videoSettings->streamingUrl()->setRawValue(url);
                int streamingType = -1;
                if (urlType == 1)
                    streamingType = 1;
                else if (urlType == 4)
                    streamingType = 0;
                _videoSettings->streamingType()->setRawValue(streamingType);
                _videoManager->startStreaming();
                qDebug() << "startstreaming type:" << urlType << "url:" << url;
                sendMqttReply("thing", "services_reply", tid, bid, "live_start_push", 0);
            }
        }
        // qDebug() << "_receiveMqttFromServer: " << message << "topic:" << topic;



    }
}

// ---------------------------------------------------------------------------
// HTTP 连接处理
// ---------------------------------------------------------------------------

void DjiBridgeServer::onNewConnection()
{
    while (_server->hasPendingConnections()) {
        QTcpSocket* socket = _server->nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead,
                this, &DjiBridgeServer::onReadyRead);
        connect(socket, &QTcpSocket::disconnected,
                this, &DjiBridgeServer::onDisconnected);
    }
}

void DjiBridgeServer::onReadyRead()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket) {
        return;
    }

    static QHash<QTcpSocket *, QByteArray> buffers;
    buffers[socket] += socket->readAll();
    QByteArray& raw = buffers[socket];

    int headerEnd = raw.indexOf("\r\n\r\n");
    if (headerEnd < 0) {
        return;
    }

    int contentLength = 0;
    {
        QString header = QString::fromUtf8(raw.left(headerEnd));
        const QStringList lines = header.split("\r\n");
        for (const QString& line : lines) {
            if (line.startsWith("Content-Length:", Qt::CaseInsensitive)) {
                contentLength = line.section(':', 1).trimmed().toInt();
                break;
            }
        }
    }

    int bodyStart = headerEnd + 4;
    int bodySize = raw.size() - bodyStart;
    if (bodySize < contentLength) {
        return;
    }

    QByteArray body = raw.mid(bodyStart, contentLength);
    buffers.remove(socket);

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(body, &parseError);

    QByteArray responseBody;
    if (parseError.error != QJsonParseError::NoError) {
        QJsonObject err = makeResponse(-1,
            QString("Invalid JSON: %1").arg(parseError.errorString()),
            QJsonValue::Null);
        responseBody = QJsonDocument(err).toJson(QJsonDocument::Compact);
    } else {
        QJsonObject req = doc.object();
        QString method = req.value("method").toString();
        QJsonArray args = req.value("args").toArray();

        qInfo().noquote() << QString("[DjiBridge] %1  args=%2")
            .arg(method, -40)
            .arg(QString::fromUtf8(QJsonDocument(args).toJson(QJsonDocument::Compact)));

        emit callReceived(method, args);
        responseBody = handleRequest(method, args);
    }

    QByteArray response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json; charset=utf-8\r\n"
        "Content-Length: " + QByteArray::number(responseBody.size()) + "\r\n"
        "Connection: close\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: POST, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type\r\n"
        "\r\n" + responseBody;

    socket->write(response);
    socket->flush();
    socket->disconnectFromHost();
}

void DjiBridgeServer::onDisconnected()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket *>(sender());
    if (socket) {
        socket->deleteLater();
    }
}

void DjiBridgeServer::activeVehicleChanged(Vehicle *vehicle)
{
    if (_activeVehicle != vehicle) {
        _activeVehicle = vehicle;
        updateDevicesInCloudServer();
    }
}

// ---------------------------------------------------------------------------
// 分发
// ---------------------------------------------------------------------------

QByteArray DjiBridgeServer::handleRequest(const QString& method, const QJsonArray& args)
{
    if (method == "platformGetVersion") {
        return "1.0.0-qgc";
    }

    if (method == "wsConnect" &&
        (args.size() == 0 || args[0].isNull() || args[0].toString().isEmpty())) {
        return QJsonDocument(wsDisconnect(args)).toJson(QJsonDocument::Compact);
    }

    QJsonObject result;
    if (method == "platformLoadComponent")         result = platformLoadComponent(args);
    else if (method == "platformUnloadComponent")   result = platformUnloadComponent(args);
    else if (method == "platformIsComponentLoaded") result = platformIsComponentLoaded(args);
    else if (method == "platformSetWorkspaceId")    result = platformSetWorkspaceId(args);
    else if (method == "platformSetInformation")    result = platformSetInformation(args);
    else if (method == "platformGetRemoteControllerSN") result = platformGetRemoteControllerSN(args);
    else if (method == "platformGetAircraftSN")     result = platformGetAircraftSN(args);
    else if (method == "platformStopSelf")          result = platformStopSelf(args);
    else if (method == "platformSetLogEncryptKey")  result = platformSetLogEncryptKey(args);
    else if (method == "platformClearLogEncryptKey")result = platformClearLogEncryptKey(args);
    else if (method == "platformGetLogPath")        result = platformGetLogPath(args);
    else if (method == "platformVerifyLicense")     result = platformVerifyLicense(args);
    else if (method == "platformIsVerified")        result = platformIsVerified(args);
    else if (method == "platformIsAppInstalled")    result = platformIsAppInstalled(args);
    else if (method == "thingGetConnectState")      result = thingGetConnectState(args);
    else if (method == "thingGetConfigs")           result = thingGetConfigs(args);
    else if (method == "thingConnect")              result = thingConnect(args);
    else if (method == "thingDisconnect")           result = thingDisconnect(args);
    else if (method == "thingSetConnectCallback")   result = thingSetConnectCallback(args);
    else if (method == "apiGetToken")               result = apiGetToken(args);
    else if (method == "apiSetToken")               result = apiSetToken(args);
    else if (method == "apiGetHost")                result = apiGetHost(args);
    else if (method == "liveshareSetVideoPublishType") result = liveshareSetVideoPublishType(args);
    else if (method == "liveshareGetConfig")        result = liveshareGetConfig(args);
    else if (method == "liveshareSetConfig")        result = liveshareSetConfig(args);
    else if (method == "liveshareSetStatusCallback")result = liveshareSetStatusCallback(args);
    else if (method == "liveshareGetStatus")        result = liveshareGetStatus(args);
    else if (method == "liveshareStartLive")        result = liveshareStartLive(args);
    else if (method == "liveshareStopLive")         result = liveshareStopLive(args);
    else if (method == "wsGetConnectState")         result = wsGetConnectState(args);
    else if (method == "wsConnect")                 result = wsConnect(args);
    else if (method == "wsDisconnect")              result = wsDisconnect(args);
    else if (method == "wsSend")                    result = wsSend(args);
    else if (method == "mediaSetAutoUploadPhoto")   result = mediaSetAutoUploadPhoto(args);
    else if (method == "mediaGetAutoUploadPhoto")   result = mediaGetAutoUploadPhoto(args);
    else if (method == "mediaSetUploadPhotoType")   result = mediaSetUploadPhotoType(args);
    else if (method == "mediaGetUploadPhotoType")   result = mediaGetUploadPhotoType(args);
    else if (method == "mediaSetAutoUploadVideo")   result = mediaSetAutoUploadVideo(args);
    else if (method == "mediaGetAutoUploadVideo")   result = mediaGetAutoUploadVideo(args);
    else if (method == "mediaSetDownloadOwner")     result = mediaSetDownloadOwner(args);
    else if (method == "mediaGetDownloadOwner")     result = mediaGetDownloadOwner(args);
    else
        result = makeResponse(-1, "Unknown method: " + method, QJsonValue::Null);

    qDebug() << "handleRequest: " << method << ", " << result;
    return QJsonDocument(result).toJson(QJsonDocument::Compact);
}

QJsonObject DjiBridgeServer::makeResponse(int code, const QString& message, const QJsonValue& data)
{
    QJsonObject obj;
    obj["code"] = code;
    obj["message"] = message;
    obj["data"] = data;
    return obj;
}

// ---------------------------------------------------------------------------
// 平台方法实现
// ---------------------------------------------------------------------------

QJsonObject DjiBridgeServer::platformLoadComponent(const QJsonArray& args)
{
    QString name = args.size() > 0 ? args[0].toString() : "";
    QString param = args.size() > 1 ? args[1].toString() : "{}";
    _loadedComponents[name] = param;
    qInfo() << "[DjiBridge]   -> load component:" << name;

    if (name == "thing") {
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(param.toUtf8(), &err);
        if (err.error == QJsonParseError::NoError && doc.isObject()) {
            QJsonObject obj = doc.object();
            _thingHost = obj.value("host").toString();
            _thingUsername = obj.value("username").toString();
            _thingPassword = obj.value("password").toString();
            _thingCallback = obj.value("connectCallback").toString();

            qInfo() << "[DjiBridge]   -> thing MQTT host:" << _thingHost
                    << "user:" << _thingUsername
                    << "callback:" << _thingCallback;

            startMqttConnection();
        } else {
            qWarning() << "[DjiBridge]   -> thing param parse error:" << err.errorString();
        }
    }

    return makeResponse(0, "success", name);
}

QJsonObject DjiBridgeServer::platformUnloadComponent(const QJsonArray& args)
{
    QString name = args.size() > 0 ? args[0].toString() : "";
    _loadedComponents.remove(name);
    return makeResponse(0, "success", name);
}

QJsonObject DjiBridgeServer::platformIsComponentLoaded(const QJsonArray& args)
{
    QString name = args.size() > 0 ? args[0].toString() : "";
    return makeResponse(0, "success", _loadedComponents.contains(name));
}

QJsonObject DjiBridgeServer::platformSetWorkspaceId(const QJsonArray& args)
{
    _workspaceId = args.size() > 0 ? args[0].toString() : "";
    return makeResponse(0, "success", _workspaceId);
}

QJsonObject DjiBridgeServer::platformSetInformation(const QJsonArray& args)
{
    return makeResponse(0, "success", true);
}

QJsonObject DjiBridgeServer::platformGetRemoteControllerSN(const QJsonArray& args)
{
    // TODO: 对接 QGC 遥控器 SN，当前返回 mock
    return makeResponse(0, "success", "dgcs001");
}

QJsonObject DjiBridgeServer::platformGetAircraftSN(const QJsonArray& args)
{
    // TODO: 对接 MultiVehicleManager::activeVehicle() 的真实飞机 SN
    return makeResponse(0, "success", "drone001");
}

QJsonObject DjiBridgeServer::platformStopSelf(const QJsonArray& args)
{
    return makeResponse(0, "success", "stopped");
}

QJsonObject DjiBridgeServer::platformSetLogEncryptKey(const QJsonArray& args)
{
    return makeResponse(0, "success", "ok");
}

QJsonObject DjiBridgeServer::platformClearLogEncryptKey(const QJsonArray& args)
{
    return makeResponse(0, "success", "ok");
}

QJsonObject DjiBridgeServer::platformGetLogPath(const QJsonArray& args)
{
    return makeResponse(0, "success", QDir::tempPath() + "/dji_logs");
}

QJsonObject DjiBridgeServer::platformVerifyLicense(const QJsonArray& args)
{
    QString appId = args.size() > 0 ? args[0].toString() : "";
    QString appKey = args.size() > 1 ? args[1].toString() : "";
    QString appLicense = args.size() > 2 ? args[2].toString() : "";
    qInfo() << "[DjiBridge]   -> VerifyLicense appId:" << appId
            << "appKey:" << (appKey.length() > 8 ? appKey.left(8) + "..." : appKey)
            << "licenseLen:" << appLicense.length();
    _verified = true;
    return makeResponse(0, "success", true);
}

QJsonObject DjiBridgeServer::platformIsVerified(const QJsonArray& args)
{
    return makeResponse(0, "success", _verified);
}

QJsonObject DjiBridgeServer::platformIsAppInstalled(const QJsonArray& args)
{
    return makeResponse(0, "success", false);
}

// ---------------------------------------------------------------------------
// Thing
// ---------------------------------------------------------------------------

QJsonObject DjiBridgeServer::thingGetConnectState(const QJsonArray& args)
{
    bool connected = (_mqttClient->state() == QMqttClient::Connected);
    _thingConnected = connected;
    return makeResponse(0, "success", connected);
}

QJsonObject DjiBridgeServer::thingGetConfigs(const QJsonArray& args)
{
    QJsonObject config;
    config["host"] = _thingHost;
    config["username"] = _thingUsername;
    config["password"] = _thingPassword;
    config["connectCallback"] = _thingCallback;
    QString configStr = QString::fromUtf8(
        QJsonDocument(config).toJson(QJsonDocument::Compact));
    return makeResponse(0, "success", configStr);
}

QJsonObject DjiBridgeServer::thingConnect(const QJsonArray& args)
{
    if (args.size() >= 2) {
        _thingUsername = args[0].toString();
        _thingPassword = args[1].toString();
    }
    if (args.size() >= 3) {
        _thingCallback = args[2].toString();
    }
    startMqttConnection();
    return makeResponse(0, "success", true);
}

QJsonObject DjiBridgeServer::thingDisconnect(const QJsonArray& args)
{
    if (_mqttClient->state() == QMqttClient::Connected) {
        _mqttClient->disconnectFromHost();
    }
    return makeResponse(0, "success", true);
}

QJsonObject DjiBridgeServer::thingSetConnectCallback(const QJsonArray& args)
{
    _thingCallback = args.size() > 0 ? args[0].toString() : "";
    qInfo() << "[DjiBridge]   -> thing callback set to:" << _thingCallback;
    return makeResponse(0, "success", _thingCallback);
}

// ---------------------------------------------------------------------------
// MQTT 内部实现
// ---------------------------------------------------------------------------

void DjiBridgeServer::parseMqttUrl(const QString& url)
{
    _useWebSocket = false;
    _websocketUrl.clear();

    if (url.startsWith("ws://", Qt::CaseInsensitive) ||
        url.startsWith("wss://", Qt::CaseInsensitive)) {
        _useWebSocket = true;
        _websocketUrl = url;
        QString clean = url;
        if (clean.startsWith("wss://", Qt::CaseInsensitive)) clean = clean.mid(6);
        else if (clean.startsWith("ws://", Qt::CaseInsensitive)) clean = clean.mid(5);
        int slashIdx = clean.indexOf('/');
        if (slashIdx > 0) clean = clean.left(slashIdx);
        int colonIdx = clean.lastIndexOf(':');
        if (colonIdx > 0) {
            _mqttClient->setHostname(clean.left(colonIdx));
            _mqttClient->setPort(clean.mid(colonIdx + 1).toUShort());
        }
        qInfo() << "[DjiBridge]   -> MQTT parsed (WebSocket):" << url;
        return;
    }

    QString clean = url;
    if (clean.startsWith("tcp://", Qt::CaseInsensitive)) {
        clean = clean.mid(6);
    }

    int colonIdx = clean.lastIndexOf(':');
    if (colonIdx > 0) {
        QString host = clean.left(colonIdx);
        quint16 port = clean.mid(colonIdx + 1).toUShort();
        _mqttClient->setHostname(host);
        _mqttClient->setPort(port);
        qInfo() << "[DjiBridge]   -> MQTT parsed (TCP):" << host << "port" << port;
    } else {
        _mqttClient->setHostname(clean);
        _mqttClient->setPort(1883);
        qInfo() << "[DjiBridge]   -> MQTT parsed (TCP):" << clean << "port 1883 (default)";
    }
}

void DjiBridgeServer::startMqttConnection()
{
    if (_thingHost.isEmpty()) {
        qWarning() << "[DjiBridge]   -> MQTT host is empty, skip connect";
        return;
    }

    parseMqttUrl(_thingHost);

    if (_useWebSocket) {
        qWarning() << "[DjiBridge]   -> WebSocket transport not yet implemented,"
                   << "falling back to TCP. URL:" << _websocketUrl;
    }

    _mqttClient->setUsername(_thingUsername);
    _mqttClient->setPassword(_thingPassword);
    _mqttClient->setClientId("QGCDjiBridge-" + QString::number(QCoreApplication::applicationPid()));
    _mqttClient->setKeepAlive(60);
    _mqttClient->setAutoKeepAlive(true);
    _mqttClient->setProtocolVersion(QMqttClient::MQTT_5_0);

    if (_mqttClient->state() == QMqttClient::Connected) {
        _mqttClient->disconnectFromHost();
    }

    qInfo() << "[DjiBridge]   -> MQTT connect details:"
            << "\n      host:" << _mqttClient->hostname()
            << "\n      port:" << _mqttClient->port()
            << "\n      username:" << _mqttClient->username()
            << "\n      clientId:" << _mqttClient->clientId()
            << "\n      protocol: MQTT 5.0"
            << "\n      keepAlive:" << _mqttClient->keepAlive();

    _mqttClient->connectToHost();
    qInfo() << "[DjiBridge]   -> MQTT connecting... (TCP)";

    QTimer::singleShot(10000, this, [this]() {
        if (_mqttClient->state() != QMqttClient::Connected) {
            qWarning() << "[DjiBridge]   -> MQTT connect TIMEOUT after 10s."
                       << "Current state:" << static_cast<int>(_mqttClient->state())
                       << "(0=Disconnected, 1=Connecting, 2=Connected)";
        }
    });
}

void DjiBridgeServer::onMqttStateChanged(QMqttClient::ClientState state)
{
    QString stateStr;
    switch (state) {
    case QMqttClient::Disconnected:
        stateStr = "Disconnected";
        _thingConnected = false;
        break;
    case QMqttClient::Connecting:
        stateStr = "Connecting";
        break;
    case QMqttClient::Connected:
        stateStr = "Connected";
        _thingConnected = true;
        // 发送更新拓扑信息：地面站→无人机
        updateDevicesInCloudServer();
        _sendStateLiveCapacityToServer();
        if (!_thingCallback.isEmpty()) {
            invokeJsCallback(_thingCallback, QJsonValue(true));
        }
        break;
    }
    qInfo() << "[DjiBridge]   -> MQTT state changed:" << stateStr;
}

void DjiBridgeServer::onMqttErrorChanged(QMqttClient::ClientError error)
{
    if (error == QMqttClient::NoError) {
        return;
    }

    QString errStr;
    switch (error) {
    case QMqttClient::NoError:              errStr = "NoError"; break;
    case QMqttClient::InvalidProtocolVersion: errStr = "InvalidProtocolVersion"; break;
    case QMqttClient::IdRejected:           errStr = "IdRejected"; break;
    case QMqttClient::ServerUnavailable:    errStr = "ServerUnavailable"; break;
    case QMqttClient::BadUsernameOrPassword: errStr = "BadUsernameOrPassword"; break;
    case QMqttClient::NotAuthorized:        errStr = "NotAuthorized"; break;
    case QMqttClient::TransportInvalid:     errStr = "TransportInvalid"; break;
    case QMqttClient::ProtocolViolation:    errStr = "ProtocolViolation"; break;
    case QMqttClient::UnknownError:         errStr = "UnknownError"; break;
    default:                                 errStr = "Unknown(" + QString::number(static_cast<int>(error)) + ")"; break;
    }
    qWarning() << "[DjiBridge]   -> MQTT error:" << errStr
               << "transport:" << (_useWebSocket ? "WebSocket" : "TCP")
               << "host:" << _thingHost;
}

void DjiBridgeServer::invokeJsCallback(const QString& callbackName, const QJsonValue& data)
{
    if (callbackName.isEmpty()) {
        return;
    }

    QString dataStr;
    if (data.isBool()) {
        dataStr = data.toBool() ? "true" : "false";
    } else if (data.isDouble()) {
        dataStr = QString::number(data.toDouble());
    } else if (data.isString()) {
        dataStr = "\"" + data.toString() + "\"";
    } else if (data.isObject()) {
        dataStr = QString::fromUtf8(QJsonDocument(data.toObject()).toJson(QJsonDocument::Compact));
    } else if (data.isArray()) {
        dataStr = QString::fromUtf8(QJsonDocument(data.toArray()).toJson(QJsonDocument::Compact));
    } else {
        dataStr = "null";
    }

    QString script = QStringLiteral(
        "(function(){"
        "  try{"
        "    var fn = window.%1;"
        "    if (typeof fn === 'function') { fn(%2); }"
        "    else { console.warn('[DjiBridge] callback %1 is not a function'); }"
        "  }catch(e){ console.error('[DjiBridge] callback error:', e); }"
        "})()").arg(callbackName, dataStr);

    qInfo() << "[DjiBridge]   -> invoke JS callback:" << callbackName
            << "arg:" << dataStr;
    emit jsCallbackRequested(script);
}

// ---------------------------------------------------------------------------
// API
// ---------------------------------------------------------------------------

QJsonObject DjiBridgeServer::apiGetToken(const QJsonArray& args)
{
    return makeResponse(0, "success", _token);
}

QJsonObject DjiBridgeServer::apiSetToken(const QJsonArray& args)
{
    _token = args.size() > 0 ? args[0].toString() : "";
    qInfo() << "[DjiBridge]   -> token set, len:" << _token.length();
    return makeResponse(0, "success", _token);
}

QJsonObject DjiBridgeServer::apiGetHost(const QJsonArray& args)
{
    return makeResponse(0, "success", _host);
}

// ---------------------------------------------------------------------------
// Liveshare
// ---------------------------------------------------------------------------

QJsonObject DjiBridgeServer::liveshareSetVideoPublishType(const QJsonArray& args)
{
    _videoPublishType = args.size() > 0 ? args[0].toString() : "";
    return makeResponse(0, "success", true);
}

QJsonObject DjiBridgeServer::liveshareGetConfig(const QJsonArray& args)
{
    // 网页端会 JSON.parse() 返回值，期望 LiveConfigParam 格式: {"params":0,"type":1}
    // type: 0=Unknown, 1=Agora, 2=RTMP, 3=RTSP, 4=GB28181
    QJsonObject config;
    config["params"] = _liveConfigParams;
    config["type"] = _liveConfigType;
    QString configStr = QString::fromUtf8(QJsonDocument(config).toJson(QJsonDocument::Compact));
    return makeResponse(0, "success", configStr);
}

QJsonObject DjiBridgeServer::liveshareSetConfig(const QJsonArray& args)
{
    if (args.size() >= 1) {
        _liveConfigType = args[0].toInt();
    }
    if (args.size() >= 2) {
        // params 可能是数字或 JSON 字符串，统一存为数字
        if (args[1].isDouble()) {
            _liveConfigParams = args[1].toInt();
        } else if (args[1].isString()) {
            bool ok = false;
            int val = args[1].toString().toInt(&ok);
            if (ok) {
                _liveConfigParams = val;
            }
        }
    }
    return makeResponse(0, "success", "ok");
}

QJsonObject DjiBridgeServer::liveshareSetStatusCallback(const QJsonArray& args)
{
    _liveshareCallback = args.size() > 0 ? args[0].toString() : "";
    return makeResponse(0, "success", _liveshareCallback);
}

QJsonObject DjiBridgeServer::liveshareGetStatus(const QJsonArray& args)
{
    QJsonObject status;
    status["type"] = 0;
    status["status"] = _liveshareActive ? 1 : 0;
    QString statusStr = QString::fromUtf8(
        QJsonDocument(status).toJson(QJsonDocument::Compact));
    return makeResponse(0, "success", statusStr);
}

QJsonObject DjiBridgeServer::liveshareStartLive(const QJsonArray& args)
{
    _liveshareActive = true;
    return makeResponse(0, "success", true);
}

QJsonObject DjiBridgeServer::liveshareStopLive(const QJsonArray& args)
{
    _liveshareActive = false;
    return makeResponse(0, "success", true);
}

// ---------------------------------------------------------------------------
// WebSocket
// ---------------------------------------------------------------------------

QJsonObject DjiBridgeServer::wsGetConnectState(const QJsonArray& args)
{
    return makeResponse(0, "success", _wsConnected);
}

QJsonObject DjiBridgeServer::wsConnect(const QJsonArray& args)
{
    QString host = args.size() > 0 ? args[0].toString() : "";
    QString token = args.size() > 1 ? args[1].toString() : "";
    QString callback = args.size() > 2 ? args[2].toString() : "";
    _wsCallback = callback;
    _wsConnected = true;
    qInfo() << "[DjiBridge]   -> WS connect host:" << host << "callback:" << callback;
    return makeResponse(0, "success", "connected");
}

QJsonObject DjiBridgeServer::wsDisconnect(const QJsonArray& args)
{
    _wsConnected = false;
    return makeResponse(0, "success", "disconnected");
}

QJsonObject DjiBridgeServer::wsSend(const QJsonArray& args)
{
    QString message = args.size() > 0 ? args[0].toString() : "";
    qInfo() << "[DjiBridge]   -> WS send, len:" << message.length();
    return makeResponse(0, "success", "sent");
}

// ---------------------------------------------------------------------------
// Media
// ---------------------------------------------------------------------------

QJsonObject DjiBridgeServer::mediaSetAutoUploadPhoto(const QJsonArray& args)
{
    _autoUploadPhoto = args.size() > 0 ? args[0].toBool() : true;
    return makeResponse(0, "success", "ok");
}

QJsonObject DjiBridgeServer::mediaGetAutoUploadPhoto(const QJsonArray& args)
{
    return makeResponse(0, "success", _autoUploadPhoto);
}

QJsonObject DjiBridgeServer::mediaSetUploadPhotoType(const QJsonArray& args)
{
    _uploadPhotoType = args.size() > 0 ? args[0].toInt() : 1;
    return makeResponse(0, "success", "ok");
}

QJsonObject DjiBridgeServer::mediaGetUploadPhotoType(const QJsonArray& args)
{
    return makeResponse(0, "success", _uploadPhotoType);
}

QJsonObject DjiBridgeServer::mediaSetAutoUploadVideo(const QJsonArray& args)
{
    _autoUploadVideo = args.size() > 0 ? args[0].toBool() : true;
    return makeResponse(0, "success", "ok");
}

QJsonObject DjiBridgeServer::mediaGetAutoUploadVideo(const QJsonArray& args)
{
    return makeResponse(0, "success", _autoUploadVideo);
}

QJsonObject DjiBridgeServer::mediaSetDownloadOwner(const QJsonArray& args)
{
    _downloadOwner = args.size() > 0 ? args[0].toInt() : 0;
    return makeResponse(0, "success", "ok");
}

QJsonObject DjiBridgeServer::mediaGetDownloadOwner(const QJsonArray& args)
{
    return makeResponse(0, "success", _downloadOwner);
}
