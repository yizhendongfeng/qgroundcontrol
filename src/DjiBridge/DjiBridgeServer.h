/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include <QtCore/QObject>
#include <QtNetwork/QTcpServer>
#include <QtNetwork/QTcpSocket>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QHash>
#include <QtCore/QVariant>
#include <QtMqtt/QMqttClient>
#include <QtWebEngineCore/QWebEngineProfile>
#include <QtWebEngineCore/QWebEngineScript>

class Vehicle;
class VideoSettings;
class VideoManager;

/// DjiBridge 本地模拟服务
///
/// 通过 127.0.0.1 上的 HTTP 服务接收网页端注入 JS 发来的同步 XHR 请求，
/// 模拟大疆安卓地面站 window.djiBridge 的全部方法，返回 JsResponse JSON 字符串。
///
/// 同时管理 QWebEngineProfile，在 DocumentCreation 阶段注入 bridge_inject.js，
/// 确保网页任何 JS 执行前 window.djiBridge 已存在。
///
/// 通信协议：
///   请求：POST /api  Body: {"method": "xxx", "args": [...]}
///   响应：HTTP 200  Body: {"code": 0, "message": "", "data": ...}
class DjiBridgeServer : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QWebEngineProfile* profile READ profile CONSTANT)

public:
    explicit DjiBridgeServer(QObject* parent = nullptr);
    ~DjiBridgeServer();

    /// 初始化：启动 HTTP 服务 + 创建 WebEngineProfile + 注入脚本
    Q_INVOKABLE void init();

    bool start();
    quint16 port() const;

    /// WebEngineProfile（含注入脚本），QML 中 WebEngineView 使用
    QWebEngineProfile* profile() const { return _profile; }

    /// 读取注入脚本模板并替换端口号
    QString injectionScript() const;


    void updateDevicesInCloudServer();
    /**
     * @brief sendMqttReply   向mqtt服务器发送应答消息
     * @param topicPrefix sys/thing，sys:任何设备都通用的上云功能，比如设备注册，设备生命周期状态更新等功能，
     *                              thing：支撑实现各设备物模型定义的功能的Topic，主要围绕Property，Service，Event展开
     * @param topicSuffix state_reply, set_reply等
     * @param tid         事务（Transaction）的 UUID：表征一次简单的消息通信,如：增/删/改/查，云台控制等
     * @param bid         业务（Business）的 UUID：有些功能不是一次通信就能完成的，包含持续一段时间内的所有交互。
     *                    业务通常由多个原子事务组成，且持续时间较长;例如点播/下载/回放；解决业务多并发和重复请求的问题，
     *                    便于所有模块的状态机管理。
     * @param method      物模型文件中的service的tidentifier
     * @param result      用于表示ack消息中的事件结果（是否成功）
     */
    void sendMqttReply(const QString& topicPrefix, const QString& topicSuffix, const QString& tid, const QString& bid, const QString& method, const int& result);

    /**
     * @brief _sendOsdToServer发送属性信息，重复间隔0.5s
     */
    void _sendOsdToServer();
    /**
     * @brief _sendStateLiveCapacityToServer 发送直播属性，只有变化时才发送
     */
    void _sendStateLiveCapacityToServer();
    void _receiveMqttFromServer(const QByteArray &message, const QMqttTopicName &topic = QMqttTopicName());
signals:
    /// 每次收到 djiBridge 调用时发出，便于调试
    void callReceived(const QString& method, const QJsonArray& args);

    /// C++ → JS 回调通道：QML 端接收后调用 webEngine.runJavaScript(script)
    void jsCallbackRequested(const QString& script);

public slots:
    void activeVehicleChanged(Vehicle* vehicle);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();
