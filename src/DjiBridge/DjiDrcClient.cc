/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "DjiDrcClient.h"
#include "DjiDrcControlMapper.h"
#include "DjiCloudProperties.h"

#include <QtCore/QTimer>
#include <QtCore/QDebug>
#include <QtCore/QDateTime>
#include <QtCore/QJsonDocument>
#include <QtCore/QCoreApplication>
#include <QtCore/QUuid>
#include <QtCore/qmath.h>

#include <algorithm>

#include "SettingsManager.h"
#include "CloudServerSettings.h"
#include "Vehicle.h"
#include "VehicleObjectAvoidance.h"
#include "GimbalController.h"
#include "Gimbal.h"
#include "FactGroup.h"

namespace {

// ---------- 时间常数 ----------
constexpr int kHeartbeatIntervalMs = 5000;  // 上行心跳周期
constexpr int kStickIntervalMs     = 40;    // 杆量保活流周期（25Hz，与 QGC 摇杆同频）

// 下面两个阈值是照云端下行心跳的周期（5s）定的，不是拍脑袋：
// 下行心跳每 5s 一条，所以"静默十几秒"才算链路有问题。这两个值必须明显大于
// 云端心跳周期，否则每次心跳间隙都会被判成断链、反复重连。
constexpr int kReconnectAfterMs = 15000;  // 静默这么久排一次重连
constexpr int kReconnectRetryMs = 5000;   // 重连尝试之间的间隔
constexpr int kSessionTimeoutMs = 30000;  // 静默这么久判定会话已死，退出 DRC

constexpr int kAuthTimeoutMs          = 30000; // 接管请求等操作员确认的超时
constexpr int kEmergencyStopTimeoutMs = 10000; // 紧急停桨确认弹窗的超时

// drc_mode_enter 里的上行频率缺省值（官方服务端固定发 10 / 1）
constexpr int kDefaultOsdFrequency = 10;
constexpr int kDefaultHsiFrequency = 1;

// joystick_invalid_notify 的 reason（JoystickInvalidReasonEnum）
constexpr int kReasonRcLost      = 0; ///< 遥控器失联（这里用来表示下行链路断了）
constexpr int kReasonRcAuthority = 4; ///< 遥控器夺回控制权（本地操作员接管）

// OBSTACLE_DISTANCE 的距离单位是厘米，协议要毫米
constexpr int kCmToMm = 10;
// DISTANCE_SENSOR 落进 fact 时已经换算成米了
constexpr int kMToMm  = 1000;
// hsi 的 around_distances 固定 72 个扇区（每 5°）
constexpr int kHsiSectors = 72;

/// snake_case / camelCase 都认：官方 Java SDK 的字段是驼峰，序列化到线上是下划线，
/// 两种写法在不同后端版本里都见过，取到哪个算哪个。
int readIntEither(const QJsonObject& obj, const QString& snake, const QString& camel, int defaultValue)
{
    if (obj.contains(snake)) {
        return obj.value(snake).toInt(defaultValue);
    }
    if (obj.contains(camel)) {
        return obj.value(camel).toInt(defaultValue);
    }
    return defaultValue;
}

} // namespace

DjiDrcClient::DjiDrcClient(QObject* parent) :
    QObject(parent),
    _drcClient(new QMqttClient(this)),
    _mapper(new DjiDrcControlMapper(this)),
    _stickTimer(new QTimer(this)),
    _heartbeatTimer(new QTimer(this)),
    _osdTimer(new QTimer(this)),
    _slowTimer(new QTimer(this)),
    _authTimer(new QTimer(this)),
    _emergencyStopTimer(new QTimer(this))
{
    connect(_drcClient, &QMqttClient::stateChanged,    this, &DjiDrcClient::onDrcStateChanged);
    connect(_drcClient, &QMqttClient::errorChanged,    this, &DjiDrcClient::onDrcErrorChanged);
    connect(_drcClient, &QMqttClient::messageReceived, this, &DjiDrcClient::onDrcMessage);

    // 25Hz，与 QGC 真实/屏幕摇杆同频，刻意比云端的 10Hz 密。
    //
    // 这条流存在的理由不是"重发云端指令"，而是**让 PX4 始终看得到手动输入源**：
    // 没有稳定输入时 PX4 会判定手动控制丢失（COM_RC_LOSS_T），而且不接受切进
    // 定点/高度这类需要手动输入的模式 —— 而切进去恰恰是云端控制能生效的前提。
    //
    // 起停条件是**持权**，不是"DRC 会话活着"（唯一一处 start() 在
    // setCloudFlightAuthority 里）。理由见 stickTick 的注释：跟会话走的话，
    // 中立值会以 25Hz 去和本地操作员的 25Hz 对撞，把对方的杆量稀释一半。
    // 这段注释以前写反了，这个文件里另外两处（本处、DjiDrcClient.h 的 _stickTimer、
    // stickTick）曾经互相矛盾 —— 实现跟的是"持权"那一份。
    _stickTimer->setInterval(kStickIntervalMs);
    connect(_stickTimer, &QTimer::timeout, this, &DjiDrcClient::stickTick);

    _heartbeatTimer->setInterval(kHeartbeatIntervalMs);
    connect(_heartbeatTimer, &QTimer::timeout, this, &DjiDrcClient::heartbeatTick);

    // 1Hz：hsi / delay 上行 + 面板状态刷新。osd 频率走自己的定时器。
    _slowTimer->setInterval(1000);
    connect(_slowTimer, &QTimer::timeout, this, &DjiDrcClient::slowTick);

    _authTimer->setSingleShot(true);
    connect(_authTimer, &QTimer::timeout, this, &DjiDrcClient::onAuthTimeout);

    _emergencyStopTimer->setSingleShot(true);
    connect(_emergencyStopTimer, &QTimer::timeout, this, &DjiDrcClient::onEmergencyStopTimeout);

    _pushStats[QStringLiteral("heartbeat_up")]      = 0;
    _pushStats[QStringLiteral("drone_control_in")]  = 0;
    _pushStats[QStringLiteral("drone_control_dup")] = 0;
    _pushStats[QStringLiteral("drone_control_rej")] = 0;
    _pushStats[QStringLiteral("osd_up")]            = 0;
    _pushStats[QStringLiteral("hsi_up")]            = 0;
    _pushStats[QStringLiteral("delay_up")]          = 0;
    _pushStats[QStringLiteral("payload_cmd")]       = 0;
    _pushStats[QStringLiteral("emergency_stop")]    = 0;

    // 镜像摇杆的初值：中位、且标注"还没收到过云端杆量"。
    // QML 靠 has_data 区分"云端真的推了中位"和"根本还没推过"。
    clearCloudStick();
}

DjiDrcClient::~DjiDrcClient()
{
    if (_drcClient->state() != QMqttClient::Disconnected) {
        _drcClient->disconnectFromHost();
    }
}

void DjiDrcClient::setActiveVehicle(Vehicle* vehicle)
{
    // 换飞机时先断开旧飞机上的连接。不显式断的话那个连接还挂着，旧飞机
    // （还在 MAVLink 心跳里的话）发 flightModeChanged 会驱动本类的 UI 状态；
    // 而且同一架飞机被重新设回来时会连第二遍，信号发两次。
    if (_activeVehicle) {
        disconnect(_activeVehicle, &Vehicle::flightModeChanged, this, nullptr);
        disconnect(_activeVehicle, &Vehicle::mavCommandResult, this, nullptr);
    }

    _activeVehicle = vehicle;
    _mapper->setVehicle(vehicle);

    if (_activeVehicle) {
        // 模式是 UI 上的一个状态量（manualControlReady → 工具栏"模式拒收"红字），
        // 飞机自己切了模式（飞控失效保护退回 Hold、操作员用遥控器拨档）也得刷新。
        connect(_activeVehicle, &Vehicle::flightModeChanged, this, [this]() {
            emit drcStatusChanged();
        });

        // 诊断：DO_SET_MODE 的应答是区分"飞控拒绝"和"我们根本没发出去"的唯一手段
        // （模式是谁切的都算 —— 云端持权期间飞机一动不动时，先看这里有没有 ACK）。
        connect(_activeVehicle, &Vehicle::mavCommandResult, this,
                [](int vehicleId, int targetComponent, int command, int ackResult, int failureCode) {
                    if (command != MAV_CMD_DO_SET_MODE) {
                        return;
                    }
                    qInfo() << "[DjiDrc] DO_SET_MODE result vehicle:" << vehicleId
                            << "component:" << targetComponent
                            << "ackResult:" << ackResult << "failureCode:" << failureCode;
                });
    }

    // 换飞机时把锁带到新飞机上 —— 不带的话控制权还在云端，锁却留在一架
    // 已经不管的飞机上，新飞机这边本地 25Hz 立刻开始盖云端指令。
    // 刻意**不去解锁旧飞机**：它可能已经被销毁（DjiBridgeServer 也持裸指针，
    // 这是既有的模式，不在这里新增一处解引用）。
    if (_activeVehicle && _cloudFlightAuthority) {
        _activeVehicle->setCloudStickLock(true);
    }

    emit drcStatusChanged();
}

