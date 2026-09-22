/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "DjiCloudClient.h"
#include "DjiDrcClient.h"
#include "DjiCloudProperties.h"
#include "DjiBridgeServer.h"

#include <QtCore/QTimer>
#include <QtCore/QDebug>
#include <QtCore/QDateTime>
#include <QtCore/QJsonDocument>
#include <QtCore/QCoreApplication>
#include <QtCore/QUuid>
#include <QtCore/QUrlQuery>

#include "SettingsManager.h"
#include "CloudServerSettings.h"
#include "VideoSettings.h"
#include "VideoManager.h"
#include "Vehicle.h"

DjiCloudClient::DjiCloudClient(QObject* parent) :
    QObject(parent),
    _mqttClient(new QMqttClient(this)),
    _osdTimer(new QTimer(this)),
    _stateTimer(new QTimer(this))
{
    connect(_mqttClient, &QMqttClient::stateChanged, this, &DjiCloudClient::onMqttStateChanged);
    connect(_mqttClient, &QMqttClient::errorChanged,  this, &DjiCloudClient::onMqttErrorChanged);
    connect(_mqttClient, &QMqttClient::messageReceived, this, &DjiCloudClient::onMqttMessage);

    _osdTimer->setInterval(500);
    connect(_osdTimer, &QTimer::timeout, this, &DjiCloudClient::osdTimerTick);

    // state（live_capacity 等）周期兜底重发：首次上线时后台还未订阅 state 主题，
    // 补发时机见 status_reply 处理；这里定期重发防止任何丢失导致网页相机下拉框为空。
    _stateTimer->setInterval(10000);
    connect(_stateTimer, &QTimer::timeout, this, &DjiCloudClient::stateTimerTick);
}

DjiCloudClient::~DjiCloudClient()
{
    if (_mqttClient->state() == QMqttClient::Connected) {
        _mqttClient->disconnectFromHost();
    }
}

void DjiCloudClient::setActiveVehicle(Vehicle* vehicle)
{
    if (_activeVehicle != vehicle) {
        _activeVehicle = vehicle;
        // 飞机变化后更新拓扑（设备管理）
        if (_connected) {
            updateTopo();
        }
    }
}

void DjiCloudClient::setDrcClient(DjiDrcClient* drcClient)
{
    if (_drcClient == drcClient) {
        return;
    }
    _drcClient = drcClient;
    if (_drcClient) {
        connect(_drcClient, &DjiDrcClient::eventToPublish, this, &DjiCloudClient::onDrcEventToPublish);
        connect(_drcClient, &DjiDrcClient::serviceReply,   this, &DjiCloudClient::onDrcServiceReply);
    }
}

// ---------------------------------------------------------------------------
// 连接管理
// ---------------------------------------------------------------------------

void DjiCloudClient::connectToCloud(const QString& host, const QString& username,
                                    const QString& password, const QString& callback)
{
    _host     = host;
    _username = username;
    _password = password;
    _callback = callback;
    startMqttConnection();
}

void DjiCloudClient::disconnectFromCloud()
{
    if (_mqttClient->state() == QMqttClient::Connected) {
        _mqttClient->disconnectFromHost();
    }
}

void DjiCloudClient::nativeConnect()
{
    CloudServerSettings* s = SettingsManager::instance()->cloudServerSettings();
    _workspaceId = s->workSpaceId()->rawValueString();
    connectToCloud(s->mqttHost()->rawValueString(),
                   s->mqttUserName()->rawValueString(),
                   s->mqttUserPassword()->rawValueString(),
                   QString());
}

void DjiCloudClient::thingConnect(const QString& username, const QString& password, const QString& callback)
{
    _username = username;
    _password = password;
    _callback = callback;
    startMqttConnection(); // 沿用已有 _host
}

void DjiCloudClient::setConnectCallback(const QString& callback)
{
    _callback = callback;
}

QJsonObject DjiCloudClient::config() const
{
    QJsonObject cfg;
    cfg["host"]            = _host;
    cfg["username"]        = _username;
    cfg["password"]        = _password;
    cfg["connectCallback"] = _callback;
    return cfg;
}

