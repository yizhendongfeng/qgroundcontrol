/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

/// @file
/// @brief DjiBridge 本地模拟服务实现（纯 JS 桥 + WebEngine 注入）
///
/// 通过本地 HTTP 服务接收网页同步 XHR 请求，模拟 window.djiBridge 全部方法。
/// 管理 QWebEngineProfile 注入脚本。上云 MQTT 逻辑已拆至 DjiCloudClient / DjiDrcClient。

#include "DjiBridgeServer.h"
#include "DjiCloudClient.h"
#include "DjiCloudMapClient.h"
#include "DjiDrcClient.h"
#include "DjiWsClient.h"
#include "DjiWaylineManager.h"

#include <QtCore/QFile>
#include <QtCore/QTextStream>
#include <QtCore/QDir>
#include <QtCore/QDebug>
#include <QtWebEngineCore/QWebEngineScriptCollection>

#include "SettingsManager.h"
#include "CloudServerSettings.h"
#include "VideoSettings.h"
#include "VideoManager.h"

DjiBridgeServer::DjiBridgeServer(QObject* parent) :
    QObject(parent),
    _server(new QTcpServer(this)),
    _port(0)
{
    connect(_server, &QTcpServer::newConnection,
            this, &DjiBridgeServer::onNewConnection);
}