bool DjiDrcClient::manualControlReady() const
{
    // 属性是"云端持权能不能真正控住飞机"的唯一判据：为 false 时 drone_control 会被
    // 逐条拒成 327002，而界面上别的数字一切正常（台架上 1726 条静默拒绝就是这么来的）。
    return _activeVehicle && DjiDrcControlMapper::isManualControlMode(_activeVehicle->flightMode());
}

// ---------------------------------------------------------------------------
// 主连接 services 的转发入口（由 DjiCloudClient 调用）
// ---------------------------------------------------------------------------

void DjiDrcClient::handleAuthRequest(const QString& tid, const QString& bid, const QJsonObject& data)
{
    _authUserId       = data.value(QStringLiteral("user_id")).toString();
    _authUserCallsign = data.value(QStringLiteral("user_callsign")).toString();

    qInfo() << "[DjiDrc] cloud_control_auth_request user:" << _authUserId
            << "callsign:" << _authUserCallsign;

    // 先应答"收到"，决策结果走 cloud_control_auth_notify（events）回给云端
    emit serviceReply(tid, bid, QStringLiteral("cloud_control_auth_request"), DjiDrcError::kSuccess, QJsonObject());

    if (!SettingsManager::instance()->cloudServerSettings()->drcRequireLocalConsent()->rawValue().toBool()) {
        // 默认行为：不弹窗，直接同意 —— 与官方网页端的流程保持一致。
        // 同样要把控制权交出去：不交的话下面那道闸门会把 drone_control 全拒掉。
        qInfo() << "[DjiDrc] auto-granting cloud control (drcRequireLocalConsent is off)";
        _consentGranted        = true;
        _cloudPayloadAuthority = true;
        _lastDroneControlSeq   = -1;
        setCloudFlightAuthority(true);
        publishAuthNotify(true);
        emit drcStatusChanged();
        return;
    }

    // 已经在问操作员了：serviceReply 上面已经回过，这里什么都不能动 ——
    // 尤其**不能**覆盖 _consentSourceMethod，否则 drcRespondAuth 里按来源判断的
    // cloud_control_auth_notify 就发不出去了，云端那个同步调用只能干等到超时。
    if (_authPending) {
        qInfo() << "[DjiDrc] cloud_control_auth_request: already asking local operator, ignored";
        return;
    }

    _authPending         = true;
    _consentSourceMethod = QStringLiteral("cloud_control_auth_request");
    _authTimer->start(kAuthTimeoutMs);
    emit authRequested(_authUserId, _authUserCallsign);
    emit drcStatusChanged();
}

void DjiDrcClient::handleAuthRelease(const QString& tid, const QString& bid, const QJsonObject& data)
{
    Q_UNUSED(data);
    qInfo() << "[DjiDrc] cloud_control_release";

    emit serviceReply(tid, bid, QStringLiteral("cloud_control_release"), DjiDrcError::kSuccess, QJsonObject());

    // 弹窗要如实说"交还前控制权在谁手上"，而下面两行会把它清掉 —— 必须先读出来。
    // （同 teardownDrc 里 heldFlightAuthority 的写法。）
    const bool hadFlightAuthority  = _cloudFlightAuthority;
    const bool hadPayloadAuthority = _cloudPayloadAuthority;

    // 云端既然主动交还，之前那个还挂着的接管请求就已经失去意义了。
    // 不收掉的话：弹窗继续挂在屏幕上，操作员再点一次"是"，就会在云端已经放手之后
    // 又把控制权锁回给它 —— 顺带静默关掉本地摇杆。
    cancelPendingAuth(QStringLiteral("云端释放控制权"));

    // 云端交还控制权：撤销两个控制权标志并立即停手。DRC 连接本身不动 ——
    // 释放控制权和退出 DRC 是两件事，后者是独立的 drc_mode_exit。
    //
    // 顺序：先撤控制权（这一步会停保活流、解锁本地），再补发最后一帧中立值。
    // 反过来的话保活流会在归零之后又把归零前的杆量发出去。
    _cloudPayloadAuthority = false;
    _consentGranted        = false;
    setCloudFlightAuthority(false);
    _mapper->zeroSticks();
    _sticksZeroed = true;
    clearCloudStick();
    publishJoystickInvalid(kReasonRcAuthority);
    emit drcStatusChanged();

    // 放在最后：弹窗读的是收尾**之后**的状态（本地已解锁、控制权已回到本机），
    // 早发的话操作员看到的是还没落定的中间态。
    //
    // 两样控制权都没在云端手上时**不弹**：什么都没交还，弹出来纯属噪音。
    // 典型就是操作员刚点了「否」、或者那个接管请求超时作废之后云端才发的释放 ——
    // 那两种情况操作员已经通过关掉的弹窗知道结果了。
    if (hadFlightAuthority || hadPayloadAuthority) {
        qInfo() << "[DjiDrc] cloud handed control back, flight:" << hadFlightAuthority
                << "payload:" << hadPayloadAuthority;
        emit remoteControlReleased(hadFlightAuthority, hadPayloadAuthority);
    }
}

void DjiDrcClient::handleDrcModeEnter(const QString& tid, const QString& bid, const QJsonObject& data)
{
    const QJsonObject broker = data.value(QStringLiteral("mqtt_broker")).toObject();
    _drcAddress   = broker.value(QStringLiteral("address")).toString();
    _drcClientId  = broker.value(QStringLiteral("client_id")).toString();
    _drcUsername  = broker.value(QStringLiteral("username")).toString();
    _drcPassword  = broker.value(QStringLiteral("password")).toString();
    _drcEnableTls = broker.value(QStringLiteral("enable_tls")).toBool();

    // 上行频率由云端指定（osd 默认 10Hz、hsi 默认 1Hz）
    _osdFrequency = qBound(1, readIntEither(data, QStringLiteral("osd_frequency"),
                                            QStringLiteral("osdFrequency"), kDefaultOsdFrequency), 50);
    _hsiFrequency = qBound(1, readIntEither(data, QStringLiteral("hsi_frequency"),
                                            QStringLiteral("hsiFrequency"), kDefaultHsiFrequency), 10);
    _osdTimer->setInterval(1000 / _osdFrequency);

    qInfo() << "[DjiDrc] drc_mode_enter broker:" << _drcAddress << "osd:" << _osdFrequency
            << "Hz hsi:" << _hsiFrequency << "Hz";

    if (_drcAddress.isEmpty()) {
        qWarning() << "[DjiDrc] drc_mode_enter missing mqtt_broker.address";
        setLastError(QStringLiteral("drc_mode_enter 缺少 mqtt_broker.address"));
        emit serviceReply(tid, bid, QStringLiteral("drc_mode_enter"), DjiDrcError::kDrcAbnormal, QJsonObject());
        notifyDrcStatus(DjiDrcError::kDrcAbnormal, 0);
        return;
    }

    // 应答收到；连接结果通过 drc_status_notify 事件上报
    emit serviceReply(tid, bid, QStringLiteral("drc_mode_enter"), DjiDrcError::kSuccess, QJsonObject());

    notifyDrcStatus(DjiDrcError::kSuccess, 1); // connecting
    connectToDrcBroker();
}

void DjiDrcClient::handleDrcModeExit(const QString& tid, const QString& bid, const QJsonObject& data)
{
    Q_UNUSED(data);
    qInfo() << "[DjiDrc] drc_mode_exit";

    emit serviceReply(tid, bid, QStringLiteral("drc_mode_exit"), DjiDrcError::kSuccess, QJsonObject());

    // 本地主动退出与云端要求退出走同一条收尾路径，区别只在 notifyOperator：
    // 云端主动退出时操作员必须被告知（控制源没了），本地退出不需要（刚点完按钮）。
    teardownDrc(QStringLiteral("云端要求退出指令飞行"), DjiDrcError::kSuccess, false, /*notifyOperator*/ true);
}

