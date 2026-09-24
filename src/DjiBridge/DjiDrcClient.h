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
#include <QtCore/QVariantMap>
#include <QtMqtt/QMqttClient>

// 下面 setCloudStick 的签名用到 DjiDrcControlMapper::Axes —— 嵌套类型没法靠前置声明，
// 只能把完整定义拿进来。
#include "DjiDrcControlMapper.h"

class Vehicle;
class QTimer;

/// @file
/// @brief DJI 上云 API —— 指令飞行（DRC）+ 远程控制
///
/// DRC 走一条与主连接完全独立的 MQTT 连接：`drc_mode_enter`（主连接的 services）
/// 下发独立 broker 凭据后，本类建立第二路连接，订阅
/// `thing/product/{sn}/drc/down`、发布 `.../drc/up`。SN 用 gcsSn（= 云端的
/// gateway_sn，官方网页端就是拿这个 sn 发 DRC 报文的）。
///
/// 报文归属（依据官方 cloud-sdk 的 AbstractControlService 与 DrcDownPublish）：
///   - **drc/down 只有三个 method**：drone_control、drone_emergency_stop、heart_beat
///   - 其余控制指令（抢控制权、进出 DRC、相机、云台…）全走主连接的 services，
///     由 DjiCloudClient 转发进来
///   - drc/up 上行：drone_control 应答、heart_beat、osd_info_push（10Hz）、
///     hsi_info_push（1Hz）、delay_info_push（1Hz），见 .cc 的各 build* 函数
///
/// 本类只管协议与状态机，"指令怎么变成飞机动作"全在 DjiDrcControlMapper 里。
///
/// 三类上行到主连接的消息通过信号交回 DjiCloudClient 发布：
///   - eventToPublish  → events topic（cloud_control_auth_notify / drc_status_notify /
///                       joystick_invalid_notify）
///   - serviceReply    → services_reply topic
class DjiDrcClient : public QObject
{
    Q_OBJECT

    // ---------- 暴露给 QML 的状态（供 DrcControlPanel 显示） ----------
    Q_PROPERTY(bool    drcActive            READ drcActive            NOTIFY drcStatusChanged)
    Q_PROPERTY(int     drcState             READ drcState             NOTIFY drcStatusChanged)
    Q_PROPERTY(bool    cloudFlightAuthority READ cloudFlightAuthority NOTIFY drcStatusChanged)
    Q_PROPERTY(bool    cloudPayloadAuthority READ cloudPayloadAuthority NOTIFY drcStatusChanged)
    Q_PROPERTY(bool    authPending          READ authPending          NOTIFY drcStatusChanged)
    Q_PROPERTY(bool    emergencyStopPending READ emergencyStopPending NOTIFY drcStatusChanged)
    Q_PROPERTY(QString authUserId           READ authUserId           NOTIFY drcStatusChanged)
    Q_PROPERTY(QString authUserCallsign     READ authUserCallsign     NOTIFY drcStatusChanged)
    Q_PROPERTY(QString brokerAddress        READ brokerAddress        NOTIFY drcStatusChanged)
    Q_PROPERTY(int     downSilenceMs        READ downSilenceMs        NOTIFY drcStatusChanged)
    Q_PROPERTY(int     osdFrequency         READ osdFrequency         NOTIFY drcStatusChanged)
    Q_PROPERTY(int     hsiFrequency         READ hsiFrequency         NOTIFY drcStatusChanged)
    Q_PROPERTY(QString lastErrorText        READ lastErrorText        NOTIFY drcStatusChanged)
    /// 当前飞机是否处于"吃手动输入"的模式。**这是云端持权能不能真正控住飞机的唯一判据**：
    /// 为 false 时 drone_control 会被逐条拒成 327002，而界面上别的数字一切正常。
    /// 工具栏图标的红色"模式拒收"态、面板与抽屉的警告行都读它。
    Q_PROPERTY(bool    manualControlReady   READ manualControlReady   NOTIFY drcStatusChanged)
    /// 各类报文的收发计数（面板上的自检数字），键见 .cc 的 bumpStat()
    Q_PROPERTY(QVariantMap pushStats        READ pushStats            NOTIFY drcStatusChanged)
    /// 最近一次组装好的低延时 osd（面板直接显示，等于顺手自检上行内容）
    Q_PROPERTY(QVariantMap drcOsd           READ drcOsd               NOTIFY drcTelemetryChanged)
    /// 云端最近一次下发的杆量。键分两组：
    ///   x/y/h/w/seq/has_data —— 收到的**原始值**。后台推了什么就记什么，
    ///                           即使这一杆被拒也不会显示成零。
    ///   roll/pitch/yaw/thrust —— **映射后**的轴值，显示必须用这组。
    ///                           映射里有取反和 drcInvertX/Y/W，用原始值画会出现
    ///                           "把手指一边、飞机飞另一边"。thrust ∈ [0,1]，0.5 = 悬停。
    Q_PROPERTY(QVariantMap cloudStick       READ cloudStick           NOTIFY cloudStickChanged)

public:
    explicit DjiDrcClient(QObject* parent = nullptr);
    ~DjiDrcClient();

