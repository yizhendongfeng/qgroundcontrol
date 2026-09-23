/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include <QtCore/QHash>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonValue>
#include <QtCore/QObject>
#include <QtCore/QSet>
#include <QtCore/QString>
#include <QtCore/QTimer>
#include <QtCore/QVariantMap>
#include <QtNetwork/QAbstractSocket>
#include <QtWebSockets/QWebSocket>

#include "QmlObjectListModel.h"

// 日志用默认类别（qInfo/qWarning），与本目录其他文件一致：
// QGCLogging::msgHandler 会把没有开启 debug 的自定义类别整条丢掉，
// 用 QGC_LOGGING_CATEGORY 建类别反而默认一条日志都看不到。

/// 云端设备（ws 推送的拓扑 + 最近一次 OSD），供抽屉里的设备表逐行绑定
class DjiCloudDeviceInfo : public QObject
{
    Q_OBJECT
    // info: model / callsign / gatewaySn
    // osd:  batteryPercent / height / latitude / longitude / modeCode（缺省即无该字段）
    Q_PROPERTY(QString     sn        READ sn        CONSTANT)
    Q_PROPERTY(bool        online    READ online    NOTIFY onlineChanged)
    Q_PROPERTY(bool        isLocal   READ isLocal   NOTIFY isLocalChanged)
    Q_PROPERTY(QVariantMap info      READ info      NOTIFY infoChanged)
    Q_PROPERTY(QVariantMap osd       READ osd       NOTIFY osdChanged)
    Q_PROPERTY(QString     lastSeen  READ lastSeen  NOTIFY lastSeenChanged)

public:
    DjiCloudDeviceInfo(const QString& sn, QObject* parent = nullptr);

    QString     sn() const { return _sn; }
    bool        online() const { return _online; }
    bool        isLocal() const { return _isLocal; }
    QVariantMap info() const { return _info; }
    QVariantMap osd() const { return _osd; }
    QString     lastSeen() const { return _lastSeen; }

    void setOnline(bool online);
    void setInfo(const QVariantMap& info);
    void setOsd(const QVariantMap& osd);
    void setLastSeen(const QString& lastSeen);
    /// 标记为本机设备（用 CloudServerSettings 的 gcsSn/droneSn 预置时才置位）
    void setLocal(bool local);

signals:
    void onlineChanged();
    void infoChanged();
    void osdChanged();
    void lastSeenChanged();
    void isLocalChanged();

private:
    const QString _sn;
    bool          _isLocal = false;
    bool          _online = false;
    QVariantMap   _info;
    QVariantMap   _osd;
    QString       _lastSeen;
};

/// HMS 健康告警条目（只增不改，字段构造后固定）
class DjiCloudHmsInfo : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString sn    READ sn    CONSTANT)
    /// 首次上报时间：同一条告警被重复推送时不变
    Q_PROPERTY(QString time  READ time  CONSTANT)
    Q_PROPERTY(int     level READ level NOTIFY changed)   ///< 0=NOTICE 1=CAUTION 2=WARN
    Q_PROPERTY(QString code  READ code  CONSTANT)
    Q_PROPERTY(QString text  READ text  NOTIFY changed)

public:
    DjiCloudHmsInfo(const QString& sn,
                    const QString& hmsId,
                    int level,
                    const QString& code,
                    const QString& text,
                    QObject* parent = nullptr);

    QString sn() const { return _sn; }
    QString hmsId() const { return _hmsId; }
    QString time() const { return _time; }
    int     level() const { return _level; }
    QString code() const { return _code; }
    QString text() const { return _text; }

    /// 同一条告警（sn + hmsId）再次推送时刷新内容，不新增行
    void update(int level, const QString& text);

signals:
    void changed();

private:
    const QString _sn;
    const QString _hmsId;
    const QString _time;
    int           _level;
    const QString _code;
    QString       _text;
};

/// 任务/上传进度条目；percent 与 statusText 会随推送更新
class DjiCloudProgressInfo : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString kind       READ kind       CONSTANT)   ///< "task" / "upload"
    Q_PROPERTY(QString title      READ title      CONSTANT)
    Q_PROPERTY(QString time       READ time       CONSTANT)
    Q_PROPERTY(int     percent    READ percent    NOTIFY percentChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)