void DjiCloudClient::parseMqttUrl(const QString& url)
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
        qInfo() << "[DjiCloud] MQTT parsed (WebSocket):" << url;
        return;
    }

    QString clean = url;
    if (clean.startsWith("tcp://", Qt::CaseInsensitive)) {
        clean = clean.mid(6);
    }

    int colonIdx = clean.lastIndexOf(':');
    if (colonIdx > 0) {
        _mqttClient->setHostname(clean.left(colonIdx));
        _mqttClient->setPort(clean.mid(colonIdx + 1).toUShort());
        qInfo() << "[DjiCloud] MQTT parsed (TCP):" << clean.left(colonIdx) << "port" << clean.mid(colonIdx + 1).toUShort();
    } else {
        _mqttClient->setHostname(clean);
        _mqttClient->setPort(1883);
        qInfo() << "[DjiCloud] MQTT parsed (TCP):" << clean << "port 1883 (default)";
    }
    SettingsManager::instance()->cloudServerSettings()->mqttHost()->setRawValue(url);

}

void DjiCloudClient::startMqttConnection()
{
    if (_host.isEmpty()) {
        qWarning() << "[DjiCloud] MQTT host is empty, skip connect";
        return;
    }

    parseMqttUrl(_host);

    if (_useWebSocket) {
        qWarning() << "[DjiCloud] WebSocket transport not yet implemented, falling back to TCP. URL:" << _websocketUrl;
    }

    _mqttClient->setUsername(_username);
    _mqttClient->setPassword(_password);
    _mqttClient->setClientId("DGCSDjiBridge-" + QString::number(QCoreApplication::applicationPid()));
    _mqttClient->setKeepAlive(60);
    _mqttClient->setAutoKeepAlive(true);
    _mqttClient->setProtocolVersion(QMqttClient::MQTT_5_0);

    if (_mqttClient->state() == QMqttClient::Connected) {
        _mqttClient->disconnectFromHost();
    }

    qInfo() << "[DjiCloud] MQTT connect:" << _mqttClient->hostname() << ":" << _mqttClient->port()
            << "user:" << _mqttClient->username() << "protocol: MQTT 5.0";

    _mqttClient->connectToHost();

    QTimer::singleShot(10000, this, [this]() {
        if (_mqttClient->state() != QMqttClient::Connected) {
            qWarning() << "[DjiCloud] MQTT connect TIMEOUT after 10s. state:"
                       << static_cast<int>(_mqttClient->state());
        }
    });
}

void DjiCloudClient::onMqttStateChanged(QMqttClient::ClientState state)
{
    switch (state) {
    case QMqttClient::Disconnected:
        _connected = false;
        _osdTimer->stop();
        _stateTimer->stop();
        // TODO 添加设置按钮
        // SettingsManager::instance()->cloudServerSettings().connect
        break;
    case QMqttClient::Connected:
        _connected = true;
        subscribeTopics();
        updateTopo();
        // 注意：这里不立即发 state。后台只有在处理完 update_topo 后才会订阅
        // thing/product/{sn}/state 主题，立即发送会被 broker 丢弃；改由收到
        // status_reply 时补发（见 onMqttMessage），并靠 _stateTimer 周期兜底。
        sendState(); //实际情况是服务器一直在运行，已经订阅了相关主题
        _stateTimer->start();
        if (!_callback.isEmpty()) {
            invokeJsCallback(_callback, QJsonValue(true));
        }
        break;
    default:
        break;
    }
    qInfo() << "[DjiCloud] MQTT state:" << static_cast<int>(state);
    emit connectStateChanged(_connected);
}

void DjiCloudClient::onMqttErrorChanged(QMqttClient::ClientError error)
{
    if (error == QMqttClient::NoError) {
        return;
    }
    qWarning() << "[DjiCloud] MQTT error:" << static_cast<int>(error) << "host:" << _host;
}

void DjiCloudClient::subscribeTopics()
{
    const QString gcsSn = SettingsManager::instance()->cloudServerSettings()->gcsSn()->rawValueString();
    _mqttClient->subscribe(QMqttTopicFilter(QStringLiteral("sys/product/")   + gcsSn + QStringLiteral("/status_reply")));
    _mqttClient->subscribe(QMqttTopicFilter(QStringLiteral("thing/product/") + gcsSn + QStringLiteral("/services")));
    _mqttClient->subscribe(QMqttTopicFilter(QStringLiteral("thing/product/") + gcsSn + QStringLiteral("/events_reply")));
    _mqttClient->subscribe(QMqttTopicFilter(QStringLiteral("thing/product/") + gcsSn + QStringLiteral("/property/set")));
}

