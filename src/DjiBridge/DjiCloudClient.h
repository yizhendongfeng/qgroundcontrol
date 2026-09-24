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
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonValue>
#include <QtMqtt/QMqttClient>

class Vehicle;
class VideoSettings;
class VideoManager;
class DjiDrcClient;

/// @file
/// @brief DJI 上云 API —— 主 thing-model MQTT 客户端
///
/// 持有主 MQTT 连接（MQTT 5.0），负责：
///   - 连接管理：web SDK 触发（platformLoadComponent("thing")/thingConnect）或原生直连。
///     **断线自动重连**（5s 起退避、封顶 60s，连上复位；disconnectFromCloud 主动断开则不重连），
///     并每 60s 重播一次 update_topo —— 后台判定设备掉线时会永久退订其主题，
///     退订之后只有重播 update_topo 能把它救回来。
///   - 拓扑上报 update_topo、属性上报 osd/state（0.5Hz + 变化）
///   - services 分发（直播 + DRC 握手）、services_reply 应答
///   - events 发布（cloud_control_auth_notify / drc_status_notify 由 DjiDrcClient 回传）
///
/// DRC 的独立第二路连接由 DjiDrcClient 负责，本类只把相关 services 转发过去，
/// 并把 DjiDrcClient 的 events/services_reply 请求发到主连接上。
class DjiCloudClient : public QObject
{
    Q_OBJECT

public:
    explicit DjiCloudClient(QObject* parent = nullptr);
    ~DjiCloudClient();

    void setActiveVehicle(Vehicle* vehicle);
    void setDrcClient(DjiDrcClient* drcClient);
    void setWorkspaceId(const QString& workspaceId) { _workspaceId = workspaceId; }

    /// 连接主 MQTT（web 触发与原生直连共用入口）。callback 为 JS 连接回调名（可空）。
    Q_INVOKABLE void connectToCloud(const QString& host, const QString& username,
                                    const QString& password, const QString& callback = QString());
    Q_INVOKABLE void disconnectFromCloud();
    /// 原生直连：读取 CloudServerSettings 中的 mqttHost/mqttUserName/mqttUserPassword
    Q_INVOKABLE void nativeConnect();
    /// thingConnect：沿用已有 host，仅更换凭据/回调后重连（web SDK 调用路径）
    Q_INVOKABLE void thingConnect(const QString& username, const QString& password, const QString& callback);
    Q_INVOKABLE void setConnectCallback(const QString& callback);

    bool connected() const { return _connected; }
    QJsonObject config() const; // thingGetConfigs 的返回体

signals:
    void connectStateChanged(bool connected);
    /// 后台受理了本机地面站的 update_topo（status_reply，data.result=0）。
    /// 这是设备侧"地面站在后台眼里已上线"的直接证据，由 DjiBridgeServer 转给
    /// DjiWsClient::noteGatewayAck 去点亮抽屉里那台本机地面站。
    void topologyAcked();
    /// C++ → JS 回调通道（转发给 DjiBridgeServer 再进 QML）
    void jsCallbackRequested(const QString& script);

private slots:
    void onMqttStateChanged(QMqttClient::ClientState state);
    void onMqttErrorChanged(QMqttClient::ClientError error);
    void onMqttMessage(const QByteArray& message, const QMqttTopicName& topic);
    void osdTimerTick();
    void stateTimerTick();
    // DjiDrcClient 回传：需在主连接 events topic 发布
    void onDrcEventToPublish(const QString& method, int needReply, const QJsonObject& data);
    // DjiDrcClient 回传：需在主连接 services_reply 应答
    void onDrcServiceReply(const QString& tid, const QString& bid, const QString& method, int result, const QJsonObject& output);

private:
    void parseMqttUrl(const QString& url);
    void startMqttConnection();
    void subscribeTopics();
    /// 排一次断线重连（幂等 + 退避）。why 只进日志，用来区分是 stateChanged 还是 errorChanged 触发的。
    void scheduleReconnect(const QString& why);

    void updateTopo();
    void sendOsd();
    void sendState();
    /// firmware_version 单独一条 state（必须与 sendState 的 live_capacity 分开，见 .cc）
    void sendFirmwareState();
    void sendServicesReply(const QString& tid, const QString& bid, const QString& method, int result, const QJsonValue& output);
    void sendEvents(const QString& method, int needReply, const QJsonObject& data);

    void handleServices(const QJsonObject& msg);
    void handleLiveService(const QString& method, const QString& tid, const QString& bid, const QJsonObject& data);
    void handlePropertySet(const QJsonObject& msg);

    void invokeJsCallback(const QString& callbackName, const QJsonValue& data);

    QMqttClient* _mqttClient    = nullptr;
    QTimer*      _osdTimer      = nullptr;
    QTimer*      _stateTimer    = nullptr;
    QTimer*      _reconnectTimer = nullptr; ///< 单发，延时 = _reconnectDelayMs（退避）
    QTimer*      _topoTimer     = nullptr;  ///< update_topo 周期重播，让后台"忘记"我们后能重新订阅
    DjiDrcClient* _drcClient = nullptr;
    Vehicle*      _activeVehicle = nullptr;

    QString _host;
    QString _username;
    QString _password;
    QString _callback;
    QString _workspaceId;
    bool    _useWebSocket = false;
    QString _websocketUrl;
    bool    _connected = false;
    bool    _userDisconnect = false;   ///< 用户主动断开：置位后不再自动重连
    int     _reconnectDelayMs = 5000;  ///< 当前退避延时，连上后复位
};
