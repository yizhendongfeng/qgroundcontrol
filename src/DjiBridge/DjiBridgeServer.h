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
#include <QtWebEngineCore/QWebEngineProfile>
#include <QtWebEngineCore/QWebEngineScript>

#include "QmlObjectListModel.h"
// djiWayline 这个 Q_PROPERTY 的指针类型必须完整定义：moc 生成元对象时对
// "指向不完整类型的指针" 会直接 static_assert 报错（C2338）。其余几个客户端
// 没有这个问题，是因为它们只经 QmlObjectListModel*/QObject* 暴露，不出现在
// Q_PROPERTY 的类型位置上。
#include "DjiWaylineManager.h"
// 同理，djiDrc 这个 Q_PROPERTY 也要求 DjiDrcClient 是完整类型
#include "DjiDrcClient.h"

class Vehicle;
class DjiCloudClient;
class DjiCloudMapClient;
class DjiWsClient;

/// DjiBridge 本地模拟服务（纯 JS 桥 + WebEngine 注入 facade）
///
/// 通过 127.0.0.1 上的 HTTP 服务接收网页端注入 JS 发来的同步 XHR 请求，
/// 模拟大疆安卓地面站 window.djiBridge 的全部方法，返回 JsResponse JSON 字符串。
///
/// 同时管理 QWebEngineProfile，在 DocumentCreation 阶段注入 bridge_inject.js，
/// 确保网页任何 JS 执行前 window.djiBridge 已存在。
///
/// 上云 MQTT 逻辑已拆出：
///   - DjiCloudClient：主 thing-model 连接（属性/设备/直播/services/events）
///   - DjiDrcClient：DRC 独立连接（指令飞行/远程控制）
/// 本类负责持有并转发这两者，自身不再包含任何 MQTT 代码。
///
/// 通信协议：
///   请求：POST /api  Body: {"method": "xxx", "args": [...]}
///   响应：HTTP 200  Body: {"code": 0, "message": "", "data": ...}
class DjiBridgeServer : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QWebEngineProfile* profile READ profile CONSTANT)

    // ---------- 上云 WebSocket（ws 组件）暴露给 QML ----------
    Q_PROPERTY(QmlObjectListModel* cloudDevices  READ cloudDevices  CONSTANT)
    Q_PROPERTY(QmlObjectListModel* cloudHms      READ cloudHms      CONSTANT)
    Q_PROPERTY(QmlObjectListModel* cloudProgress READ cloudProgress CONSTANT)
    Q_PROPERTY(QmlObjectListModel* cloudMessages READ cloudMessages CONSTANT)
    Q_PROPERTY(bool    cloudWsConnected READ cloudWsConnected NOTIFY cloudWsStateChanged)
    Q_PROPERTY(QString cloudWsUrl       READ cloudWsUrl       NOTIFY cloudWsStateChanged)
    Q_PROPERTY(int     cloudDeviceCount READ cloudDeviceCount NOTIFY cloudWsCountsChanged)
    Q_PROPERTY(int     cloudOnlineCount READ cloudOnlineCount NOTIFY cloudWsCountsChanged)
    Q_PROPERTY(int     cloudHmsCount    READ cloudHmsCount    NOTIFY cloudWsCountsChanged)

    // ---------- 云平台地图元素（Pilot 地图标注）暴露给 QML ----------
    Q_PROPERTY(QmlObjectListModel* cloudMapLines   READ cloudMapLines   CONSTANT)
    Q_PROPERTY(QmlObjectListModel* cloudMapAreas   READ cloudMapAreas   CONSTANT)
    /// 端点（Point）：官方控制台上的菱形图钉
    Q_PROPERTY(QmlObjectListModel* cloudMapPoints  READ cloudMapPoints  CONSTANT)
    /// 当前编辑中的元素（CloudMapElement*，QML 侧按 object.xxx 动态访问），null=不在编辑态
    Q_PROPERTY(QObject* cloudMapEditing READ cloudMapEditing NOTIFY cloudMapStateChanged)
    Q_PROPERTY(int     cloudMapCount  READ cloudMapCount  NOTIFY cloudMapStateChanged)
    Q_PROPERTY(QString cloudMapStatus READ cloudMapStatus NOTIFY cloudMapStateChanged)

    // ---------- 云平台飞行区域（任务区域 / GEO 区域，只读图层）暴露给 QML ----------
    Q_PROPERTY(QmlObjectListModel* cloudFlightAreas READ cloudFlightAreas CONSTANT)
    Q_PROPERTY(int  cloudTaskAreaCount  READ cloudTaskAreaCount  NOTIFY cloudFlightAreaChanged)
    Q_PROPERTY(int  cloudGeoZoneCount   READ cloudGeoZoneCount   NOTIFY cloudFlightAreaChanged)
    /// 两个图层各自是否在显示（图标列上那两个按钮切的就是它们）
    Q_PROPERTY(bool cloudShowTaskAreas READ cloudShowTaskAreas NOTIFY cloudFlightAreaChanged)
    Q_PROPERTY(bool cloudShowGeoZones  READ cloudShowGeoZones  NOTIFY cloudFlightAreaChanged)
    Q_PROPERTY(QString cloudFlightAreaStatus READ cloudFlightAreaStatus NOTIFY cloudFlightAreaChanged)

    // ---------- 云平台航线库（列表/收藏/上传/下载 + 本地 .plan 双向转换）暴露给 QML ----------
    Q_PROPERTY(DjiWaylineManager* djiWayline READ djiWayline CONSTANT)

    // ---------- 指令飞行 / 远程控制（DRC）暴露给 QML ----------
    /// 状态、控制权、上行统计都在这里，UI 见 DrcControlPanel.qml
    Q_PROPERTY(DjiDrcClient* djiDrc READ drcClient CONSTANT)