public:
    DjiCloudProgressInfo(const QString& kind,
                         const QString& title,
                         QObject* parent = nullptr);

    QString kind() const { return _kind; }
    QString title() const { return _title; }
    QString time() const { return _time; }
    int     percent() const { return _percent; }
    QString statusText() const { return _statusText; }

    void setPercent(int percent);
    void setStatusText(const QString& statusText);

signals:
    void percentChanged();
    void statusTextChanged();

private:
    const QString _kind;
    const QString _title;
    const QString _time;
    int           _percent = 0;
    QString       _statusText;
};

/// 原始消息日志条目（只增不改）
class DjiCloudMessageInfo : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString bizCode READ bizCode CONSTANT)
    Q_PROPERTY(QString time    READ time    CONSTANT)
    Q_PROPERTY(QString summary READ summary CONSTANT)

public:
    DjiCloudMessageInfo(const QString& bizCode, const QString& summary, QObject* parent = nullptr);

    QString bizCode() const { return _bizCode; }
    QString time() const { return _time; }
    QString summary() const { return _summary; }

private:
    const QString _bizCode;
    const QString _time;
    const QString _summary;
};

/// DJI 上云 API —— WebSocket（ws 组件）客户端
///
/// 对应大疆 Pilot 原生 SDK 的 ws 组件：加载组件时由原生侧建立到云平台的
/// WebSocket 长连接（`/api/v1/ws?x-auth-token=<access_token>`），云平台通过它
/// 主动推送设备上下线、HMS 告警、任务进度等实时消息。
///
/// 协议要点（与后端 Cloud-API-Demo 一致）：
///   - 裸 WebSocket + 裸 JSON，无需 STOMP 子协议
///   - 信封 {biz_code, version, timestamp, data}
///   - 鉴权走 URL 查询参数 x-auth-token，值是 web 端登录的 access_token
///
/// 收到的消息按 biz_code 分流到四个模型：设备 / 告警 / 进度 / 消息日志。
/// 其中 device_osd、gateway_osd 是高频推送，先缓存再按定时器合流刷新，
/// 且不写入消息日志，避免 UI 抖动与日志被刷爆。
class DjiWsClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QmlObjectListModel* devices  READ devices  CONSTANT)
    Q_PROPERTY(QmlObjectListModel* hms      READ hms      CONSTANT)
    Q_PROPERTY(QmlObjectListModel* progress READ progress CONSTANT)
    Q_PROPERTY(QmlObjectListModel* messages READ messages CONSTANT)
    Q_PROPERTY(bool    connected  READ connected  NOTIFY connectedChanged)
    Q_PROPERTY(QString url        READ url        NOTIFY urlChanged)
    Q_PROPERTY(int     deviceCount READ deviceCount NOTIFY countsChanged)
    Q_PROPERTY(int     onlineCount READ onlineCount NOTIFY countsChanged)
    Q_PROPERTY(int     hmsCount    READ hmsCount    NOTIFY countsChanged)

public:
    explicit DjiWsClient(QObject* parent = nullptr);
    ~DjiWsClient() override;

    /// web SDK 加载 ws 组件 / 网页调用 wsConnect 时进入；token 为空则不附带查询参数
    void connectToWebSocket(const QString& url, const QString& token, const QString& callback = QString());

    /// 原生直连：用 CloudServerSettings 的 websocketUrl + serverToken
    Q_INVOKABLE void connectNative();

    /// 主动断开，且不再自动重连（网页卸载组件 / 用户点断开）
    Q_INVOKABLE void disconnectNow();

    Q_INVOKABLE void clearHms();

    void setConnectCallback(const QString& callback);
    void sendText(const QString& message);

    bool connected() const;
    QString url() const { return _url; }
    int deviceCount() const;
    int onlineCount() const;
    int hmsCount() const;

    QmlObjectListModel* devices() const { return _devices; }
    QmlObjectListModel* hms() const { return _hms; }
    QmlObjectListModel* progress() const { return _progress; }
    QmlObjectListModel* messages() const { return _messages; }

    /// 用 CloudServerSettings 的 gcsSn / droneSn 预置"本机"条目，避免抽屉打开是空的
    void seedLocalDevices();

    /// 后台受理了本机地面站的拓扑上报（收到 sys/product/<gcsSn>/status_reply，result=0）。
    /// 这是"地面站在后台眼里已上线"的一条直接证据 —— 见 .cc 里的长注释。
    void noteGatewayAck();