    /// 当前 vehicle 变化时由外部调用（DjiBridgeServer::activeVehicleChanged 转发）
    void setActiveVehicle(Vehicle* vehicle);

    /// DRC 是否处于活跃（第二路连接已建立）
    bool drcActive() const { return _drcActive; }

    // ---------- 主连接 services 的转发入口（由 DjiCloudClient 调用） ----------

    /// 云端请求接管（操作员需在 RC 端确认）→ cloud_control_auth_notify
    void handleAuthRequest(const QString& tid, const QString& bid, const QJsonObject& data);
    /// 云端释放控制权
    void handleAuthRelease(const QString& tid, const QString& bid, const QJsonObject& data);
    /// 进入 DRC：data 里带 mqtt_broker 凭据与 osd/hsi 上行频率
    void handleDrcModeEnter(const QString& tid, const QString& bid, const QJsonObject& data);
    /// 退出 DRC
    void handleDrcModeExit(const QString& tid, const QString& bid, const QJsonObject& data);
    /// 抢夺飞行控制权（云端后续的 drone_control 才被接受）
    void handleFlightAuthorityGrab(const QString& tid, const QString& bid, const QJsonObject& data);
    /// 抢夺负载控制权（云端后续的相机/云台指令才被接受）
    void handlePayloadAuthorityGrab(const QString& tid, const QString& bid, const QJsonObject& data);
    /// 负载控制（相机/云台等）。@return 是否已处理，false 时由调用方回"不支持"
    bool handlePayloadControl(const QString& method, const QString& tid, const QString& bid,
                              const QJsonObject& data);

    // ---------- QML 入口 ----------

    /// 应答 cloud_control_auth_request（操作员点了同意/拒绝）
    Q_INVOKABLE void drcRespondAuth(bool accept);
    /// 本地操作员抢/放飞行控制权
    Q_INVOKABLE void drcGrabFlightAuthority(bool grab);
    /// 本地操作员抢/放负载控制权
    Q_INVOKABLE void drcGrabPayloadAuthority(bool grab);
    /// 本地主动退出 DRC（会同时通知云端与发 joystick_invalid_notify）
    Q_INVOKABLE void drcRequestExit();
    /// 紧急停桨。**调用前必须已二次确认**（QML 侧的确认弹窗）
    Q_INVOKABLE void drcEmergencyStop();
    /// 应答云端下发的紧急停桨（操作员在确认弹窗上点同意/拒绝）
    Q_INVOKABLE void drcRespondEmergencyStop(bool accept);
    /// 调试用虚拟摇杆：直接走本地映射，用来在台架上校方向、不依赖网页端
    Q_INVOKABLE void drcDebugStick(double x, double y, double h, double w);
    Q_INVOKABLE void drcDebugZeroSticks();
    Q_INVOKABLE void drcClearStats();