public:
    explicit DjiBridgeServer(QObject* parent = nullptr);
    ~DjiBridgeServer();

    /// 初始化：启动 HTTP 服务 + 创建 WebEngineProfile + 注入脚本 + 创建云客户端
    Q_INVOKABLE void init();

    bool start();
    quint16 port() const;

    /// WebEngineProfile（含注入脚本），QML 中 WebEngineView 使用
    QWebEngineProfile* profile() const { return _profile; }

    /// 读取注入脚本模板并替换端口号
    QString injectionScript() const;

    /// 原生直连入口（QML 可调用，绕开 web SDK）
    Q_INVOKABLE void nativeConnectCloud();
    Q_INVOKABLE bool cloudConnected() const;

    /// 供外部/测试访问底层客户端
    DjiCloudClient*    cloudClient() const { return _cloudClient; }
    DjiDrcClient*      drcClient()   const { return _drcClient; }
    DjiWsClient*       wsClient()    const { return _wsClient; }
    DjiCloudMapClient* mapClient()   const { return _mapClient; }
    DjiWaylineManager* djiWayline()  const { return _waylineManager; }

    // ---------- 上云 WebSocket 状态转发 ----------
    QmlObjectListModel* cloudDevices()  const;
    QmlObjectListModel* cloudHms()      const;
    QmlObjectListModel* cloudProgress() const;
    QmlObjectListModel* cloudMessages() const;
    bool    cloudWsConnected() const;
    QString cloudWsUrl()       const;
    int     cloudDeviceCount() const;
    int     cloudOnlineCount() const;
    int     cloudHmsCount()    const;

    /// 用 CloudServerSettings 的 websocketUrl + serverToken 直接连（QML 手动补齐）
    Q_INVOKABLE void wsConnectNative();
    Q_INVOKABLE void wsDisconnectNow();
    Q_INVOKABLE void clearCloudHms();

    // ---------- 云平台地图元素状态转发 ----------
    QmlObjectListModel* cloudMapLines()   const;
    QmlObjectListModel* cloudMapAreas()   const;
    QmlObjectListModel* cloudMapPoints()  const;
    QObject*            cloudMapEditing() const;
    int                 cloudMapCount()   const;
    QString             cloudMapStatus()  const;

    /// 地图元素编辑会话转发（QML 只认 djiBridgeServer 这个上下文对象）
    Q_INVOKABLE void cloudMapRefresh();
    Q_INVOKABLE void cloudMapBeginCreate(int type);          ///< 0=端点 1=线段 2=区域
    /// 在地图上点了左键：把端点放到这个坐标上（由 CloudElementMapLayer 的点击处理调用）
    Q_INVOKABLE void cloudMapPlacePoint(double latitude, double longitude);
    Q_INVOKABLE void cloudMapBeginEdit(const QString& id);
    Q_INVOKABLE void cloudMapCancelEdit();
    Q_INVOKABLE void cloudMapSaveEdit(const QString& name, const QString& color);
    Q_INVOKABLE void cloudMapRemoveElement(const QString& id);

    // ---------- 云平台飞行区域（只读） ----------
    QmlObjectListModel* cloudFlightAreas() const;
    int     cloudTaskAreaCount() const;
    int     cloudGeoZoneCount() const;
    bool    cloudShowTaskAreas() const;
    bool    cloudShowGeoZones() const;
    QString cloudFlightAreaStatus() const;

    Q_INVOKABLE void cloudToggleTaskAreas();
    Q_INVOKABLE void cloudToggleGeoZones();
    Q_INVOKABLE void cloudRefreshFlightAreas();

    /// 组装注入网页的 JS 调用脚本（C++ → JS 回调共用）
    ///
    /// 统一用 QJsonDocument 序列化参数，避免字符串里出现 `"` 或 `\` 时拼出坏 JS。
    static QString jsCallbackScript(const QString& fn, const QJsonValue& data);