void DjiDrcClient::handleFlightAuthorityGrab(const QString& tid, const QString& bid, const QJsonObject& data)
{
    Q_UNUSED(data);
    if (!_drcActive) {
        // 还没进 DRC 就谈不上控制权
        setLastError(QStringLiteral("抢飞行控制权失败：DRC 未连接"));
        emit serviceReply(tid, bid, QStringLiteral("flight_authority_grab"), DjiDrcError::kDrcAbnormal, QJsonObject());
        return;
    }

    // 需要操作员确认时：先答应下来（云端的 HTTP 调用是同步等的，回非 0 会直接报错），
    // 但把同意与否交给操作员 —— 没同意之前 drone_control 一律拒绝。
    if (SettingsManager::instance()->cloudServerSettings()->drcRequireLocalConsent()->rawValue().toBool()
        && !_consentGranted) {
        // 已经在问操作员了：云端的 HTTP 调用还在同步等，serviceReply 必须回；
        // 但状态一律不动（覆盖 _consentSourceMethod 会让 auth_notify 发不出去），
        // 也不要重启 _authTimer —— 那等于把操作员的思考时间偷偷续期。
        if (_authPending) {
            qInfo() << "[DjiDrc] flight_authority_grab: already asking local operator, ignored";
            emit serviceReply(tid, bid, QStringLiteral("flight_authority_grab"), DjiDrcError::kSuccess, QJsonObject());
            return;
        }
        qInfo() << "[DjiDrc] flight_authority_grab: asking local operator";
        emit serviceReply(tid, bid, QStringLiteral("flight_authority_grab"), DjiDrcError::kSuccess, QJsonObject());
        _authUserId           = data.value(QStringLiteral("user_id")).toString();
        _authUserCallsign     = data.value(QStringLiteral("user_callsign")).toString();
        _authPending          = true;
        _consentSourceMethod  = QStringLiteral("flight_authority_grab");
        _authTimer->start(kAuthTimeoutMs);
        emit authRequested(_authUserId, _authUserCallsign);
        emit drcStatusChanged();
        return;
    }

    _lastDroneControlSeq  = -1; // 换了控制权持有者，seq 从头算
    qInfo() << "[DjiDrc] flight authority granted to cloud";
    setCloudFlightAuthority(true);
    emit serviceReply(tid, bid, QStringLiteral("flight_authority_grab"), DjiDrcError::kSuccess, QJsonObject());
    emit drcStatusChanged();
}

void DjiDrcClient::handlePayloadAuthorityGrab(const QString& tid, const QString& bid, const QJsonObject& data)
{
    if (!_drcActive) {
        setLastError(QStringLiteral("抢负载控制权失败：DRC 未连接"));
        emit serviceReply(tid, bid, QStringLiteral("payload_authority_grab"), DjiDrcError::kDrcAbnormal, QJsonObject());
        return;
    }

    if (SettingsManager::instance()->cloudServerSettings()->drcRequireLocalConsent()->rawValue().toBool()
        && !_consentGranted) {
        // 同 flight_authority_grab：已经在问了就只回应答，状态不动
        if (_authPending) {
            qInfo() << "[DjiDrc] payload_authority_grab: already asking local operator, ignored";
            emit serviceReply(tid, bid, QStringLiteral("payload_authority_grab"), DjiDrcError::kSuccess, QJsonObject());
            return;
        }
        qInfo() << "[DjiDrc] payload_authority_grab: asking local operator";
        emit serviceReply(tid, bid, QStringLiteral("payload_authority_grab"), DjiDrcError::kSuccess, QJsonObject());
        // 这两行原来漏了：不读的话弹窗显示的是上一条请求的用户，或者干脆空白
        _authUserId          = data.value(QStringLiteral("user_id")).toString();
        _authUserCallsign    = data.value(QStringLiteral("user_callsign")).toString();
        _authPending         = true;
        _consentSourceMethod = QStringLiteral("payload_authority_grab");
        _authTimer->start(kAuthTimeoutMs);
        emit authRequested(_authUserId, _authUserCallsign);
        emit drcStatusChanged();
        return;
    }

    _cloudPayloadAuthority = true;
    qInfo() << "[DjiDrc] payload authority granted to cloud";
    emit serviceReply(tid, bid, QStringLiteral("payload_authority_grab"), DjiDrcError::kSuccess, QJsonObject());
    emit drcStatusChanged();
}

bool DjiDrcClient::handlePayloadControl(const QString& method, const QString& tid, const QString& bid,
                                        const QJsonObject& data)
{
    // 只有这一批是负载控制；其余（fly_to_point / takeoff_to_point 等）交回调用方
    static const QStringList kPayloadMethods = {
        QStringLiteral("camera_mode_switch"),
        QStringLiteral("camera_photo_take"),
        QStringLiteral("camera_photo_stop"),
        QStringLiteral("camera_recording_start"),
        QStringLiteral("camera_recording_stop"),
        QStringLiteral("camera_focal_length_set"),
        QStringLiteral("gimbal_reset"),
        QStringLiteral("camera_aim"),
        QStringLiteral("camera_look_at"),
    };
    if (!kPayloadMethods.contains(method)) {
        return false;
    }

    bumpStat(QStringLiteral("payload_cmd"));

    // 没有负载控制权就不动手（与 flight_authority_grab 对称）
    if (!_cloudPayloadAuthority) {
        qWarning() << "[DjiDrc] payload command without authority:" << method;
        setLastError(QStringLiteral("无负载控制权，已拒绝 %1").arg(method));
        emit serviceReply(tid, bid, method, DjiDrcError::kObtainControlFailed, QJsonObject());
        return true;
    }

    int result = DjiDrcError::kSuccess;
    if (method == QStringLiteral("camera_photo_take")) {
        result = _mapper->cameraPhotoTake();
    } else if (method == QStringLiteral("camera_photo_stop")) {
        result = _mapper->cameraPhotoStop();
    } else if (method == QStringLiteral("camera_recording_start")) {
        result = _mapper->cameraRecordingStart();
    } else if (method == QStringLiteral("camera_recording_stop")) {
        result = _mapper->cameraRecordingStop();
    } else if (method == QStringLiteral("camera_mode_switch")) {
        result = _mapper->cameraModeSwitch(data.value(QStringLiteral("camera_mode")).toInt(-1));
    } else if (method == QStringLiteral("camera_focal_length_set")) {
        result = _mapper->cameraFocalLengthSet(data.value(QStringLiteral("zoom_factor")).toDouble());
    } else if (method == QStringLiteral("gimbal_reset")) {
        result = _mapper->gimbalReset(data.value(QStringLiteral("reset_mode")).toInt(-1));
    } else if (method == QStringLiteral("camera_aim")) {
        result = _mapper->cameraAim(data.value(QStringLiteral("x")).toDouble(),
                                   data.value(QStringLiteral("y")).toDouble());
    } else if (method == QStringLiteral("camera_look_at")) {
        result = _mapper->cameraLookAt(data.value(QStringLiteral("latitude")).toDouble(),
                                      data.value(QStringLiteral("longitude")).toDouble(),
                                      data.value(QStringLiteral("height")).toDouble());
    }

    if (result != DjiDrcError::kSuccess) {
        setLastError(QStringLiteral("%1 失败(%2)：%3").arg(method).arg(result).arg(_mapper->lastError()));
        qWarning() << "[DjiDrc] payload command failed:" << method << result << _mapper->lastError();
    }
    emit serviceReply(tid, bid, method, result, QJsonObject());
    return true;
}

// ---------------------------------------------------------------------------
// QML 入口
// ---------------------------------------------------------------------------

void DjiDrcClient::drcRespondAuth(bool accept)
{
    if (!_authPending) {
        return;
    }

    _authPending = false;
    _authTimer->stop();
    _consentGranted = accept;

    qInfo() << "[DjiDrc] operator" << (accept ? "granted" : "denied")
            << "cloud control (" << _consentSourceMethod << ")";

    if (!accept) {
        _cloudPayloadAuthority = false;
        setCloudFlightAuthority(false);
        _mapper->zeroSticks();
        _sticksZeroed = true;
        clearCloudStick();
    } else {
        // 同意 = 把刚才那条请求对应的控制权**真的**交出去。这一步不能省：
        // 抢控制权时我们已经先回了 result=0，云端（官方后端 seizeAuthority() 里
        // checkAuthorityFlight 命中就短路）就此认定控制权到手，不会再补发一次，
        // 所以在这里不落地的话，操作员点了"同意"之后 drone_control 仍会被下面
        // 那道 _cloudFlightAuthority 闸门全数拒掉，后台永远控不了飞机。
        if (_consentSourceMethod == QStringLiteral("flight_authority_grab")
            || _consentSourceMethod == QStringLiteral("cloud_control_auth_request")) {
            _lastDroneControlSeq  = -1; // 换了控制权持有者，seq 从头算
            qInfo() << "[DjiDrc] flight authority granted to cloud (operator consent)";
            setCloudFlightAuthority(true);
        }
        if (_consentSourceMethod == QStringLiteral("payload_authority_grab")
            || _consentSourceMethod == QStringLiteral("cloud_control_auth_request")) {
            _cloudPayloadAuthority = true;
            qInfo() << "[DjiDrc] payload authority granted to cloud (operator consent)";
        }
    }

    // 只有 cloud_control_auth_request 这条流程需要回 notify；
    // 抢控制权那两条在收到时就已经应答过 services_reply 了。
    if (_consentSourceMethod == QStringLiteral("cloud_control_auth_request")) {
        publishAuthNotify(accept);
    }
    _consentSourceMethod.clear();
    emit drcStatusChanged();
}