// ---------------------------------------------------------------------------
// 上行发布
// ---------------------------------------------------------------------------

void DjiCloudClient::updateTopo()
{
    const QString gcsSn = SettingsManager::instance()->cloudServerSettings()->gcsSn()->rawValueString();
    const QString droneSn = SettingsManager::instance()->cloudServerSettings()->droneSn()->rawValueString();

    QJsonObject msg;
    msg["tid"]       = QUuid::createUuid().toString(QUuid::WithoutBraces);
    msg["bid"]       = QUuid::createUuid().toString(QUuid::WithoutBraces);
    msg["timestamp"] = QDateTime::currentMSecsSinceEpoch();
    msg["method"]    = "update_topo";

    QJsonObject data;
    data["domain"]        = 2;
    data["type"]          = 144;
    data["sub_type"]      = 0;
    data["device_secret"] = "device_secret";
    data["nonce"]         = "nonce";
    data["version"]       = 1;
    data["workspace_id"]  = _workspaceId;

    QJsonArray subDevices;
    if (_activeVehicle) {
        QJsonObject sub;
        sub["sn"]            = droneSn;
        sub["domain"]        = 0;
        sub["type"]          = 77;
        sub["sub_type"]      = 0;
        sub["index"]         = "A";
        sub["device_secret"] = "secret";
        subDevices.append(sub);
    }
    // sub_devices 为空表示子设备下线
    data["sub_devices"] = subDevices;

    msg["data"] = data;
    _mqttClient->publish(QMqttTopicName(QStringLiteral("sys/product/") + gcsSn + QStringLiteral("/status")),
                         QJsonDocument(msg).toJson(QJsonDocument::Compact));
    qDebug() << "[DjiCloud] update_topo published";
}

void DjiCloudClient::osdTimerTick()
{
    sendOsd();
}

void DjiCloudClient::stateTimerTick()
{
    sendState();
}

void DjiCloudClient::sendOsd()
{
    if (!_connected) {
        return;
    }

    CloudServerSettings* s = SettingsManager::instance()->cloudServerSettings();
    const QString gcsSn   = s->gcsSn()->rawValueString();
    const QString droneSn = s->droneSn()->rawValueString();

    VideoSettings* videoSettings = SettingsManager::instance()->videoSettings();

    // 地面站（RC）osd
    QJsonObject gcsMsg;
    gcsMsg["tid"]       = QUuid::createUuid().toString(QUuid::WithoutBraces);
    gcsMsg["bid"]       = QUuid::createUuid().toString(QUuid::WithoutBraces);
    gcsMsg["timestamp"] = QDateTime::currentMSecsSinceEpoch();
    gcsMsg["gateway"]   = gcsSn;
    gcsMsg["data"]      = DjiCloudProperties::buildGcsOsd(_activeVehicle, videoSettings);
    _mqttClient->publish(QMqttTopicName(QStringLiteral("thing/product/") + gcsSn + QStringLiteral("/osd")),
                         QJsonDocument(gcsMsg).toJson(QJsonDocument::Compact));

    // 无人机 osd
    if (_activeVehicle) {
        QJsonObject droneMsg;
        droneMsg["tid"]       = QUuid::createUuid().toString(QUuid::WithoutBraces);
        droneMsg["bid"]       = QUuid::createUuid().toString(QUuid::WithoutBraces);
        droneMsg["timestamp"] = QDateTime::currentMSecsSinceEpoch();
        droneMsg["gateway"]   = gcsSn;
        droneMsg["data"]      = DjiCloudProperties::buildDroneOsd(_activeVehicle);
        _mqttClient->publish(QMqttTopicName(QStringLiteral("thing/product/") + droneSn + QStringLiteral("/osd")),
                             QJsonDocument(droneMsg).toJson(QJsonDocument::Compact));
    }
}