signals:
    /// 每次收到 djiBridge 调用时发出，便于调试
    void callReceived(const QString& method, const QJsonArray& args);

    /// C++ → JS 回调通道：QML 端接收后调用 webEngine.runJavaScript(script)
    void jsCallbackRequested(const QString& script);

    void cloudWsStateChanged();
    void cloudWsCountsChanged();
    void cloudMapStateChanged();
    void cloudFlightAreaChanged();

public slots:
    void activeVehicleChanged(Vehicle* vehicle);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();
    void onCloudJsCallback(const QString& script);
    /// DjiWsClient 的连接状态回调 → 注入网页（wsConnectCallback(true/false)）
    void onCloudWsJsCallback(const QString& callback, const QJsonValue& value);

private:
    QTcpServer* _server;
    quint16 _port;
    QWebEngineProfile* _profile = nullptr;
    bool _initialized = false;
    Vehicle* _activeVehicle = nullptr;

    DjiCloudClient*    _cloudClient = nullptr;
    DjiDrcClient*      _drcClient   = nullptr;
    DjiWsClient*       _wsClient    = nullptr;
    DjiCloudMapClient* _mapClient   = nullptr;
    DjiWaylineManager* _waylineManager = nullptr;

    QHash<QString, QVariant> _loadedComponents;

    // ---------- 平台/授权状态 ----------
    bool _verified = false;
    QString _token;
    QString _host;
    QString _workspaceId;

    // ---------- Media / Liveshare / WebSocket 状态 ----------
    bool _autoUploadPhoto = true;
    int _uploadPhotoType = 1;
    bool _autoUploadVideo = true;
    int _downloadOwner = 0;
    QString _videoPublishType = "video-by-manual";
    int _liveConfigType = 3;          ///< 直播类型：0=Unknown, 1=Agora, 2=RTMP, 3=RTSP, 4=GB28181
    int _liveConfigParams = 0;        ///< 直播配置参数
    QString _liveshareCallback;
    bool _liveshareActive = false;

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

    // ---------- Thing（转发 DjiCloudClient） ----------
    QJsonObject thingGetConnectState(const QJsonArray& args);
    QJsonObject thingGetConfigs(const QJsonArray& args);
    QJsonObject thingConnect(const QJsonArray& args);
    QJsonObject thingDisconnect(const QJsonArray& args);
    QJsonObject thingSetConnectCallback(const QJsonArray& args);

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