signals:
    void connectedChanged();
    void urlChanged();
    void countsChanged();
    /// 地图元素类消息（map_element_create/update/delete、map_group_refresh）原样转发给
    /// DjiCloudMapClient。这几条也需要重新拉全量，解析逻辑与模型都在那边，这里只做桥接。
    void mapElementMessage(const QString& bizCode, const QJsonObject& data);
    /// 飞行区域变动（flight_areas_update）。只读图层，收到就整表重拉，解析在那边。
    void flightAreaMessage(const QString& bizCode, const QJsonObject& data);
    /// C++ → JS：由 DjiBridgeServer 组装脚本并注入网页
    void jsCallbackRequested(const QString& callback, const QJsonValue& value);

private slots:
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError error);
    void onTextMessageReceived(const QString& message);
    void onReconnectTimeout();
    void onOsdFlushTimeout();
    void onOnlineSweepTimeout();

private:
    // 连接
    void _openSocket();
    void _scheduleReconnect();
    void _notifyCallback(bool connected);

    // 消息分发
    void _handleEnvelope(const QJsonObject& envelope);
    void _applyDeviceOnline(const QJsonObject& data);
    void _applyDeviceOffline(const QJsonObject& data);
    void _applyDeviceTopo(const QJsonObject& data);
    void _applyHms(const QJsonObject& data);
    void _queueOsd(const QJsonObject& data);
    void _applyProgress(const QString& bizCode, const QJsonObject& data);
    void _appendMessage(const QString& bizCode, const QJsonObject& data);

    DjiCloudDeviceInfo* _deviceFor(const QString& sn, bool createIfMissing);
    /// 记一次"后台还认这台设备"的证据：置在线 + 刷新时间戳（宽限窗口见 BackendEvidenceTimeoutMs）
    void _noteBackendEvidence(const QString& sn);
    void _refreshCounts();
    static QString _summarize(const QString& bizCode, const QJsonObject& data);

    static constexpr int ReconnectInitialDelayMs = 5000;    ///< 与网页 reconnecting-websocket 一致
    static constexpr int ReconnectMaxDelayMs     = 20000;
    static constexpr int ReconnectMaxAttempts    = 5;
    static constexpr int OsdFlushIntervalMs      = 250;     ///< OSD 合流刷新间隔
    /// 「后台还认这台设备」的宽限窗口。证据有三条，取最慢那条做上限：
    /// osd 回推（我们 2 秒发一条，后台每条都回推）、device_online 推送、
    /// status_reply（后台对我们每 60 秒重播的 update_topo 回一条）。
    /// 150 秒 = 2.5 个 topo 周期，丢一两条、重连退避 60 秒都不会误判掉线。
    static constexpr int BackendEvidenceTimeoutMs = 150000;
    static constexpr int OnlineSweepIntervalMs    = 5000;   ///< 掉线扫描周期
    static constexpr int MaxMessages             = 200;
    static constexpr int MaxHms                  = 200;
    static constexpr int MaxProgress             = 50;

    QWebSocket* _socket = nullptr;
    QTimer*     _reconnectTimer = nullptr;
    QTimer*     _osdFlushTimer = nullptr;
    QTimer*     _onlineSweepTimer = nullptr;

    QmlObjectListModel* _devices = nullptr;
    QmlObjectListModel* _hms = nullptr;
    QmlObjectListModel* _progress = nullptr;
    QmlObjectListModel* _messages = nullptr;

    QString _url;
    QString _token;
    QString _callback;
    int     _reconnectAttempts = 0;
    bool    _manualDisconnect = false;  ///< 主动断开：不再重连
    bool    _intentionalClose = false;  ///< 换地址/主动断开导致的 close，不触发重连
    bool    _shuttingDown = false;

    QHash<QString, QJsonObject> _pendingOsd;   ///< sn -> host（等待合流刷新）
    QSet<QString>               _dirtyOsdSn;
    QHash<QString, qint64>      _lastEvidenceMs;  ///< sn -> 最近一次"后台还认它"的时刻
};