    int     drcState() const { return _drcState; }
    bool    cloudFlightAuthority() const { return _cloudFlightAuthority; }
    bool    manualControlReady() const;
    bool    cloudPayloadAuthority() const { return _cloudPayloadAuthority; }
    bool    authPending() const { return _authPending; }
    bool    emergencyStopPending() const { return _emergencyStopPending; }
    QString authUserId() const { return _authUserId; }
    QString authUserCallsign() const { return _authUserCallsign; }
    QString brokerAddress() const { return _drcAddress; }
    int     downSilenceMs() const;
    int     osdFrequency() const { return _osdFrequency; }
    int     hsiFrequency() const { return _hsiFrequency; }
    QString lastErrorText() const { return _lastErrorText; }
    QVariantMap pushStats() const { return _pushStats; }
    QVariantMap drcOsd() const { return _lastOsd; }
    QVariantMap cloudStick() const { return _cloudStick; }

signals:
    /// 需要在主连接 events topic 上发布的消息
    void eventToPublish(const QString& method, int needReply, const QJsonObject& data);
    /// 需要在主连接 services_reply topic 上应答的消息
    void serviceReply(const QString& tid, const QString& bid, const QString& method, int result, const QJsonObject& output);
    /// 有用户请求接管飞行控制权，供 UI 弹窗（同时 authPending 变为 true）
    void authRequested(const QString& userId, const QString& callsign);
    /// 待确认的接管请求作废了（本地收回控制权 / 云端释放 / DRC 收尾），UI 应当把弹窗关掉。
    /// 单独开一个信号而不是复用 drcStatusChanged：后者每秒都在发，拿它当边沿会乱关窗。
    void authCancelled();
    /// 云端下发紧急停桨，等操作员确认
    void emergencyStopRequested();
    /// **不是操作员自己点的**断开：云端要求退出指令飞行、下行静默超时、心跳超时。
    /// 这三条路的共同点是：飞机刚刚失去（或即将失去）那个正在驱动它的控制源，
    /// 而操作员可能还不知道 —— 所以必须弹窗提醒接管，不能只靠工具栏那颗图标变色。
    /// 本地点「退出指令飞行」/「收回控制权」不发这个信号（那是操作员自己的动作）。
    /// @param reason 断开原因，直接给操作员看
    /// @param heldFlightAuthority 断开的那一刻飞行控制权是不是还在云端手上
    void remoteControlLost(const QString& reason, bool heldFlightAuthority);
    /// 云端主动交还控制权（cloud_control_release）。与 remoteControlLost 是两回事：
    /// 那是**失去**控制源，这是**优雅交还** —— DRC 会话没断，链路还在，云端随时能再来。
    /// 之所以仍然要弹窗：交还的那一刻飞机可能正被云端驱动着，而操作员不会一直盯着
    /// 工具栏那颗图标。"控制权回到本机了"得有人当场告诉他，否则他会以为还是云端在飞。
    /// 本地操作员自己点「收回控制权」不发这个信号 —— 那是他自己的动作，弹窗只是噪音。
    /// @param hadFlightAuthority 交还的那一刻飞行控制权在不在云端手上
    /// @param hadPayloadAuthority 交还的那一刻负载控制权（相机/云台）在不在云端手上
    void remoteControlReleased(bool hadFlightAuthority, bool hadPayloadAuthority);

    void drcStatusChanged();
    void drcTelemetryChanged();
    /// 云端杆量变化（每次收到 drone_control 都会发，10Hz 量级）
    void cloudStickChanged();

private slots:
    void onDrcStateChanged(QMqttClient::ClientState state);
    void onDrcErrorChanged(QMqttClient::ClientError error);
    void onDrcMessage(const QByteArray& message, const QMqttTopicName& topic);
    /// 25 Hz 保活流。与云端下发的那条流是两回事：DRC 会话活着就一直跑，
    /// 持权且有杆量时重发，否则发中立值。
    void stickTick();
    void heartbeatTick();
    void slowTick();
    void osdTick();
    void onAuthTimeout();
    void onEmergencyStopTimeout();

private:
    void connectToDrcBroker();
    /// 上行发布。DRC 报文的信封是 {tid, bid, timestamp, method, data} —— 没有顶层 seq，
    /// seq 是 data 里的字段（DroneControlRequest.seq / HeartBeatRequest.seq）。
    /// 应答类报文要把下行的 tid/bid 原样带回去，云端靠它们匹配请求与应答；
    /// 主动推送（心跳、osd…）留空则新生成。
    void publishDrcUp(const QString& method, const QJsonObject& data,
                      const QString& tid = QString(), const QString& bid = QString());
    void handleDroneControl(const QJsonObject& data, const QString& tid, const QString& bid);
    void handleEmergencyStop(const QString& tid, const QString& bid);
    /// 下行存活检查：deadman 归零 / 重连 / 彻底超时退出。由 osd 与 slow 定时器调用
    void checkDownLink();
    /// 收尾：归零摇杆 + 停所有定时器 + 断开连接 + 上报状态
    /// @param errorCode 0 表示正常退出，否则为 DjiDrcError 里的码
    /// @param notifyJoystickInvalid 是否发 joystick_invalid_notify（云端主动退出时它自己知道，不用发）
    /// @param notifyOperator 是否发 remoteControlLost() 提醒操作员接管。**只对"不是操作员点的"
    ///        那几条断开路置 true**（云端 drc_mode_exit / 下行静默超时 / 心跳超时）；
    ///        本地主动退出置 false —— 操作员刚点完按钮，再弹一个窗只是噪音。
    void teardownDrc(const QString& reason, int errorCode, bool notifyJoystickInvalid,
                     bool notifyOperator = false);
    void notifyDrcStatus(int errorCode, int drcState);
    void publishJoystickInvalid(int reason);
    void publishAuthNotify(bool accept);
    /// 作废一个还挂着的"等操作员确认"的接管请求：清 pending、停超时表，并按来源
    /// 决定要不要回 cloud_control_auth_notify（不回的话云端那个同步调用会一直等到超时），
    /// 最后发 authCancelled() 让 UI 收窗。
    /// 调用点只有三个：云端释放控制权、本地收回控制权、DRC 收尾 —— 三者都会让
    /// "刚才那个请求"失去意义。**不碰 _consentGranted**：那是调用点自己的语义。
    void cancelPendingAuth(const QString& reason);
    void setLastError(const QString& text);
    void bumpStat(const QString& key, int delta = 1);
    /// 飞行控制权的**唯一**写入口。收口成一个函数是因为它原先有 7 个赋值点，
    /// 而每一个"置 false"都**必须**同时归零杆量（不归零的话最后一条指令会一直挂着，
    /// 而 25 Hz 保活流还会把它持续重发）。这里只负责标志本身，归零与上行通知仍留在
    /// 各调用点 —— 那几个点的语义并不相同（有的要 publishJoystickInvalid，有的不要）。
    void setCloudFlightAuthority(bool granted);
    /// 记下云端下发的杆量并通知 QML（只读摇杆的镜像源）
    /// @param axes 映射后的轴值，由 mapAxes 算好传进来，避免算两遍
    void setCloudStick(double x, double y, double h, double w, qint64 seq,
                       const DjiDrcControlMapper::Axes& axes);
    /// 控制权收回/断开时把镜像摇杆归零，免得停在最后一杆的位置骗人
    void clearCloudStick();