void DjiCloudClient::sendState()
{
    if (!_connected) {
        return;
    }

    const QString gcsSn = SettingsManager::instance()->cloudServerSettings()->gcsSn()->rawValueString();

    QJsonObject msg;
    msg["tid"]       = QUuid::createUuid().toString(QUuid::WithoutBraces);
    msg["bid"]       = QUuid::createUuid().toString(QUuid::WithoutBraces);
    msg["timestamp"] = QDateTime::currentMSecsSinceEpoch();
    msg["gateway"]   = gcsSn;
    msg["data"]      = DjiCloudProperties::buildGcsState();

    _mqttClient->publish(QMqttTopicName(QStringLiteral("thing/product/") + gcsSn + QStringLiteral("/state")),
                         QJsonDocument(msg).toJson(QJsonDocument::Compact), 1);
    qDebug() << "sendState:" << msg["data"];
}

void DjiCloudClient::sendServicesReply(const QString& tid, const QString& bid, const QString& method,
                                       int result, const QJsonValue& output)
{
    if (!_connected) {
        return;
    }

    const QString gcsSn = SettingsManager::instance()->cloudServerSettings()->gcsSn()->rawValueString();

    QJsonObject msg;
    msg["tid"]       = tid;
    msg["bid"]       = bid;
    msg["timestamp"] = QDateTime::currentMSecsSinceEpoch();
    msg["gateway"]   = gcsSn;
    msg["method"]    = method;

    QJsonObject data;
    data["result"] = result;
    data["output"] = output;
    msg["data"]    = data;

    _mqttClient->publish(QMqttTopicName(QStringLiteral("thing/product/") + gcsSn + QStringLiteral("/services_reply")),
                         QJsonDocument(msg).toJson(QJsonDocument::Compact), 1);
    qDebug() << "[DjiCloud] services_reply" << method << "result:" << result;
}

void DjiCloudClient::sendEvents(const QString& method, int needReply, const QJsonObject& data)
{
    if (!_connected) {
        return;
    }

    const QString gcsSn = SettingsManager::instance()->cloudServerSettings()->gcsSn()->rawValueString();

    QJsonObject msg;
    msg["tid"]        = QUuid::createUuid().toString(QUuid::WithoutBraces);
    msg["bid"]        = QUuid::createUuid().toString(QUuid::WithoutBraces);
    msg["timestamp"]  = QDateTime::currentMSecsSinceEpoch();
    msg["need_reply"] = needReply;
    msg["gateway"]    = gcsSn;
    msg["method"]     = method;
    msg["data"]       = data;

    _mqttClient->publish(QMqttTopicName(QStringLiteral("thing/product/") + gcsSn + QStringLiteral("/events")),
                         QJsonDocument(msg).toJson(QJsonDocument::Compact), 1);
    qDebug() << "[DjiCloud] events" << method;
}

// ---------------------------------------------------------------------------
// 下行分发
// ---------------------------------------------------------------------------

void DjiCloudClient::onMqttMessage(const QByteArray& message, const QMqttTopicName& topic)
{
    const QJsonObject msg = QJsonDocument::fromJson(message).object();
    qDebug() << "[DjiCloud] recv topic:" << topic.name();

    const QString gcsSn = SettingsManager::instance()->cloudServerSettings()->gcsSn()->rawValueString();

    if (topic.name() == QStringLiteral("sys/product/") + gcsSn + QStringLiteral("/status_reply")) {
        // status_reply 是 update_topo 的应答，data.result=0 表示后台已处理完拓扑并订阅了
        // state/osd 主题。此时开始周期上报 osd，并立即补发一次 state（live_capacity），
        // 否则连接时立即发送的 state 因后台尚未订阅而被丢弃，导致服务器后台网页的
        // "选择相机"下拉框为空。
        // state 无独立应答（协议里没有 state_reply），且只需成功上报一次，因此收到成功
        // 应答后即关闭周期重发定时器，不再反复重发。
        if (msg.value("data").toObject().value("result").toInt() == 0) {
            _osdTimer->start();
            _stateTimer->stop();
            sendState();
        }
    } else if (topic.name() == QStringLiteral("thing/product/") + gcsSn + QStringLiteral("/services")) {
        handleServices(msg);
    } else if (topic.name() == QStringLiteral("thing/product/") + gcsSn + QStringLiteral("/property/set")) {
        handlePropertySet(msg);
    } else if (topic.name() == QStringLiteral("thing/product/") + gcsSn + QStringLiteral("/events_reply")) {
        qDebug() << "[DjiCloud] events_reply:" << msg;
    }
}