void DjiDrcClient::drcGrabFlightAuthority(bool grab)
{
    // 本地把控制权收回来的同时，把"云端正在请求接管"的弹窗也作废掉。
    // 不这么做就是一条真实的安全漏洞：弹窗还开着，操作员顺手点"是"，
    // drcRespondAuth(true) 会把控制权又交给云端，而 setCloudStickLock(true)
    // 会**静默**关掉本地摇杆 —— 操作员以为自己刚夺回了控制权。
    if (!grab) {
        cancelPendingAuth(QStringLiteral("本地收回飞行控制权"));
    }

    _consentGranted = grab;
    if (grab) {
        setCloudFlightAuthority(true);
    } else {
        setCloudFlightAuthority(false);
        _mapper->zeroSticks();
        _sticksZeroed = true;
        clearCloudStick();
        publishJoystickInvalid(kReasonRcAuthority);
    }
    qInfo() << "[DjiDrc] local operator" << (grab ? "grabbed" : "released") << "flight authority";
    emit drcStatusChanged();
}

void DjiDrcClient::drcGrabPayloadAuthority(bool grab)
{
    _cloudPayloadAuthority = grab;
    qInfo() << "[DjiDrc] local operator" << (grab ? "grabbed" : "released") << "payload authority";
    emit drcStatusChanged();
}

void DjiDrcClient::drcRequestExit()
{
    qInfo() << "[DjiDrc] local operator requested DRC exit";
    // notifyOperator=false：这一步是操作员自己点的，他知道自己在干什么
    teardownDrc(QStringLiteral("本地退出"), DjiDrcError::kSuccess, true, /*notifyOperator*/ false);
}

void DjiDrcClient::drcEmergencyStop()
{
    // 本地按钮：QML 那边已经二次确认过了
    _emergencyStopPending = false;
    _emergencyStopTimer->stop();
    if (_mapper->emergencyStop()) {
        bumpStat(QStringLiteral("emergency_stop"));
    }
    emit drcStatusChanged();
}

void DjiDrcClient::drcRespondEmergencyStop(bool accept)
{
    if (!_emergencyStopPending) {
        return;
    }

    _emergencyStopPending = false;
    _emergencyStopTimer->stop();

    int result = DjiDrcError::kSuccess;
    if (accept) {
        if (!_mapper->emergencyStop()) {
            result = DjiDrcError::kObtainControlFailed;
        } else {
            bumpStat(QStringLiteral("emergency_stop"));
        }
    } else {
        // 操作员拒绝：明确回非 0，别让云端以为停桨了
        result = DjiDrcError::kObtainControlFailed;
        qWarning() << "[DjiDrc] operator REFUSED cloud emergency stop";
    }

    QJsonObject data;
    data[QStringLiteral("result")] = result;
    data[QStringLiteral("output")] = QJsonObject();
    publishDrcUp(QStringLiteral("drone_emergency_stop"), data, _emergencyTid, _emergencyBid);
    _emergencyTid.clear();
    _emergencyBid.clear();
    emit drcStatusChanged();
}

void DjiDrcClient::drcDebugStick(double x, double y, double h, double w)
{
    // 调试通道：不走 MQTT，直接喂给本地映射，用来在台架上校方向
    const auto result = _mapper->applyDroneControl(x, y, h, w);
    if (result != DjiDrcControlMapper::JoystickResult::Ok) {
        setLastError(QStringLiteral("调试摇杆被拒：%1").arg(_mapper->lastError()));
    } else {
        _sticksZeroed = false;
    }
    emit drcStatusChanged();
}

void DjiDrcClient::drcDebugZeroSticks()
{
    _mapper->zeroSticks();
    _sticksZeroed = true;
    emit drcStatusChanged();
}

void DjiDrcClient::drcClearStats()
{
    for (auto it = _pushStats.begin(); it != _pushStats.end(); ++it) {
        it.value() = 0;
    }
    _lastErrorText.clear();
    emit drcStatusChanged();
}

// ---------------------------------------------------------------------------
// DRC 独立连接
// ---------------------------------------------------------------------------

void DjiDrcClient::connectToDrcBroker()
{
    if (_drcAddress.isEmpty()) {
        return;
    }

    // 解析 "tcp://1.2.3.4:8883" / "ssl://host:8883" / "host:1883"
    QString clean = _drcAddress;
    const int schemeIdx = clean.indexOf(QStringLiteral("://"));
    if (schemeIdx >= 0) {
        clean = clean.mid(schemeIdx + 3);
    }

    QString host;
    quint16 port = _drcEnableTls ? 8883 : 1883;
    const int colonIdx = clean.lastIndexOf(':');
    if (colonIdx > 0) {
        host = clean.left(colonIdx);
        port = clean.mid(colonIdx + 1).toUShort();
    } else {
        host = clean;
    }

    // broker 凭据里的 client_id 有些后端不给（官方 demo 会给）。空 clientId 连不上，
    // 兜一个和主连接同风格的 id —— 前缀不同是有意的：DRC 是第二条连接，
    // 两条连接用同一个 clientId 会被 broker 互相踢下线。
    QString clientId = _drcClientId;
    if (clientId.isEmpty()) {
        clientId = QStringLiteral("DGCSDrc-") + QString::number(QCoreApplication::applicationPid());
    }

    _drcClient->setHostname(host);
    _drcClient->setPort(port);
    _drcClient->setUsername(_drcUsername);
    _drcClient->setPassword(_drcPassword);
    _drcClient->setClientId(clientId);
    _drcClient->setProtocolVersion(QMqttClient::MQTT_5_0);
    _drcClient->setKeepAlive(60);
    _drcClient->setAutoKeepAlive(true);

    qInfo() << "[DjiDrc] connecting to DRC broker:" << host << ":" << port
            << "clientId:" << clientId << "tls:" << _drcEnableTls;

    if (_drcEnableTls) {
        _drcClient->connectToHostEncrypted(QSslConfiguration());
    } else {
        _drcClient->connectToHost();
    }
}

void DjiDrcClient::onDrcStateChanged(QMqttClient::ClientState state)
{
    switch (state) {
    case QMqttClient::Connected: {
        _drcActive       = true;
        _drcState        = 2;
        _lastDownMsgTime = QDateTime::currentMSecsSinceEpoch();
        _reconnectAtMs   = 0;
        _heartbeatMisses = 0;

        const QString gcsSn = SettingsManager::instance()->cloudServerSettings()->gcsSn()->rawValueString();
        _drcClient->subscribe(QMqttTopicFilter(QStringLiteral("thing/product/") + gcsSn + QStringLiteral("/drc/down")));

        _heartbeatTimer->start();
        _osdTimer->start();
        _slowTimer->start();

        notifyDrcStatus(DjiDrcError::kSuccess, 2);
        qInfo() << "[DjiDrc] DRC broker connected, subscribed drc/down";
        emit drcStatusChanged();
        break;
    }
    case QMqttClient::Connecting:
        _drcState = 1;
        emit drcStatusChanged();
        break;
    case QMqttClient::Disconnected:
        // 连接掉了但会话没结束（比如重连中）：这里不归零、不停表，
        // 交给 checkDownLink 统一处理，避免重连瞬间把状态机搅乱
        if (_drcActive) {
            _drcActive = false;
            _drcState  = 1; // 仍算"连接中"（重连在即）
        }
        emit drcStatusChanged();
        break;
    default:
        break;
    }
}