DjiBridgeServer::~DjiBridgeServer()
{
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

    // 创建上云客户端：主连接 + DRC 独立连接
    _cloudClient = new DjiCloudClient(this);
    _drcClient   = new DjiDrcClient(this);
    _cloudClient->setDrcClient(_drcClient);
    connect(_cloudClient, &DjiCloudClient::jsCallbackRequested,
            this, &DjiBridgeServer::onCloudJsCallback);

    // ws 客户端必须在这里就创建：对 QML 暴露的 Q_PROPERTY 是 CONSTANT，只求值一次，
    // 晚于 QML 引擎创建就会让 cloudDevices 永久为 null。
    _wsClient = new DjiWsClient(this);
    _wsClient->seedLocalDevices();  // 先用本机 SN 占位，抽屉打开不是空表
    connect(_wsClient, &DjiWsClient::jsCallbackRequested,
            this, &DjiBridgeServer::onCloudWsJsCallback);
    connect(_wsClient, &DjiWsClient::connectedChanged,
            this, &DjiBridgeServer::cloudWsStateChanged);
    connect(_wsClient, &DjiWsClient::urlChanged,
            this, &DjiBridgeServer::cloudWsStateChanged);
    connect(_wsClient, &DjiWsClient::countsChanged,
            this, &DjiBridgeServer::cloudWsCountsChanged);
    // 后台受理了我们的拓扑上报（status_reply）—— 设备侧能证明"地面站在后台在线"的
    // 主连接证据。ws 那条链路上后台几乎不发本机地面站的 device_online（见 DjiWsClient.cc），
    // 所以这条必须接上，否则抽屉里本机地面站永远是灰的。
    connect(_cloudClient, &DjiCloudClient::topologyAcked,
            _wsClient, &DjiWsClient::noteGatewayAck);

    // 地图元素：ws 推送 + REST 都在 DjiCloudMapClient 里，这里只接线与转发 Q_PROPERTY
    _mapClient = new DjiCloudMapClient(this);
    connect(_wsClient, &DjiWsClient::mapElementMessage,
            _mapClient, &DjiCloudMapClient::onWsMapElement);
    // 连上就要一次全量：ws 只推增量，首连时平台已有的元素只能靠 HTTP 拉
    connect(_wsClient, &DjiWsClient::connectedChanged, this, [this]() {
        if (_wsClient->connected()) {
            _mapClient->refresh();
            // 飞行区域也先拉一次：图层默认关着，但拉过一次之后点按钮就能立刻出图，
            // 而且按钮上的提示能直接显示"平台上有几个"。后端没实现这个接口时
            // 只会在日志和状态行里留一条 404，不影响元素那条链路。
            _mapClient->refreshFlightAreas();
        } else {
            _mapClient->clearAll();   // 断开上云：元素与飞行区域都属于平台，本地不留
        }
    });
    connect(_mapClient, &DjiCloudMapClient::countChanged,
            this, &DjiBridgeServer::cloudMapStateChanged);
    connect(_mapClient, &DjiCloudMapClient::editingChanged,
            this, &DjiBridgeServer::cloudMapStateChanged);
    connect(_mapClient, &DjiCloudMapClient::statusChanged,
            this, &DjiBridgeServer::cloudMapStateChanged);

    // 飞行区域：同一套 HTTP/鉴权，独立模型与开关。ws 只推"某一条变了"，
    // 客户端收到后整表重拉，所以这里只接线，解析全在 DjiCloudMapClient 里。
    connect(_wsClient, &DjiWsClient::flightAreaMessage,
            _mapClient, &DjiCloudMapClient::onWsFlightArea);
    connect(_mapClient, &DjiCloudMapClient::flightAreasChanged,
            this, &DjiBridgeServer::cloudFlightAreaChanged);

    // 航线库：列表/收藏/上传/下载 + 本地 .plan 双向转换。
    // 同 _wsClient，必须建在 QML 引擎之前 —— djiWayline 是 CONSTANT 属性，只求值一次。
    _waylineManager = new DjiWaylineManager(this);

    CloudServerSettings* cloudSettings = SettingsManager::instance()->cloudServerSettings();
    qInfo() << "[DjiBridge] nativeCloudConnect:" << cloudSettings->nativeCloudConnect()->rawValue()
            << "websocketUrl:" << cloudSettings->websocketUrl()->rawValueString()
            << "token set:" << !cloudSettings->serverToken()->rawValueString().isEmpty();

    // 启动**不**连云 —— 即使 nativeCloudConnect 开着。
    // 设置里的 serverToken 是上一次会话留下的，直接拿它 nativeConnect 也能授权通过，
    // 于是用户本次没登录，右上角云状态就显示"地面站已连接"、地图元素也拉下来画出来了。
    // 连云时机挪到登录成功（apiSetToken）之后，见那里的说明。
    if (cloudSettings->nativeCloudConnect()->rawValue().toBool()) {
        qInfo() << "[DjiBridge] 原生直连已开启，但本次会话尚未登录，等登录成功后再连云";
    }

    // 1. 启动本地 HTTP 服务
    if (!start()) {
        qWarning() << "[DjiBridge] Failed to start HTTP server";
        return;
    }

    // 2. 创建 WebEngineProfile（直接使用 defaultProfile，WebEngineView 默认就用它）
    _profile = QWebEngineProfile::defaultProfile();

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

void DjiBridgeServer::nativeConnectCloud()
{
    if (_cloudClient) {
        _cloudClient->nativeConnect();
    }
}

bool DjiBridgeServer::cloudConnected() const
{
    return _cloudClient ? _cloudClient->connected() : false;
}

void DjiBridgeServer::_setCloudLoggedIn(bool loggedIn)
{
    if (_cloudLoggedIn == loggedIn) {
        return;
    }
    _cloudLoggedIn = loggedIn;
    qInfo() << "[DjiBridge] 登录态：" << (loggedIn ? "已登录" : "未登录（云端航线库会隐藏列表）");
    emit cloudLoggedInChanged();
}

// ---------------------------------------------------------------------------
// 上云 WebSocket（ws 组件）状态转发
// ---------------------------------------------------------------------------

QmlObjectListModel* DjiBridgeServer::cloudDevices() const
{
    return _wsClient ? _wsClient->devices() : nullptr;
}

QmlObjectListModel* DjiBridgeServer::cloudHms() const
{
    return _wsClient ? _wsClient->hms() : nullptr;
}

QmlObjectListModel* DjiBridgeServer::cloudProgress() const
{
    return _wsClient ? _wsClient->progress() : nullptr;
}

QmlObjectListModel* DjiBridgeServer::cloudMessages() const
{
    return _wsClient ? _wsClient->messages() : nullptr;
}

bool DjiBridgeServer::cloudWsConnected() const
{
    return _wsClient ? _wsClient->connected() : false;
}

QString DjiBridgeServer::cloudWsUrl() const
{
    return _wsClient ? _wsClient->url() : QString();
}

int DjiBridgeServer::cloudDeviceCount() const
{
    return _wsClient ? _wsClient->deviceCount() : 0;
}

int DjiBridgeServer::cloudOnlineCount() const
{
    return _wsClient ? _wsClient->onlineCount() : 0;
}

int DjiBridgeServer::cloudHmsCount() const
{
    return _wsClient ? _wsClient->hmsCount() : 0;
}

void DjiBridgeServer::wsConnectNative()
{
    if (_wsClient) {
        _wsClient->connectNative();
    }
}

void DjiBridgeServer::wsDisconnectNow()
{
    if (_wsClient) {
        _wsClient->disconnectNow();
    }
}

void DjiBridgeServer::clearCloudHms()
{
    if (_wsClient) {
        _wsClient->clearHms();
    }
}

// ---------------------------------------------------------------------------
// 云平台地图元素转发
// ---------------------------------------------------------------------------

QmlObjectListModel* DjiBridgeServer::cloudMapLines() const
{
    return _mapClient ? _mapClient->lines() : nullptr;
}

QmlObjectListModel* DjiBridgeServer::cloudMapAreas() const
{
    return _mapClient ? _mapClient->areas() : nullptr;
}

QmlObjectListModel* DjiBridgeServer::cloudMapPoints() const
{
    return _mapClient ? _mapClient->points() : nullptr;
}

QObject* DjiBridgeServer::cloudMapEditing() const
{
    return _mapClient ? _mapClient->editing() : nullptr;
}

int DjiBridgeServer::cloudMapCount() const
{
    return _mapClient ? _mapClient->count() : 0;
}

QString DjiBridgeServer::cloudMapStatus() const
{
    return _mapClient ? _mapClient->status() : QString();
}

void DjiBridgeServer::cloudMapRefresh()
{
    if (_mapClient) {
        _mapClient->refresh();
    }
}

void DjiBridgeServer::cloudMapBeginCreate(int type)
{
    if (_mapClient) {
        _mapClient->beginCreate(type);
    }
}

void DjiBridgeServer::cloudMapPlacePoint(double latitude, double longitude)
{
    if (_mapClient) {
        _mapClient->placePoint(latitude, longitude);
    }
}

void DjiBridgeServer::cloudMapBeginEdit(const QString& id)
{
    if (_mapClient) {
        _mapClient->beginEdit(id);
    }
}

void DjiBridgeServer::cloudMapCancelEdit()
{
    if (_mapClient) {
        _mapClient->cancelEdit();
    }
}

void DjiBridgeServer::cloudMapSaveEdit(const QString& name, const QString& color)
{
    if (_mapClient) {
        _mapClient->saveEdit(name, color);
    }
}

void DjiBridgeServer::cloudMapRemoveElement(const QString& id)
{
    if (_mapClient) {
        _mapClient->removeElement(id);
    }
}

// ---------------------------------------------------------------------------
// 云平台飞行区域转发（任务区域 / GEO 区域，只读）
// ---------------------------------------------------------------------------

QmlObjectListModel* DjiBridgeServer::cloudFlightAreas() const
{
    return _mapClient ? _mapClient->flightAreas() : nullptr;
}

int DjiBridgeServer::cloudTaskAreaCount() const
{
    return _mapClient ? _mapClient->taskAreaCount() : 0;
}

int DjiBridgeServer::cloudGeoZoneCount() const
{
    return _mapClient ? _mapClient->geoZoneCount() : 0;
}

bool DjiBridgeServer::cloudShowTaskAreas() const
{
    return _mapClient ? _mapClient->showTaskAreas() : false;
}

bool DjiBridgeServer::cloudShowGeoZones() const
{
    return _mapClient ? _mapClient->showGeoZones() : false;
}

QString DjiBridgeServer::cloudFlightAreaStatus() const
{
    return _mapClient ? _mapClient->flightAreaStatus() : QString();
}

void DjiBridgeServer::cloudToggleTaskAreas()
{
    if (_mapClient) {
        _mapClient->toggleTaskAreas();
    }
}

void DjiBridgeServer::cloudToggleGeoZones()
{
    if (_mapClient) {
        _mapClient->toggleGeoZones();
    }
}

void DjiBridgeServer::cloudRefreshFlightAreas()
{
    if (_mapClient) {
        _mapClient->refreshFlightAreas();
    }
}

QString DjiBridgeServer::jsCallbackScript(const QString& fn, const QJsonValue& data)
{
    if (fn.isEmpty()) {
        return QString();
    }

    // 借 QJsonDocument 做序列化：字符串里的引号/反斜杠/换行都会被正确转义。
    // QJsonDocument 顶层只接受数组或对象，这里塞进单元素数组再剥掉方括号。
    QString payload = QStringLiteral("null");
    if (!data.isUndefined() && !data.isNull()) {
        QByteArray json = QJsonDocument(QJsonArray{data}).toJson(QJsonDocument::Compact);
        json = json.mid(1, json.size() - 2);
        payload = QString::fromUtf8(json);
    }

    // 网页可能还没注册回调（或已随页面卸载），包一层判定，避免注入脚本抛异常
    return QStringLiteral(
        "(function(){"
        "  try{"
        "    var fn = window.%1;"
        "    if (typeof fn === 'function') { fn(%2); }"
        "    else { console.warn('[DjiBridge] callback %1 is not a function'); }"
        "  }catch(e){ console.error('[DjiBridge] callback error:', e); }"
        "})()").arg(fn, payload);
}

void DjiBridgeServer::onCloudWsJsCallback(const QString& callback, const QJsonValue& value)
{
    const QString script = jsCallbackScript(callback, value);
    if (!script.isEmpty()) {
        emit jsCallbackRequested(script);
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

void DjiBridgeServer::onCloudJsCallback(const QString& script)
{
    emit jsCallbackRequested(script);
}

void DjiBridgeServer::activeVehicleChanged(Vehicle *vehicle)
{
    if (_activeVehicle != vehicle) {
        _activeVehicle = vehicle;
        if (_cloudClient) _cloudClient->setActiveVehicle(vehicle);
        if (_drcClient)   _drcClient->setActiveVehicle(vehicle);
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

    // qDebug() << "handleRequest: " << method << ", " << result;
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
            const QString host = obj.value("host").toString();
            const QString username = obj.value("username").toString();
            const QString password = obj.value("password").toString();
            const QString callback = obj.value("connectCallback").toString();

            qInfo() << "[DjiBridge]   -> thing MQTT host:" << host
                    << "user:" << username
                    << "callback:" << callback;
            if (_cloudClient) {
                _cloudClient->connectToCloud(host, username, password, callback);
            }
        } else {
            qWarning() << "[DjiBridge]   -> thing param parse error:" << err.errorString();
        }
    } else if (name == "ws") {
        // 真实的上云 WebSocket：DJI Pilot 是由原生 SDK 在加载组件时自己建连的，
        // 桥这边必须替它建连，否则网页的 wsGetConnectState 永远是 false。
        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(param.toUtf8(), &err);
        if (err.error == QJsonParseError::NoError && doc.isObject()) {
            const QJsonObject obj = doc.object();
            QString host = obj.value("host").toString();
            const QString token = obj.value("token").toString();
            const QString callback = obj.value("connectCallback").toString();

            if (host.isEmpty()) {
                host = SettingsManager::instance()->cloudServerSettings()->websocketUrl()->rawValueString();
            } else if (!host.startsWith("ws://") && !host.startsWith("wss://")) {
                host = "ws://" + host;
            }
            if (!host.contains("/api/v1/ws")) {
                host = host.endsWith('/') ? host + "api/v1/ws" : host + "/api/v1/ws";
            }

            qInfo() << "[DjiBridge]   -> ws url:" << host
                    << "token:" << (token.isEmpty() ? "<empty>" : "<set>")
                    << "callback:" << callback;
            if (_wsClient) {
                // 这条路径不受 nativeCloudConnect 开关限制：网页要连就必须连
                _wsClient->connectToWebSocket(host, token, callback);
            }
        } else {
            qWarning() << "[DjiBridge]   -> ws param parse error:" << err.errorString();
        }
    }

    return makeResponse(0, "success", name);
}

QJsonObject DjiBridgeServer::platformUnloadComponent(const QJsonArray& args)
{
    QString name = args.size() > 0 ? args[0].toString() : "";
    _loadedComponents.remove(name);

    // 组件卸载时同步断开，避免网页已经卸载而桥这边还在收推送
    if (name == "ws" && _wsClient) {
        _wsClient->disconnectNow();
    }

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
    if (_cloudClient) {
        _cloudClient->setWorkspaceId(_workspaceId);
    }
    SettingsManager::instance()->cloudServerSettings()->workSpaceId()->setRawValue(_workspaceId);
    return makeResponse(0, "success", _workspaceId);
}

QJsonObject DjiBridgeServer::platformSetInformation(const QJsonArray& args)
{
    //示例：["Cloud Api Platform","",""]
    //platformName: 平台名称
    //workspaceName: DJI Pilot 2上云入口显示工作空间名称
    //desc: DJI Pilot 2上云入口显示工作空间描述
    qDebug() << "DjiBridgeServer::platformSetInformation: " << args;
    return makeResponse(0, "success", true);
}

QJsonObject DjiBridgeServer::platformGetRemoteControllerSN(const QJsonArray& args)
{
    return makeResponse(0, "success",
        SettingsManager::instance()->cloudServerSettings()->gcsSn()->rawValueString());
}

QJsonObject DjiBridgeServer::platformGetAircraftSN(const QJsonArray& args)
{
    return makeResponse(0, "success",
        SettingsManager::instance()->cloudServerSettings()->droneSn()->rawValueString());
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
// Thing（转发 DjiCloudClient）
// ---------------------------------------------------------------------------

QJsonObject DjiBridgeServer::thingGetConnectState(const QJsonArray& args)
{
    return makeResponse(0, "success", _cloudClient ? _cloudClient->connected() : false);
}

QJsonObject DjiBridgeServer::thingGetConfigs(const QJsonArray& args)
{
    QJsonObject config = _cloudClient ? _cloudClient->config() : QJsonObject();
    QString configStr = QString::fromUtf8(QJsonDocument(config).toJson(QJsonDocument::Compact));
    return makeResponse(0, "success", configStr);
}

QJsonObject DjiBridgeServer::thingConnect(const QJsonArray& args)
{
    QString username = args.size() > 0 ? args[0].toString() : "";
    QString password = args.size() > 1 ? args[1].toString() : "";
    QString callback = args.size() > 2 ? args[2].toString() : "";
    if (_cloudClient) {
        _cloudClient->thingConnect(username, password, callback);
    }
    return makeResponse(0, "success", true);
}

QJsonObject DjiBridgeServer::thingDisconnect(const QJsonArray& args)
{
    if (_cloudClient) {
        _cloudClient->disconnectFromCloud();
    }
    return makeResponse(0, "success", true);
}

QJsonObject DjiBridgeServer::thingSetConnectCallback(const QJsonArray& args)
{
    QString callback = args.size() > 0 ? args[0].toString() : "";
    if (_cloudClient) {
        _cloudClient->setConnectCallback(callback);
    }
    qInfo() << "[DjiBridge]   -> thing callback set to:" << callback;
    return makeResponse(0, "success", callback);
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
    // 返回了新token说明已经成功登陆
    _token = args.size() > 0 ? args[0].toString() : "";

    // _mqttHostFact->setRawValue(jsonObj["mqtt_addr"].toString());
    // _serverTokenFact->setRawValue(jsonObj["access_token"].toString());
    // _userNameFact->setRawValue(jsonObj["username"].toString());
    // _userPasswordFact->setRawValue(jsonObj["mqtt_password"].toString());
    // _workSpaceIdFact->setRawValue(jsonObj["workspace_id"].toString());
    QString serverIp = QUrl(SettingsManager::instance()->cloudServerSettings()->serverUrl()->rawValueString()).host();
    SettingsManager::instance()->cloudServerSettings()->serverIp()->setRawValue(serverIp);
    SettingsManager::instance()->cloudServerSettings()->serverToken()->setRawValue(_token);
    qInfo() << "[DjiBridge]   -> token set, len:" << _token.length();

    // 登录态：有 token 就是登录了，空 token（网页端退出登录）就是没登录。
    // 云端航线库那页绑它来隐藏列表，所以退出登录也必须回退，不能只置位。
    const bool wasLoggedIn = _cloudLoggedIn;
    _setCloudLoggedIn(!_token.isEmpty());

    // 网页登录成功（拿回了 token）= 本次会话已登录，这才是连云的时机。
    // init() 里已经不连了：启动时那个 token 是上一次会话留下的，用户并没有登录过。
    if (!_token.isEmpty()) {
        if (!wasLoggedIn) {
            qInfo() << "[DjiBridge] 本次会话登录成功，开始连云";
        }

        // 原生直连模式下这里补一次连接 —— 启动时那次已经挪走了。
        // MQTT 主连接和 ws 都要接上：只连 ws 的话，平台侧看不到本机地面站上线
        // （device_online / 拓扑上报走 MQTT），直播、航线库、DRC 也都在 MQTT 那条链上。
        // 判据用"当前是否连着"而不是"是不是首次登录"：退出登录后再登录，这里得能连回去。
        // web SDK 那条路（loadComponent）此时也会各连一次，客户端内部都有"已连则不重连"的判断。
        if (SettingsManager::instance()->cloudServerSettings()->nativeCloudConnect()->rawValue().toBool()) {
            if (_cloudClient && !_cloudClient->connected()) {
                _cloudClient->nativeConnect();
            }
            if (_wsClient && !_wsClient->connected()) {
                _wsClient->connectNative();
            }
        }
    }

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
    QJsonObject config;
    config["params"] = _liveConfigParams;
    config["type"] = _liveConfigType;
    QString configStr = QString::fromUtf8(QJsonDocument(config).toJson(QJsonDocument::Compact));
    return makeResponse(0, "success", configStr);
}

QJsonObject DjiBridgeServer::liveshareSetConfig(const QJsonArray& args)
{
    int type = args.size() > 0 ? args[0].toInt() : -1;
    _liveConfigType = type;

    // params 可能是 JSON 字符串或对象，统一规整为 JSON 字符串交给 CloudServerSettings 解析
    QString params;
    if (args.size() > 1) {
        if (args[1].isString()) {
            params = args[1].toString();
        } else if (args[1].isObject()) {
            params = QString::fromUtf8(QJsonDocument(args[1].toObject()).toJson(QJsonDocument::Compact));
        }
    }

    qInfo() << "[DjiBridge]   -> liveshareSetConfig type:" << type << "params:" << params;

    // 复用 CloudServerSettings::setLiveshareConfig 解析并写入 streamingType/streamingUrl
    SettingsManager::instance()->cloudServerSettings()->setLiveshareConfig(type, params);
    return makeResponse(0, "success", "ok");
}

QJsonObject DjiBridgeServer::liveshareSetStatusCallback(const QJsonArray& args)
{
    _liveshareCallback = args.size() > 0 ? args[0].toString() : "";
    return makeResponse(0, "success", _liveshareCallback);
}

QJsonObject DjiBridgeServer::liveshareGetStatus(const QJsonArray& args)
{
    // 返回真实直播状态（以 VideoSettings::streamingOut 为准，由推流结果异步回写）
    bool streaming = SettingsManager::instance()->videoSettings()->streamingOut();
    QJsonObject status;
    status["type"] = 0;
    status["status"] = streaming ? 1 : 0;
    QString statusStr = QString::fromUtf8(
        QJsonDocument(status).toJson(QJsonDocument::Compact));
    return makeResponse(0, "success", statusStr);
}

QJsonObject DjiBridgeServer::liveshareStartLive(const QJsonArray& args)
{
    qInfo() << "[DjiBridge]   -> liveshareStartLive args:" << args;
    VideoManager::instance()->startStreaming();
    _liveshareActive = true;
    return makeResponse(0, "success", true);
}

QJsonObject DjiBridgeServer::liveshareStopLive(const QJsonArray& args)
{
    qInfo() << "[DjiBridge]   -> liveshareStopLive";
    VideoManager::instance()->stopStreaming();
    _liveshareActive = false;
    return makeResponse(0, "success", true);
}

// ---------------------------------------------------------------------------
// WebSocket
// ---------------------------------------------------------------------------

QJsonObject DjiBridgeServer::wsGetConnectState(const QJsonArray& args)
{
    // 必须反映真实 socket 状态：网页整套模块状态都挂在这个返回值上
    return makeResponse(0, "success", _wsClient && _wsClient->connected());
}

QJsonObject DjiBridgeServer::wsConnect(const QJsonArray& args)
{
    QString host = args.size() > 0 ? args[0].toString() : "";
    QString token = args.size() > 1 ? args[1].toString() : "";
    QString callback = args.size() > 2 ? args[2].toString() : "";
    if (host.isEmpty()) {
        host = SettingsManager::instance()->cloudServerSettings()->websocketUrl()->rawValueString();
    }
    if (token.isEmpty()) {
        token = _token;
    }
    qInfo() << "[DjiBridge]   -> WS connect host:" << host
            << "token:" << (token.isEmpty() ? "<empty>" : "<set>")
            << "callback:" << callback;
    if (_wsClient) {
        _wsClient->connectToWebSocket(host, token, callback);
    }
    return makeResponse(0, "success", _wsClient && _wsClient->connected() ? "connected" : "connecting");
}

QJsonObject DjiBridgeServer::wsDisconnect(const QJsonArray& args)
{
    qInfo() << "[DjiBridge]   -> WS disconnect";
    if (_wsClient) {
        _wsClient->disconnectNow();
    }
    return makeResponse(0, "success", "disconnected");
}

QJsonObject DjiBridgeServer::wsSend(const QJsonArray& args)
{
    QString message = args.size() > 0 ? args[0].toString() : "";
    if (_wsClient) {
        _wsClient->sendText(message);
    }
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