    QJsonObject buildOsdPush() const;
    QJsonObject buildHsiPush() const;
    QJsonObject buildDelayPush() const;
    QJsonObject buildDroneStatePush() const;
    QJsonObject buildCameraOsdPush() const;

    QMqttClient*           _drcClient      = nullptr;
    DjiDrcControlMapper*   _mapper         = nullptr;

    QTimer* _stickTimer     = nullptr;  ///< 40ms(25Hz)：杆量保活流，**持权期间**跑（见 stickTick 的注释）
    QTimer* _heartbeatTimer = nullptr;  ///< 5s：上行心跳 + 下行存活检查
    QTimer* _osdTimer       = nullptr;  ///< 1000/osd_frequency ms：低延时 osd 上行
    QTimer* _slowTimer      = nullptr;  ///< 1s：hsi / delay 上行 + 面板状态刷新
    QTimer* _authTimer      = nullptr;  ///< 云端接管请求的应答超时
    QTimer* _emergencyStopTimer = nullptr; ///< 紧急停桨确认弹窗的超时

    Vehicle* _activeVehicle = nullptr;

    // drc_mode_enter 下发的 broker 凭据
    QString _drcAddress;
    QString _drcClientId;
    QString _drcUsername;
    QString _drcPassword;
    bool    _drcEnableTls = false;

    bool    _drcActive       = false;
    int     _drcState        = 0;        ///< 0 未连接 / 1 连接中 / 2 已连接
    bool    _stateSubscribed = false;    ///< 收到 drc_initial_state_subscribe，需持续推送状态
    int     _osdFrequency    = 10;       ///< 来自 drc_mode_enter，协议默认 10Hz
    int     _hsiFrequency    = 1;        ///< 来自 drc_mode_enter，协议默认 1Hz
    quint64 _seq             = 0;        ///< drc/up 心跳的单调递增序列号

    // ---- 失联保护 ----
    qint64 _lastDownMsgTime   = 0;   ///< 最近一次收到 drc/down 的时刻（deadman 判据）
    int    _deadmanMs         = 500; ///< 下行静默超过它就把摇杆归零
    bool   _sticksZeroed      = true;
    qint64 _reconnectAtMs     = 0;   ///< 已排定的重连时刻，0 表示没排
    qint64 _lastHeartbeatSent = 0;
    int    _heartbeatMisses   = 0;

    // ---- 控制权 ----
    bool    _cloudFlightAuthority  = false;
    bool    _cloudPayloadAuthority = false;
    bool    _authPending           = false;
    QString _authUserId;
    QString _authUserCallsign;
    bool    _emergencyStopPending  = false;
    /// 等确认的紧急停桨的 tid/bid：应答要等操作员点完才发，得先把它们存下来
    QString _emergencyTid;
    QString _emergencyBid;
    /// 操作员是否已同意交出控制权（drcRequireLocalConsent 打开时才看这个）。
    /// 与 _authPending 的区别：前者是"弹窗正开着"，这个是"同意过且还没收回"。
    bool    _consentGranted        = false;
    /// 触发确认弹窗的 services method：cloud_control_auth_request 要回 notify，
    /// 抢控制权那两条已经应答过了，不用再回。
    QString _consentSourceMethod;

    // ---- 统计（面板自检用） ----
    QVariantMap _pushStats;
    QVariantMap _lastOsd;
    QString     _lastErrorText;
    /// 云端最近一杆的原始值（x/y/h/w/has_data/seq）。has_data 为 false 时 QML 不该画摇杆
    QVariantMap _cloudStick;

    /// drone_control 的 seq 去重：官方服务端下行是 publishCount=5 的重复投递，
    /// 同一条指令会到达 5 次。只执行、只应答第一次。
    qint64 _lastDroneControlSeq = -1;
};