void DjiDrcClient::onDrcErrorChanged(QMqttClient::ClientError error)
{
    if (error == QMqttClient::NoError) {
        return;
    }

    qWarning() << "[DjiDrc] DRC broker error:" << static_cast<int>(error) << "host:" << _drcAddress;

    // 错误分类上报，让云端能区分"凭据不对"和"网络断了"。
    // 分档照 QMqttClient::ClientError 的两段：1~5 是 broker 的协议级拒绝
    //（协议版本、clientId、账号密码、未授权），256 起是 Qt 侧的链路问题。
    int code = DjiDrcError::kDrcLinkLost;
    switch (error) {
    case QMqttClient::IdRejected:
    case QMqttClient::BadUsernameOrPassword:
    case QMqttClient::NotAuthorized:
        // 凭据类：broker 收到了连接但认下了我们。DJI 的错误码里没有更贴切的，
        // 用"证书异常"这一档（都是"凭据没通过"）
        code = DjiDrcError::kDrcCertificateAbnormal;
        break;
    case QMqttClient::InvalidProtocolVersion:
    case QMqttClient::ServerUnavailable:
        code = DjiDrcError::kDrcLinkRefused;
        break;
    case QMqttClient::TransportInvalid:
    case QMqttClient::ProtocolViolation:
    case QMqttClient::UnknownError:
    case QMqttClient::Mqtt5SpecificError:
    default:
        code = DjiDrcError::kDrcLinkLost;
        break;
    }
    setLastError(QStringLiteral("DRC broker 错误 %1").arg(static_cast<int>(error)));
    notifyDrcStatus(code, 0);
}

// ---------------------------------------------------------------------------
// drc/down 分发
// ---------------------------------------------------------------------------

void DjiDrcClient::onDrcMessage(const QByteArray& message, const QMqttTopicName& topic)
{
    _lastDownMsgTime = QDateTime::currentMSecsSinceEpoch();

    const QJsonObject msg    = QJsonDocument::fromJson(message).object();
    const QString     method = msg.value(QStringLiteral("method")).toString();
    const QString     tid    = msg.value(QStringLiteral("tid")).toString();
    const QString     bid    = msg.value(QStringLiteral("bid")).toString();
    const QJsonObject data   = msg.value(QStringLiteral("data")).toObject();

    qDebug() << "[DjiDrc] recv" << topic.name() << "method:" << method;

    if (method == QStringLiteral("drone_control")) {
        handleDroneControl(data, tid, bid);
    } else if (method == QStringLiteral("drone_emergency_stop")) {
        handleEmergencyStop(tid, bid);
    } else if (method == QStringLiteral("heart_beat")) {
        // 回一条心跳，把下行的 seq 带回去（HeartBeatRequest 的 seq 是 data 里的字段）
        QJsonObject reply;
        reply[QStringLiteral("seq")]       = data.value(QStringLiteral("seq")).toVariant().toLongLong();
        reply[QStringLiteral("timestamp")] = QDateTime::currentMSecsSinceEpoch();
        publishDrcUp(QStringLiteral("heart_beat"), reply, tid, bid);
        bumpStat(QStringLiteral("heartbeat_up"));
    } else if (method == QStringLiteral("drc_initial_state_subscribe")) {
        // 订阅远程控制状态：应答 result 并开始随心跳持续推送状态
        _stateSubscribed = true;
        QJsonObject reply;
        reply[QStringLiteral("result")] = DjiDrcError::kSuccess;
        publishDrcUp(QStringLiteral("drc_initial_state_subscribe"), reply, tid, bid);
        publishDrcUp(QStringLiteral("drc_drone_state_push"), buildDroneStatePush());
        publishDrcUp(QStringLiteral("drc_camera_osd_info_push"), buildCameraOsdPush());
        bumpStat(QStringLiteral("state_subscribe"));
    } else {
        qWarning() << "[DjiDrc] unsupported drc/down method:" << method;
    }
}

void DjiDrcClient::stickTick()
{
    // 只有云端持权时才发（定时器也是跟着控制权起停的，这里是防御性判断）。
    //
    // 刻意**不**跟"DRC 会话活着"走：那样在"会话在、但云端还没拿到控制权"的窗口里，
    // 保活流的中立值会以 25Hz 去和本地操作员的指令对撞 —— 两个发送者各 25Hz，
    // 操作员的杆量被稀释一半。这恰恰是本次要消灭的那类 bug，不能自己再造一个。
    if (!_cloudFlightAuthority) {
        return;
    }
    // 死锁（checkDownLink）会把杆量归零但**不动控制权**。这时不能直接 return ——
    // 断流会让 PX4 判定手动控制丢失，而这条流正是它手动输入的来源；飞机悬停着
    // 比触发失效保护安全。zeroSticks() 已经清掉了"上一次杆量"，所以这里发出去的
    // 必然是中立值，不会把归零的杆量复活。
    _mapper->resendLastAxes();
}

void DjiDrcClient::setCloudFlightAuthority(bool granted)
{
    if (_cloudFlightAuthority == granted) {
        // 重复授权不是"变更"：不去打扰既有状态（云端重发一次 grab 不该把
        // 正在生效的杆量清掉）。重复撤销同理。
        return;
    }
    _cloudFlightAuthority = granted;

    if (granted) {
        // **先上锁、再起流。** 反过来的话会有一个"两个发送者并存"的窗口：
        // 保活流已经在发，本地那条 25Hz 也在发，谁最后到飞控就听谁的。
        if (_activeVehicle) {
            _activeVehicle->setCloudStickLock(true);
        }
        _stickTimer->start();
    } else {
        _stickTimer->stop();
        if (_activeVehicle) {
            _activeVehicle->setCloudStickLock(false);
        }
    }

    // 放在最后发：QML 在这条信号里把 pad 推回中位（见 VirtualJoystick.qml 的
    // onCloudDrivingChanged）。emit 是同步派发，所以本函数返回时 pad 已经在中位 ——
    // 本地那条 25Hz 流要等本轮事件循环结束才可能触发，那时它读到的必然是中位。
    emit drcStatusChanged();
}

void DjiDrcClient::handleDroneControl(const QJsonObject& data, const QString& tid, const QString& bid)
{
    // freq / delay_time 是发送端声明的期望频率与时延（协议范围 2~10Hz、100~1000ms），
    // 拿它来定 deadman 阈值：至少 3 个周期、且不小于发送端声明的时延。
    const int freq      = data.value(QStringLiteral("freq")).toInt(0);
    const int delayTime = readIntEither(data, QStringLiteral("delay_time"), QStringLiteral("delayTime"), 0);
    if (freq >= 2 && freq <= 10) {
        _deadmanMs = std::max(500, std::max(3000 / freq, delayTime));
    }

    bumpStat(QStringLiteral("drone_control_in"));

    // 官方服务端下行是 publishCount=5 的重复投递：同一条 seq 会到达 5 次。
    // 只执行第一次、只应答第一次（重复应答会被云端当成多条指令的结果）。
    const qint64 seq = data.value(QStringLiteral("seq")).toVariant().toLongLong();
    if (seq == _lastDroneControlSeq && seq >= 0) {
        bumpStat(QStringLiteral("drone_control_dup"));
        return;
    }
    _lastDroneControlSeq = seq;

    const double sx = data.value(QStringLiteral("x")).toDouble();
    const double sy = data.value(QStringLiteral("y")).toDouble();
    const double sh = data.value(QStringLiteral("h")).toDouble();
    const double sw = data.value(QStringLiteral("w")).toDouble();

    // 镜像给 QML：原始值和映射后的轴值都记。原始值保证"后台推了什么就显示什么"
    // （即使这杆被拒也不会显示成零），轴值则是画摇杆该用的 —— 映射里有取反和
    // drcInvertX/Y/W，用原始值画会出现"把手指一边、飞机飞另一边"。
    // 放在去重之后，免得 publishCount=5 的重复投递让摇杆白白重绘 5 次。
    setCloudStick(sx, sy, sh, sw, seq, _mapper->mapAxes(sx, sy, sh, sw));

    QJsonObject output;
    output[QStringLiteral("seq")] = seq;

    QJsonObject reply;
    reply[QStringLiteral("output")] = output;

    int result = DjiDrcError::kSuccess;
    if (!_cloudFlightAuthority) {
        result = DjiDrcError::kObtainControlFailed;
        bumpStat(QStringLiteral("drone_control_rej"));
        setLastError(QStringLiteral("收到 drone_control 但未持有飞行控制权"));
    } else {
        const auto joystickResult = _mapper->applyDroneControl(data.value(QStringLiteral("x")).toDouble(),
                                                               data.value(QStringLiteral("y")).toDouble(),
                                                               data.value(QStringLiteral("h")).toDouble(),
                                                               data.value(QStringLiteral("w")).toDouble());
        if (joystickResult == DjiDrcControlMapper::JoystickResult::Ok) {
            _sticksZeroed = false;
        } else {
            result = DjiDrcControlMapper::joystickResultCode(joystickResult);
            bumpStat(QStringLiteral("drone_control_rej"));
            setLastError(QStringLiteral("摇杆被拒：%1").arg(_mapper->lastError()));
            qWarning() << "[DjiDrc] drone_control refused:" << _mapper->lastError();
        }
    }

    reply[QStringLiteral("result")] = result;
    publishDrcUp(QStringLiteral("drone_control"), reply, tid, bid);
}