void DjiCloudClient::handleServices(const QJsonObject& msg)
{
    const QString method = msg.value("method").toString();
    const QString tid    = msg.value("tid").toString();
    const QString bid    = msg.value("bid").toString();
    const QJsonObject data = msg.value("data").toObject();

    // 直播服务
    if (method.startsWith(QStringLiteral("live_"))) {
        handleLiveService(method, tid, bid, data);
        return;
    }

    // DRC 握手 → 转发 DjiDrcClient
    if (method == QStringLiteral("cloud_control_auth_request")) {
        if (_drcClient) _drcClient->handleAuthRequest(tid, bid, data);
        else sendServicesReply(tid, bid, method, 1, QJsonObject());
        return;
    }
    if (method == QStringLiteral("cloud_control_release")) {
        if (_drcClient) _drcClient->handleAuthRelease(tid, bid, data);
        else sendServicesReply(tid, bid, method, 1, QJsonObject());
        return;
    }
    if (method == QStringLiteral("drc_mode_enter")) {
        if (_drcClient) _drcClient->enterDrcMode(tid, bid, data);
        else sendServicesReply(tid, bid, method, 1, QJsonObject());
        return;
    }

    // 其余 services 一律回"不支持"（相机/云台/红外 payload 服务属 Phase 2）
    qWarning() << "[DjiCloud] unsupported service:" << method;
    sendServicesReply(tid, bid, method, 1, QJsonObject());
}

void DjiCloudClient::handleLiveService(const QString& method, const QString& tid, const QString& bid,
                                       const QJsonObject& data)
{
    VideoManager*  videoManager  = VideoManager::instance();
    VideoSettings* videoSettings = SettingsManager::instance()->videoSettings();

    if (method == "live_start_push") {
        // url_type 官方枚举（Pilot-to-Cloud）：0=Agora, 1=RTMP, 2=RTSP, 3=GB28181；
        // QGC 原生仅支持 RTMP / RTSP，其余回"不支持"
        const int urlType = data.value("url_type").toInt(-1);
        const QString url = data.value("url").toString();
        const int videoQuality = data.value("video_quality").toInt(-1);

        if (url.isEmpty()) {
            qWarning() << "[DjiCloud] live_start_push wrong parameter";
            sendServicesReply(tid, bid, method, 1, QJsonValue(QString()));
            return;
        }

        int streamingType = -1;
        QString streamUrl = url;
        if (urlType == 1) {
            streamingType = 1; // RTMP：url 已是完整 rtmp:// 地址
        } else if (urlType == 2) {
            // RTSP：url 形如 "userName=xx&password=yy&port=zz"，需拼接为 rtsp:// 地址
            streamingType = 0;
            QUrlQuery query(url);
            const QString userName = query.queryItemValue(QStringLiteral("userName"));
            const QString password = query.queryItemValue(QStringLiteral("password"));
            const int port = query.queryItemValue(QStringLiteral("port")).toInt();
            const QString serverIp = QUrl(SettingsManager::instance()->cloudServerSettings()->serverUrl()->rawValueString()).host();
            streamUrl = QStringLiteral("rtsp://") + userName + QStringLiteral(":") + password +
                        QStringLiteral("@") + serverIp + QStringLiteral(":") + QString::number(port) +
                        QStringLiteral("/dgcs");
        } else {
            // Agora(0) / GB28181(3) 暂不支持
            qWarning() << "[DjiCloud] live_start_push unsupported url_type:" << urlType;
            sendServicesReply(tid, bid, method, 1, QJsonValue(QString()));
            return;
        }

        videoSettings->streamingUrl()->setRawValue(streamUrl);
        videoSettings->streamingType()->setRawValue(streamingType);
        if (videoQuality >= 0 && videoQuality <= 4) {
            videoManager->setLiveClarity(videoQuality);
        }
        videoManager->startStreaming();
        qDebug() << "[DjiCloud] live_start_push type:" << urlType << "streamUrl:" << streamUrl << "quality:" << videoQuality;

        // live_start_push 的 services_reply.output 后端按 String 解析（ServicesReplyData<String>）：
        //   - RTSP(2)：返回拼接好的 rtsp:// 地址；
        //   - RTMP(1)：返回空串（后端用请求里的 url，不读 output）。
        const QJsonValue liveOutput = (urlType == 2) ? QJsonValue(streamUrl) : QJsonValue(QString());
        sendServicesReply(tid, bid, method, 0, liveOutput);

    } else if (method == "live_set_quality") {
        const int videoQuality = data.value("video_quality").toInt(-1);
        if (videoQuality >= 0 && videoQuality <= 4) {
            videoManager->setLiveClarity(videoQuality);
            sendServicesReply(tid, bid, method, 0, QJsonObject());
        } else {
            qWarning() << "[DjiCloud] live_set_quality wrong parameter:" << videoQuality;
            sendServicesReply(tid, bid, method, 1, QJsonObject());
        }

    } else if (method == "live_stop_push") {
        videoManager->stopStreaming();
        sendServicesReply(tid, bid, method, 0, QJsonObject());

    } else if (method == "live_lens_change") {
        const QString videoType = data.value("video_type").toString();
        uint8_t mode = 0x02; // 默认可见光
        if (videoType == "thermal" || videoType == "ir") {
            mode = 0x03; // 热成像
        } else if (videoType == "normal") {
            mode = 0x02; // 可见光
        } else {
            qWarning() << "[DjiCloud] live_lens_change unsupported video_type:" << videoType;
            sendServicesReply(tid, bid, method, 1, QJsonObject());
            return;
        }

        if (videoManager->inyyoA102Pro()) {
            videoManager->inyyoA102Pro()->pictureInPictureSwitch(mode);
            sendServicesReply(tid, bid, method, 0, QJsonObject());
        } else {
            qWarning() << "[DjiCloud] live_lens_change: inyyoA102Pro not available";
            sendServicesReply(tid, bid, method, 1, QJsonObject());
        }

    } else {
        sendServicesReply(tid, bid, method, 1, QJsonObject());
    }
}