private:
    QTcpServer* _server;
    quint16 _port;
    QWebEngineProfile* _profile = nullptr;
    bool _initialized = false;
    Vehicle* _activeVehicle = nullptr;              ///< Currently active vehicle from a ui perspective
    VideoSettings* _videoSettings = nullptr;
    VideoManager*  _videoManager  = nullptr;
    /**********  Mqtt  **********/
    QTimer* _timerSendOsd = nullptr;               // 发送信息到服务器
    // QJsonObject jsonDevices;                        // 设备信息
    // ---------- 模拟状态 ----------
    bool _verified = false;
    QString _token;
    QString _host;
    QString _workspaceId;
    bool _autoUploadPhoto = true;
    int _uploadPhotoType = 1;
    bool _autoUploadVideo = true;
    int _downloadOwner = 0;
    QString _videoPublishType = "video-by-manual";
    int _liveConfigType = 3;          ///< 直播类型：0=Unknown, 1=Agora, 2=RTMP, 3=RTSP, 4=GB28181
    int _liveConfigParams = 0;        ///< 直播配置参数
    QString _wsCallback;
    QString _liveshareCallback;
    bool _wsConnected = false;
    bool _thingConnected = false;
    bool _liveshareActive = false;
    QHash<QString, QVariant> _loadedComponents;

    // ---------- MQTT (Thing 模块) ----------
    QMqttClient* _mqttClient = nullptr;
    QString _thingHost;
    QString _thingUsername;
    QString _thingPassword;
    QString _thingCallback;
    bool _useWebSocket = false;
    QString _websocketUrl;

    // ---------- 核心分发 ----------
    QByteArray handleRequest(const QString& method, const QJsonArray& args);
    QJsonObject makeResponse(int code, const QString& message, const QJsonValue& data);

    // ---------- 平台方法 ----------
    QJsonObject platformLoadComponent(const QJsonArray& args);
    QJsonObject platformUnloadComponent(const QJsonArray& args);
    QJsonObject platformIsComponentLoaded(const QJsonArray& args);
    QJsonObject platformSetWorkspaceId(const QJsonArray& args);
    QJsonObject platformSetInformation(const QJsonArray& args);
    QJsonObject platformGetRemoteControllerSN(const QJsonArray& args);
    QJsonObject platformGetAircraftSN(const QJsonArray& args);
    QJsonObject platformStopSelf(const QJsonArray& args);
    QJsonObject platformSetLogEncryptKey(const QJsonArray& args);
    QJsonObject platformClearLogEncryptKey(const QJsonArray& args);
    QJsonObject platformGetLogPath(const QJsonArray& args);
    QJsonObject platformVerifyLicense(const QJsonArray& args);
    QJsonObject platformIsVerified(const QJsonArray& args);
    QJsonObject platformIsAppInstalled(const QJsonArray& args);

    // ---------- Thing ----------
    QJsonObject thingGetConnectState(const QJsonArray& args);
    QJsonObject thingGetConfigs(const QJsonArray& args);
    QJsonObject thingConnect(const QJsonArray& args);
    QJsonObject thingDisconnect(const QJsonArray& args);
    QJsonObject thingSetConnectCallback(const QJsonArray& args);

    // ---------- MQTT 内部 ----------
    void parseMqttUrl(const QString& url);
    void startMqttConnection();
    void onMqttStateChanged(QMqttClient::ClientState state);
    void onMqttErrorChanged(QMqttClient::ClientError error);
    void invokeJsCallback(const QString& callbackName, const QJsonValue& data);

    // ---------- API ----------
    QJsonObject apiGetToken(const QJsonArray& args);
    QJsonObject apiSetToken(const QJsonArray& args);
    QJsonObject apiGetHost(const QJsonArray& args);

    // ---------- Liveshare ----------
    QJsonObject liveshareSetVideoPublishType(const QJsonArray& args);
    QJsonObject liveshareGetConfig(const QJsonArray& args);
    QJsonObject liveshareSetConfig(const QJsonArray& args);
    QJsonObject liveshareSetStatusCallback(const QJsonArray& args);
    QJsonObject liveshareGetStatus(const QJsonArray& args);
    QJsonObject liveshareStartLive(const QJsonArray& args);
    QJsonObject liveshareStopLive(const QJsonArray& args);

    // ---------- WebSocket ----------
    QJsonObject wsGetConnectState(const QJsonArray& args);
    QJsonObject wsConnect(const QJsonArray& args);
    QJsonObject wsDisconnect(const QJsonArray& args);
    QJsonObject wsSend(const QJsonArray& args);

    // ---------- Media ----------
    QJsonObject mediaSetAutoUploadPhoto(const QJsonArray& args);
    QJsonObject mediaGetAutoUploadPhoto(const QJsonArray& args);
    QJsonObject mediaSetUploadPhotoType(const QJsonArray& args);
    QJsonObject mediaGetUploadPhotoType(const QJsonArray& args);
    QJsonObject mediaSetAutoUploadVideo(const QJsonArray& args);
    QJsonObject mediaGetAutoUploadVideo(const QJsonArray& args);
    QJsonObject mediaSetDownloadOwner(const QJsonArray& args);
    QJsonObject mediaGetDownloadOwner(const QJsonArray& args);
};