void DjiDrcClient::handleEmergencyStop(const QString& tid, const QString& bid)
{
    // 紧急停桨映射到 MAV_CMD_DO_FLIGHTTERMINATION，不可逆，所以除了设置里的总开关
    // 之外还要操作员在弹窗上点头。
    const bool enabled = SettingsManager::instance()->cloudServerSettings()
                             ->drcEmergencyStopEnabled()->rawValue().toBool();
    if (!enabled) {
        qWarning() << "[DjiDrc] drone_emergency_stop refused: disabled by setting";
        setLastError(QStringLiteral("收到紧急停桨，但设置里未启用，已拒绝"));
        QJsonObject data;
        data[QStringLiteral("result")] = DjiDrcError::kDrcAbnormal;
        data[QStringLiteral("output")] = QJsonObject();
        publishDrcUp(QStringLiteral("drone_emergency_stop"), data, tid, bid);
        return;
    }

    if (_emergencyStopPending) {
        return; // 已经有一个确认弹窗在等
    }

    qWarning() << "[DjiDrc] drone_emergency_stop from cloud, waiting for operator confirmation";
    _emergencyStopPending = true;
    _emergencyTid         = tid;
    _emergencyBid         = bid;
    _emergencyStopTimer->start(kEmergencyStopTimeoutMs);
    emit emergencyStopRequested();
    emit drcStatusChanged();
}

void DjiDrcClient::checkDownLink()
{
    // 判据是 drcState 而不是 drcActive：连接掉了之后 drcActive 就是 false，
    // 而"掉了要重连"恰恰是这时候该做的事 —— 用 drcActive 当闸门的话，
    // 一次断线之后就再也不会重连、也不会超时退出了（drcState 掉线时停在 1）。
    if (_drcState == 0 || _lastDownMsgTime <= 0) {
        return;
    }

    const qint64 now     = QDateTime::currentMSecsSinceEpoch();
    const qint64 silence = now - _lastDownMsgTime;

    // 1) deadman：下行静默超过阈值就把摇杆归零。链路抖一下不退出（退出了很难用），
    //    但摇杆必须马上归零 —— 否则最后一条速度指令会一直挂着。
    if (!_sticksZeroed && silence > _deadmanMs) {
        _mapper->zeroSticks();
        _sticksZeroed = true;
        // 实际杆量已经归零了，镜像摇杆也得跟着归零，否则界面上还停在最后一杆的位置
        clearCloudStick();
        qWarning() << "[DjiDrc] down-link silent" << silence << "ms, sticks zeroed";
        setLastError(QStringLiteral("下行静默 %1ms，摇杆已归零").arg(silence));
        publishJoystickInvalid(kReasonRcLost);
        emit drcStatusChanged();
    }

    // 2) 链路疑似断了：排一次重连。
    //    重试间隔 5 秒：broker 没起来的时候，1 秒一次的连接尝试会把日志刷满，
    //    而且这么密的重试对"网线被拔了"这种情形也没有任何帮助。
    if (silence > kReconnectAfterMs) {
        if (_reconnectAtMs == 0) {
            _reconnectAtMs = now + kReconnectRetryMs;
            qWarning() << "[DjiDrc] down-link silent" << silence << "ms, reconnect scheduled";
        } else if (now >= _reconnectAtMs) {
            _reconnectAtMs = 0;
            qWarning() << "[DjiDrc] reconnecting to DRC broker";
            if (_drcClient->state() != QMqttClient::Disconnected) {
                _drcClient->disconnectFromHost();
            }
            connectToDrcBroker();
        }
    }

    // 3) 会话已死：退出并把心跳超时上报给云端
    if (silence > kSessionTimeoutMs) {
        qWarning() << "[DjiDrc] DRC session timed out after" << silence << "ms of silence";
        teardownDrc(QStringLiteral("下行静默超时"), DjiDrcError::kDrcHeartbeatTimeout, true, /*notifyOperator*/ true);
    }
}

// ---------------------------------------------------------------------------
// 定时器
// ---------------------------------------------------------------------------