void DjiCloudClient::handlePropertySet(const QJsonObject& msg)
{
    // RC Pro 的 RC 物模型无 rw 属性，正常不会收到 property/set；统一按失败回 set_reply
    const QString gcsSn = SettingsManager::instance()->cloudServerSettings()->gcsSn()->rawValueString();
    const QString tid = msg.value("tid").toString();
    const QString bid = msg.value("bid").toString();

    QJsonObject data;
    const QJsonObject inData = msg.value("data").toObject();
    for (auto it = inData.constBegin(); it != inData.constEnd(); ++it) {
        QJsonObject r;
        r["result"] = 1; // 不支持写入
        data[it.key()] = r;
    }

    QJsonObject reply;
    reply["tid"]       = tid;
    reply["bid"]       = bid;
    reply["timestamp"] = QDateTime::currentMSecsSinceEpoch();
    reply["gateway"]   = gcsSn;
    reply["data"]      = data;

    _mqttClient->publish(QMqttTopicName(QStringLiteral("thing/product/") + gcsSn + QStringLiteral("/property/set_reply")),
                         QJsonDocument(reply).toJson(QJsonDocument::Compact), 1);
    qDebug() << "[DjiCloud] property/set replied (unsupported)";
}

// ---------------------------------------------------------------------------
// DjiDrcClient 回传
// ---------------------------------------------------------------------------

void DjiCloudClient::onDrcEventToPublish(const QString& method, int needReply, const QJsonObject& data)
{
    sendEvents(method, needReply, data);
}

void DjiCloudClient::onDrcServiceReply(const QString& tid, const QString& bid, const QString& method,
                                       int result, const QJsonObject& output)
{
    sendServicesReply(tid, bid, method, result, output);
}

// ---------------------------------------------------------------------------
// JS 回调
// ---------------------------------------------------------------------------

void DjiCloudClient::invokeJsCallback(const QString& callbackName, const QJsonValue& data)
{
    if (callbackName.isEmpty()) {
        return;
    }

    // 脚本构造统一走 DjiBridgeServer::jsCallbackScript：参数由 QJsonDocument 序列化，
    // 字符串里的引号/反斜杠不会再拼出坏 JS。
    const QString script = DjiBridgeServer::jsCallbackScript(callbackName, data);
    if (script.isEmpty()) {
        return;
    }

    qInfo() << "[DjiCloud] invoke JS callback:" << callbackName;
    emit jsCallbackRequested(script);
}