void DjiDrcClient::heartbeatTick()
{
    checkDownLink();
    if (!_drcActive) {
        return;
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    QJsonObject data;
    data[QStringLiteral("seq")]       = static_cast<qint64>(_seq++);
    data[QStringLiteral("timestamp")] = now;
    publishDrcUp(QStringLiteral("heart_beat"), data);
    bumpStat(QStringLiteral("heartbeat_up"));

    // 上行也断了？连续两次心跳之后仍没收到任何下行，就判定链路死掉
    if (_lastHeartbeatSent > 0 && _lastDownMsgTime < _lastHeartbeatSent) {
        _heartbeatMisses++;
        if (_heartbeatMisses >= 2) {
            qWarning() << "[DjiDrc] heartbeat timeout: no down-link after two heartbeats";
            teardownDrc(QStringLiteral("心跳超时"), DjiDrcError::kDrcHeartbeatTimeout, true, /*notifyOperator*/ true);
            return;
        }
    } else {
        _heartbeatMisses = 0;
    }
    _lastHeartbeatSent = now;

    // 订阅过远程控制状态的，随心跳推状态（5s 一次，不必更密）
    if (_stateSubscribed) {
        publishDrcUp(QStringLiteral("drc_drone_state_push"), buildDroneStatePush());
        publishDrcUp(QStringLiteral("drc_camera_osd_info_push"), buildCameraOsdPush());
    }

    emit drcStatusChanged();
}

void DjiDrcClient::osdTick()
{
    checkDownLink();
    if (!_drcActive) {
        return;
    }

    const QJsonObject osd = buildOsdPush();
    if (osd.isEmpty()) {
        return;
    }
    _lastOsd = osd.toVariantMap();
    publishDrcUp(QStringLiteral("osd_info_push"), osd);
    bumpStat(QStringLiteral("osd_up"));
    emit drcTelemetryChanged();
}

void DjiDrcClient::slowTick()
{
    checkDownLink();
    if (!_drcActive) {
        return;
    }

    if (_hsiFrequency > 1) {
        // 协议允许 hsi 频率更高，但 MAVLink 的 OBSTACLE_DISTANCE 本身就是低频的，
        // 这里按 1Hz 发，别把同一条避障数据重复报上去。
        static bool warned = false;
        if (!warned) {
            warned = true;
            qInfo() << "[DjiDrc] hsi_frequency" << _hsiFrequency << "Hz requested, capped at 1Hz";
        }
    }

    publishDrcUp(QStringLiteral("hsi_info_push"), buildHsiPush());
    bumpStat(QStringLiteral("hsi_up"));
    publishDrcUp(QStringLiteral("delay_info_push"), buildDelayPush());
    bumpStat(QStringLiteral("delay_up"));

    // 面板上的"下行静默"要每秒动一下，所以状态信号也在这里发
    emit drcStatusChanged();
}

void DjiDrcClient::cancelPendingAuth(const QString& reason)
{
    if (!_authPending) {
        return;
    }

    _authPending = false;
    _authTimer->stop();

    // 与 onAuthTimeout 同一套语义：只有 cloud_control_auth_request 这条流程需要回 notify，
    // 而且必须回 —— 云端在那个 HTTP 调用上同步等结果，不回就是等满超时。
    // 抢控制权那两条（flight/payload_authority_grab）在收到时已经应答过 services_reply。
    if (_consentSourceMethod == QStringLiteral("cloud_control_auth_request")) {
        publishAuthNotify(false);
    }

    qInfo() << "[DjiDrc] pending cloud control request cancelled:" << reason
            << "source:" << _consentSourceMethod;
    _consentSourceMethod.clear();

    // 先让 UI 把弹窗收掉再报状态：收窗走 onClosed → drcRespondAuth(false)，
    // 而那时 _authPending 已经是 false，那边会直接返回，不会覆盖任何东西。
    emit authCancelled();
    emit drcStatusChanged();
}

void DjiDrcClient::onAuthTimeout()
{
    if (!_authPending) {
        return;
    }

    _authPending    = false;
    _consentGranted = false;
    qWarning() << "[DjiDrc] no operator response to cloud control request, treating as denied";

    if (_consentSourceMethod == QStringLiteral("cloud_control_auth_request")) {
        publishAuthNotify(false);
    }
    _consentSourceMethod.clear();
    emit drcStatusChanged();
}

void DjiDrcClient::onEmergencyStopTimeout()
{
    if (!_emergencyStopPending) {
        return;
    }

    _emergencyStopPending = false;
    qWarning() << "[DjiDrc] no operator response to emergency stop, replying refused";

    QJsonObject data;
    data[QStringLiteral("result")] = DjiDrcError::kObtainControlFailed;
    data[QStringLiteral("output")] = QJsonObject();
    publishDrcUp(QStringLiteral("drone_emergency_stop"), data, _emergencyTid, _emergencyBid);
    _emergencyTid.clear();
    _emergencyBid.clear();
    emit drcStatusChanged();
}

// ---------------------------------------------------------------------------
// 上行报文组装
// ---------------------------------------------------------------------------

QJsonObject DjiDrcClient::buildOsdPush() const
{
    QJsonObject data;
    if (!_activeVehicle) {
        return data;
    }

    // 10Hz 的高频路径：只放协议要求的低延时字段，不做电池/固件那套重活
    // （那套在 DjiCloudProperties::buildDroneOsd 里，走主连接的周期上报）。
    const double heading     = _activeVehicle->heading()->rawValue().toDouble();
    const double groundSpeed = _activeVehicle->groundSpeed()->rawValue().toDouble();
    const double climbRate   = _activeVehicle->climbRate()->rawValue().toDouble();

    data[QStringLiteral("attitude_head")] = static_cast<int>(heading);
    data[QStringLiteral("latitude")]      = _activeVehicle->latitude();
    data[QStringLiteral("longitude")]     = _activeVehicle->longitude();
    data[QStringLiteral("altitude")]      = _activeVehicle->altitudeRelative()->rawValue().toDouble();

    // speed_x/y/z 是 NED 三轴速度。QGC 只给水平地速的模长与升降速度，
    // 水平两轴按航向投影拆出来（侧滑和风带来的分量在这里看不出来）。
    const double headingRad = qDegreesToRadians(heading);
    data[QStringLiteral("speed_x")] = groundSpeed * std::cos(headingRad); // 北
    data[QStringLiteral("speed_y")] = groundSpeed * std::sin(headingRad); // 东
    data[QStringLiteral("speed_z")] = -climbRate;                         // NED 向下为正

    double gimbalPitch = 0.0;
    double gimbalRoll  = 0.0;
    double gimbalYaw   = 0.0;
    if (_activeVehicle->gimbalController() && _activeVehicle->gimbalController()->activeGimbal()) {
        Gimbal* gimbal = _activeVehicle->gimbalController()->activeGimbal();
        gimbalPitch = gimbal->absolutePitch()->rawValue().toDouble();
        gimbalRoll  = gimbal->absoluteRoll()->rawValue().toDouble();
        gimbalYaw   = gimbal->absoluteYaw()->rawValue().toDouble();
    }
    data[QStringLiteral("gimbal_pitch")] = gimbalPitch;
    data[QStringLiteral("gimbal_roll")]  = gimbalRoll;
    data[QStringLiteral("gimbal_yaw")]   = gimbalYaw;

    return data;
}

QJsonObject DjiDrcClient::buildHsiPush() const
{
    QJsonObject data;

    // around_distances：72 个扇区、每 5°，0° 是机头正前方、顺时针。MAVLink 的
    // OBSTACLE_DISTANCE 给的是一圈带 increment/angle_offset 的采样，这里重采样到
    // 固定的 72 格。单位：MAVLink 是厘米，协议要毫米。
    QJsonArray around;
    QList<int> rawDistances;
    qreal increment   = 0.0;
    qreal angleOffset = 0.0;
    int   maxDistance = 0;

    if (_activeVehicle && _activeVehicle->objectAvoidance()) {
        VehicleObjectAvoidance* avoidance = _activeVehicle->objectAvoidance();
        rawDistances = avoidance->distances();
        increment    = avoidance->increment();
        angleOffset  = avoidance->angleOffset();
        maxDistance  = avoidance->maxDistance();
    }

    for (int i = 0; i < kHsiSectors; i++) {
        int distanceMm = 0;
        if (!rawDistances.isEmpty() && increment > 0.0) {
            const double sectorAngle = i * (360.0 / kHsiSectors);
            int index = qRound((sectorAngle - angleOffset) / increment);
            const int count = rawDistances.count();
            index = ((index % count) + count) % count; // 环绕到 [0, count)
            const int value = rawDistances.at(index);
            // UINT16_MAX 表示该扇区没有有效数据；超过量程的也当归零
            if (value != UINT16_MAX && (maxDistance <= 0 || value < maxDistance)) {
                distanceMm = value * kCmToMm;
            }
        }
        around.append(distanceMm);
    }
    data[QStringLiteral("around_distances")] = around;

    // 上/下视距离：取 DISTANCE_SENSOR 里对应朝向的那两个探头的值
    // （MAV_SENSOR_ROTATION_PITCH_90 = 上、PITCH_270 = 下），一次都没有就是 0。
    // 注意这两个 fact 的单位是米（VehicleDistanceSensorFactGroup 里已经把消息的
    // 厘米除以 100 了），和 around_distances 那条路的厘米不是一回事。
    double upDistance   = 0.0;
    double downDistance = 0.0;
    if (_activeVehicle && _activeVehicle->distanceSensorFactGroup()) {
        FactGroup* sensors = _activeVehicle->distanceSensorFactGroup();
        if (Fact* up = sensors->getFact(QStringLiteral("rotationPitch90"))) {
            upDistance = up->rawValue().toDouble() * kMToMm;
        }
        if (Fact* down = sensors->getFact(QStringLiteral("rotationPitch270"))) {
            downDistance = down->rawValue().toDouble() * kMToMm;
        }
    }
    data[QStringLiteral("up_distance")]   = upDistance;
    data[QStringLiteral("down_distance")] = downDistance;

    // 各向避障的开关/工作状态 MAVLink 里没有对应字段（那是 DJI 飞控自己的状态量），
    // 统一报 false —— 报未知比报假的"已开启"安全。要接的话从飞控扩展状态里取。
    const QStringList directions = {
        QStringLiteral("up"), QStringLiteral("down"), QStringLiteral("left"), QStringLiteral("right"),
        QStringLiteral("front"), QStringLiteral("back"),
    };
    for (const QString& direction : directions) {
        data[direction + QStringLiteral("_enable")] = false;
        data[direction + QStringLiteral("_work")]   = false;
    }
    // 综合状态由各向推导（协议定义如此）
    data[QStringLiteral("vertical_enable")]    = false;
    data[QStringLiteral("vertical_work")]      = false;
    data[QStringLiteral("horizontal_enable")]  = false;
    data[QStringLiteral("horizontal_work")]    = false;

    return data;
}

QJsonObject DjiDrcClient::buildDelayPush() const
{
    QJsonObject data;

    // sdr_cmd_delay 是飞机侧 SDR 链路的命令延时，本机（PC 直连飞控）没有这个量，
    // 报 0 表示未知 —— 不拿"网络往返"之类的东西凑数，那会让云端看到假数据。
    data[QStringLiteral("sdr_cmd_delay")] = 0;

    // liveview_delay_list 要从直播模块拿每路流的延时，而 QGC 的 VideoReceiver
    // 没有暴露延迟接口，所以这里先给空数组。接上之后按
    // [{video_id, liveview_delay_time}] 填。
    data[QStringLiteral("liveview_delay_list")] = QJsonArray();

    return data;
}

QJsonObject DjiDrcClient::buildDroneStatePush() const
{
    // 远程控制状态（drc_drone_state_push）。本地 docs 里没有这个方法的字段表，
    // 沿用既有实现的三个字段。
    QJsonObject data;
    data[QStringLiteral("stealth_state")]      = false; // QGC 无隐身模式来源
    data[QStringLiteral("night_lights_state")] = false; // QGC 无航行灯来源
    data[QStringLiteral("mode_code")]          = DjiCloudProperties::flightModeCode(_activeVehicle);
    return data;
}

QJsonObject DjiDrcClient::buildCameraOsdPush() const
{
    // 相机 OSD（drc_camera_osd_info_push）。QGC 的相机后端没有这些字段的对应量，
    // 只上报负载索引占位 —— 与既有实现一致，等接了相机 OSD 再补。
    QJsonObject data;
    data[QStringLiteral("payload_index")] = QStringLiteral("66-0-0");
    return data;
}

// ---------------------------------------------------------------------------
// 收尾与上报
// ---------------------------------------------------------------------------

void DjiDrcClient::teardownDrc(const QString& reason, int errorCode, bool notifyJoystickInvalid,
                              bool notifyOperator)
{
    const bool wasActive = _drcActive || _drcState != 0;

    // 弹窗要如实说"断开时控制权在谁手上"，而下面 setCloudFlightAuthority(false) 会把它清掉 ——
    // 所以必须在收尾**之前**读出来。
    const bool heldFlightAuthority = _cloudFlightAuthority;

    // 先归零再断链路：断开之后想发也发不出去了
    _mapper->zeroSticks();
    _sticksZeroed = true;

    // 收尾前的作废也要走收口函数，而且必须赶在断链路之前 ——
    // 它可能还要往云端回一条 cloud_control_auth_notify，断线之后就发不出去了。
    cancelPendingAuth(QStringLiteral("DRC 收尾"));

    _heartbeatTimer->stop();
    _osdTimer->stop();
    _slowTimer->stop();
    _authTimer->stop();
    _emergencyStopTimer->stop();
    _stickTimer->stop();

    if (_drcClient->state() != QMqttClient::Disconnected) {
        _drcClient->disconnectFromHost();
    }

    _drcActive             = false;
    _drcState              = 0;
    _stateSubscribed       = false;
    _lastDownMsgTime       = 0;
    _reconnectAtMs         = 0;
    _heartbeatMisses       = 0;
    _sticksZeroed          = true;
    _lastDroneControlSeq   = -1;
    // 走收口函数而不是直接赋值：它顺带解锁本地杆量与停保活流。
    // 漏掉解锁的话，退出 DRC 之后本地摇杆再也不起作用（而用户看到的界面一切正常）。
    setCloudFlightAuthority(false);
    _cloudPayloadAuthority = false;
    _consentGranted        = false;
    // _authPending 由上面的 cancelPendingAuth 清（那里还要回 notify、发 authCancelled）
    _emergencyStopPending  = false;
    clearCloudStick();

    if (notifyJoystickInvalid && wasActive) {
        publishJoystickInvalid(kReasonRcLost);
    }

    notifyDrcStatus(errorCode, 0);
    setLastError(reason);
    qInfo() << "[DjiDrc] DRC torn down:" << reason << "errorCode:" << errorCode
            << "heldFlightAuthority:" << heldFlightAuthority;
    emit drcStatusChanged();

    // 放在最后：弹窗读的是收尾**之后**的状态（本地摇杆已解锁、控制权已回到本机），
    // 早发的话操作员看到的就是还没落定的中间态。wasActive 挡住"本来就没会话"的空转。
    if (notifyOperator && wasActive) {
        emit remoteControlLost(reason, heldFlightAuthority);
    }
}

void DjiDrcClient::notifyDrcStatus(int errorCode, int drcState)
{
    // DrcStatusNotify：{result: DrcStatusErrorEnum, drc_state: 0未连接/1连接中/2已连接}
    QJsonObject data;
    data[QStringLiteral("result")]    = errorCode;
    data[QStringLiteral("drc_state")] = drcState;
    _drcState = drcState;
    emit eventToPublish(QStringLiteral("drc_status_notify"), 1, data);
    emit drcStatusChanged();
}

void DjiDrcClient::publishJoystickInvalid(int reason)
{
    // JoystickInvalidNotify：{reason: JoystickInvalidReasonEnum}
    QJsonObject data;
    data[QStringLiteral("reason")] = reason;
    emit eventToPublish(QStringLiteral("joystick_invalid_notify"), 1, data);
}

void DjiDrcClient::publishAuthNotify(bool accept)
{
    // 云端按 output.status 判断：ok / failed / canceled。
    // 拒绝用 canceled（"人不同意"），failed 留给"想给但给不了"。
    QJsonObject output;
    output[QStringLiteral("status")] = accept ? QStringLiteral("ok") : QStringLiteral("canceled");

    QJsonObject data;
    data[QStringLiteral("result")] = DjiDrcError::kSuccess;
    data[QStringLiteral("output")] = output;
    emit eventToPublish(QStringLiteral("cloud_control_auth_notify"), 1, data);
}

void DjiDrcClient::setLastError(const QString& text)
{
    if (_lastErrorText == text) {
        return;
    }
    _lastErrorText = text;
    emit drcStatusChanged();
}

void DjiDrcClient::bumpStat(const QString& key, int delta)
{
    _pushStats[key] = _pushStats.value(key).toInt() + delta;
}

// ---------------------------------------------------------------------------
// 云端杆量的镜像（飞行视图上的只读摇杆）
// ---------------------------------------------------------------------------

void DjiDrcClient::setCloudStick(double x, double y, double h, double w, qint64 seq,
                                 const DjiDrcControlMapper::Axes& axes)
{
    _cloudStick[QStringLiteral("x")]        = x;
    _cloudStick[QStringLiteral("y")]        = y;
    _cloudStick[QStringLiteral("h")]        = h;
    _cloudStick[QStringLiteral("w")]        = w;
    _cloudStick[QStringLiteral("seq")]      = seq;
    _cloudStick[QStringLiteral("has_data")] = true;
    // 映射后的轴值：QML 画摇杆用这组（thrust 是 [0,1]，0.5 = 悬停）
    _cloudStick[QStringLiteral("roll")]     = axes.roll;
    _cloudStick[QStringLiteral("pitch")]    = axes.pitch;
    _cloudStick[QStringLiteral("yaw")]      = axes.yaw;
    _cloudStick[QStringLiteral("thrust")]   = axes.thrust;
    emit cloudStickChanged();
}

void DjiDrcClient::clearCloudStick()
{
    // 中立位直接用 Axes 的默认值：**thrust 默认是 0.5 不是 0**
    // （0 会被飞控读成全速下降，见 DjiDrcControlMapper 的 kThrottleNeutral）。
    // 从默认值取而不是再写一遍字面量，是为了将来改零点时不会漏掉这里。
    const DjiDrcControlMapper::Axes neutral;

    _cloudStick[QStringLiteral("x")]        = 0.0;
    _cloudStick[QStringLiteral("y")]        = 0.0;
    _cloudStick[QStringLiteral("h")]        = 0.0;
    _cloudStick[QStringLiteral("w")]        = 0.0;
    _cloudStick[QStringLiteral("seq")]      = -1;
    _cloudStick[QStringLiteral("has_data")] = false;
    _cloudStick[QStringLiteral("roll")]     = neutral.roll;
    _cloudStick[QStringLiteral("pitch")]    = neutral.pitch;
    _cloudStick[QStringLiteral("yaw")]      = neutral.yaw;
    _cloudStick[QStringLiteral("thrust")]   = neutral.thrust;
    emit cloudStickChanged();
}

int DjiDrcClient::downSilenceMs() const
{
    if (_lastDownMsgTime <= 0) {
        return 0;
    }
    return static_cast<int>(QDateTime::currentMSecsSinceEpoch() - _lastDownMsgTime);
}

// ---------------------------------------------------------------------------
// drc/up 发布
// ---------------------------------------------------------------------------

void DjiDrcClient::publishDrcUp(const QString& method, const QJsonObject& data,
                                const QString& tid, const QString& bid)
{
    if (_drcClient->state() != QMqttClient::Connected) {
        return;
    }

    // 信封见 DjiDrcClient.h 的说明：{tid, bid, timestamp, method, data}
    QJsonObject msg;
    msg[QStringLiteral("tid")]       = tid.isEmpty()
                                           ? QUuid::createUuid().toString(QUuid::WithoutBraces)
                                           : tid;
    msg[QStringLiteral("bid")]       = bid.isEmpty()
                                           ? QUuid::createUuid().toString(QUuid::WithoutBraces)
                                           : bid;
    msg[QStringLiteral("timestamp")] = QDateTime::currentMSecsSinceEpoch();
    msg[QStringLiteral("method")]    = method;
    msg[QStringLiteral("data")]      = data;

    const QString gcsSn = SettingsManager::instance()->cloudServerSettings()->gcsSn()->rawValueString();
    _drcClient->publish(QMqttTopicName(QStringLiteral("thing/product/") + gcsSn + QStringLiteral("/drc/up")),
                        QJsonDocument(msg).toJson(QJsonDocument::Compact));
}
