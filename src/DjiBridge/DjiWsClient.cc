/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

/// @file
/// @brief DJI 上云 API WebSocket（ws 组件）客户端实现

#include "DjiWsClient.h"

#include <QtCore/QDateTime>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QtMath>
#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>

#include "CloudServerSettings.h"
#include "SettingsManager.h"


namespace {

QString nowText()
{
    return QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
}

/// 按数量上限裁剪模型首部条目；QmlObjectListModel::clear() 不删对象，这里必须 removeAt + deleteLater
void trimModel(QmlObjectListModel* model, int maxCount)
{
    while (model->count() > maxCount) {
        QObject* item = model->removeAt(0);
        if (item) {
            item->deleteLater();
        }
    }
}

/// 从 QJsonValue 里取一个可以当文本用的标量（字符串原样，数字转文本）
QString valueText(const QJsonValue& value)
{
    if (value.isString()) {
        return value.toString();
    }
    if (value.isDouble()) {
        return QString::number(value.toDouble());
    }
    return QString();
}

}  // namespace

// ---------------------------------------------------------------------------
// 设备条目
// ---------------------------------------------------------------------------

DjiCloudDeviceInfo::DjiCloudDeviceInfo(const QString& sn, QObject* parent)
    : QObject(parent)
    , _sn(sn)
{
}

void DjiCloudDeviceInfo::setLocal(bool local)
{
    if (_isLocal == local) {
        return;
    }
    _isLocal = local;
    emit isLocalChanged();
}

void DjiCloudDeviceInfo::setOnline(bool online)
{
    if (_online == online) {
        return;
    }
    _online = online;
    emit onlineChanged();
}

void DjiCloudDeviceInfo::setInfo(const QVariantMap& info)
{
    if (_info == info) {
        return;
    }
    _info = info;
    emit infoChanged();
}

void DjiCloudDeviceInfo::setOsd(const QVariantMap& osd)
{
    if (_osd == osd) {
        return;
    }
    _osd = osd;
    emit osdChanged();
}

void DjiCloudDeviceInfo::setLastSeen(const QString& lastSeen)
{
    if (_lastSeen == lastSeen) {
        return;
    }
    _lastSeen = lastSeen;
    emit lastSeenChanged();
}

// ---------------------------------------------------------------------------
// 告警 / 进度 / 消息条目
// ---------------------------------------------------------------------------

DjiCloudHmsInfo::DjiCloudHmsInfo(const QString& sn,
                                 const QString& hmsId,
                                 int level,
                                 const QString& code,
                                 const QString& text,
                                 QObject* parent)
    : QObject(parent)
    , _sn(sn)
    , _hmsId(hmsId)
    , _time(nowText())
    , _level(level)
    , _code(code)
    , _text(text)
{
}

void DjiCloudHmsInfo::update(int level, const QString& text)
{
    if ((_level == level) && (_text == text)) {
        return;
    }
    _level = level;
    _text = text;
    emit changed();
}

DjiCloudProgressInfo::DjiCloudProgressInfo(const QString& kind, const QString& title, QObject* parent)
    : QObject(parent)
    , _kind(kind)
    , _title(title)
    , _time(nowText())
{
}

void DjiCloudProgressInfo::setPercent(int percent)
{
    const int clamped = qBound(0, percent, 100);
    if (_percent == clamped) {
        return;
    }
    _percent = clamped;
    emit percentChanged();
}

void DjiCloudProgressInfo::setStatusText(const QString& statusText)
{
    if (_statusText == statusText) {
        return;
    }
    _statusText = statusText;
    emit statusTextChanged();
}

DjiCloudMessageInfo::DjiCloudMessageInfo(const QString& bizCode, const QString& summary, QObject* parent)
    : QObject(parent)
    , _bizCode(bizCode)
    , _time(nowText())
    , _summary(summary)
{
}

// ---------------------------------------------------------------------------
// DjiWsClient：连接管理
// ---------------------------------------------------------------------------

DjiWsClient::DjiWsClient(QObject* parent)
    : QObject(parent)
    , _socket(new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this))
    , _reconnectTimer(new QTimer(this))
    , _osdFlushTimer(new QTimer(this))
    , _devices(new QmlObjectListModel(this))
    , _hms(new QmlObjectListModel(this))
    , _progress(new QmlObjectListModel(this))
    , _messages(new QmlObjectListModel(this))
{
    _reconnectTimer->setSingleShot(true);
    connect(_reconnectTimer, &QTimer::timeout, this, &DjiWsClient::onReconnectTimeout);

    _osdFlushTimer->setSingleShot(true);
    _osdFlushTimer->setTimerType(Qt::CoarseTimer);
    connect(_osdFlushTimer, &QTimer::timeout, this, &DjiWsClient::onOsdFlushTimeout);

    _onlineSweepTimer = new QTimer(this);
    _onlineSweepTimer->setTimerType(Qt::CoarseTimer);
    _onlineSweepTimer->setInterval(OnlineSweepIntervalMs);
    connect(_onlineSweepTimer, &QTimer::timeout, this, &DjiWsClient::onOnlineSweepTimeout);
    _onlineSweepTimer->start();

    connect(_socket, &QWebSocket::connected, this, &DjiWsClient::onSocketConnected);
    connect(_socket, &QWebSocket::disconnected, this, &DjiWsClient::onSocketDisconnected);
    connect(_socket, &QWebSocket::errorOccurred, this, &DjiWsClient::onSocketError);
    connect(_socket, &QWebSocket::textMessageReceived, this, &DjiWsClient::onTextMessageReceived);
}

DjiWsClient::~DjiWsClient()
{
    _shuttingDown = true;
    _reconnectTimer->stop();
    _osdFlushTimer->stop();
    _socket->abort();
}

bool DjiWsClient::connected() const
{
    return _socket->state() == QAbstractSocket::ConnectedState;
}

int DjiWsClient::deviceCount() const
{
    return _devices->count();
}

int DjiWsClient::onlineCount() const
{
    int count = 0;
    for (const QObject* obj : *_devices->objectList()) {
        const auto* device = qobject_cast<const DjiCloudDeviceInfo*>(obj);
        if (device && device->online()) {
            ++count;
        }
    }
    return count;
}

int DjiWsClient::hmsCount() const
{
    return _hms->count();
}

void DjiWsClient::connectToWebSocket(const QString& url, const QString& token, const QString& callback)
{
    if (url.isEmpty()) {
        qWarning() << "组件参数缺少 host，跳过连接";
        return;
    }
    if (!callback.isEmpty()) {
        _callback = callback;
    }

    const bool targetChanged = (_url != url) || (_token != token);
    _url  = url;
    _token = token;
    _manualDisconnect = false;

    if (!targetChanged && _socket->state() != QAbstractSocket::UnconnectedState) {
        return;  // 同一地址重复加载组件：保持现有连接
    }

    _intentionalClose = (_socket->state() != QAbstractSocket::UnconnectedState);
    _socket->abort();
    _reconnectAttempts = 0;
    emit urlChanged();
    _openSocket();
}

void DjiWsClient::connectNative()
{
    CloudServerSettings* settings = SettingsManager::instance()->cloudServerSettings();
    const QString url = settings->websocketUrl()->rawValueString();
    const QString token = settings->serverToken()->rawValueString();

    // 空 token 会被后端握手直接拒掉，重连只会变成无限空转
    if (token.isEmpty()) {
        qWarning() << "原生直连缺少 serverToken，跳过；WebSocket 地址：" << url;
        return;
    }
    connectToWebSocket(url, token, QString());
}

void DjiWsClient::disconnectNow()
{
    _manualDisconnect = true;
    _intentionalClose = true;
    _reconnectTimer->stop();
    _reconnectAttempts = 0;
    if (_socket->state() != QAbstractSocket::UnconnectedState) {
        _socket->close();
    }
}

void DjiWsClient::setConnectCallback(const QString& callback)
{
    _callback = callback;
}

void DjiWsClient::sendText(const QString& message)
{
    if (!connected()) {
        qWarning() << "未连接，丢弃待发送消息，长度：" << message.length();
        return;
    }
    _socket->sendTextMessage(message);
}

void DjiWsClient::_openSocket()
{
    if (_url.isEmpty() || _socket->state() != QAbstractSocket::UnconnectedState) {
        return;
    }

    QUrl url(_url);
    if (!_token.isEmpty()) {
        QUrlQuery query(url);
        query.addQueryItem(QStringLiteral("x-auth-token"), _token);  // JWT 含 +/=，必须由 QUrlQuery 编码
        url.setQuery(query);
    }

    qInfo() << "连接中：" << url.toString(QUrl::RemoveQuery);
    _intentionalClose = false;
    _socket->open(url);
}

void DjiWsClient::_scheduleReconnect()
{
    if (_shuttingDown || _manualDisconnect || _intentionalClose || _url.isEmpty()) {
        return;
    }
    if (_reconnectAttempts >= ReconnectMaxAttempts) {
        qWarning() << "重连次数已达上限，停止重连：" << _url;
        return;
    }

    ++_reconnectAttempts;
    const double delay = ReconnectInitialDelayMs * qPow(1.3, _reconnectAttempts - 1);
    const int timeout = qMin(ReconnectMaxDelayMs, static_cast<int>(delay));
    qInfo() << "第" << _reconnectAttempts << "次重连将在" << timeout << "ms 后开始";
    _reconnectTimer->start(timeout);
}

void DjiWsClient::_notifyCallback(bool connectedState)
{
    if (_shuttingDown || _callback.isEmpty()) {
        return;
    }
    emit jsCallbackRequested(_callback, QJsonValue(connectedState));
}

// ---------------------------------------------------------------------------
// DjiWsClient：socket 事件
// ---------------------------------------------------------------------------

void DjiWsClient::onSocketConnected()
{
    _reconnectAttempts = 0;
    _reconnectTimer->stop();
    qInfo() << "已连接：" << _url;
    emit connectedChanged();
    _notifyCallback(true);
}

void DjiWsClient::onSocketDisconnected()
{
    qInfo() << "连接断开，code:" << _socket->closeCode()
                           << "reason:" << _socket->closeReason();
    emit connectedChanged();
    _notifyCallback(false);
    _scheduleReconnect();
}

void DjiWsClient::onSocketError(QAbstractSocket::SocketError error)
{
    qWarning() << "socket 错误:" << error << _socket->errorString();
}

void DjiWsClient::onReconnectTimeout()
{
    _openSocket();
}

void DjiWsClient::onTextMessageReceived(const QString& message)
{
    const QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isObject()) {
        qWarning() << "收到非 JSON 消息，长度：" << message.length();
        return;
    }
    _handleEnvelope(doc.object());
}

// ---------------------------------------------------------------------------
// DjiWsClient：消息分发
// ---------------------------------------------------------------------------

void DjiWsClient::_handleEnvelope(const QJsonObject& envelope)
{
    const QString bizCode = envelope.value(QStringLiteral("biz_code")).toString();
    if (bizCode.isEmpty()) {
        return;
    }
    const QJsonObject data = envelope.value(QStringLiteral("data")).toObject();

    if (bizCode == QLatin1String("device_online")) {
        _applyDeviceOnline(data);
    } else if (bizCode == QLatin1String("device_offline")) {
        _applyDeviceOffline(data);
    } else if (bizCode == QLatin1String("device_update_topo")) {
        _applyDeviceTopo(data);
    } else if (bizCode == QLatin1String("device_hms")) {
        _applyHms(data);
    } else if (bizCode == QLatin1String("device_osd") ||
               bizCode == QLatin1String("gateway_osd") ||
               bizCode == QLatin1String("dock_osd")) {
        _queueOsd(data);  // 高频遥测：合流刷新，且不写消息日志
        return;
    } else if (bizCode == QLatin1String("flighttask_progress") ||
               bizCode == QLatin1String("file_upload_callback") ||
               bizCode == QLatin1String("fileupload_progress")) {
        _applyProgress(bizCode, data);
    } else if (bizCode == QLatin1String("map_element_create") ||
               bizCode == QLatin1String("map_element_update") ||
               bizCode == QLatin1String("map_element_delete") ||
               bizCode == QLatin1String("map_group_refresh")) {
        // 地图元素：推送频率低，转发给 DjiCloudMapClient 画到地图上。
        // 不 return —— 继续走 _appendMessage，消息日志里也留一行方便排查。
        emit mapElementMessage(bizCode, data);
    } else if (bizCode == QLatin1String("flight_areas_update")) {
        // 飞行区域（任务区域 / GEO 区域）变动。只读图层，客户端收到后整表重拉。
        emit flightAreaMessage(bizCode, data);
    }

    _appendMessage(bizCode, data);
}

DjiCloudDeviceInfo* DjiWsClient::_deviceFor(const QString& sn, bool createIfMissing)
{
    if (sn.isEmpty()) {
        return nullptr;
    }
    for (QObject* obj : *_devices->objectList()) {
        auto* device = qobject_cast<DjiCloudDeviceInfo*>(obj);
        if (device && device->sn() == sn) {
            return device;
        }
    }
    if (!createIfMissing) {
        return nullptr;
    }
    auto* device = new DjiCloudDeviceInfo(sn, _devices);
    _devices->append(device);
    return device;
}

void DjiWsClient::_noteBackendEvidence(const QString& sn)
{
    if (sn.isEmpty()) {
        return;
    }
    DjiCloudDeviceInfo* device = _deviceFor(sn, false);
    if (!device) {
        return;
    }

    // 时间戳要走在置位前面：掉线扫描按它判超时，先置位会让扫描头一次就命中旧时间戳
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    _lastEvidenceMs.insert(sn, now);

    const bool wasOnline = device->online();
    device->setOnline(true);
    device->setLastSeen(nowText());
    if (!wasOnline) {
        // 只对本机设备打点：别人的飞机/机场的 OSD 与我们无关，逐条打会把日志刷爆。
        // 这条是"抽屉里那台本机地面站为什么会变绿"的唯一直接证据，别删。
        if (device->isLocal()) {
            qInfo() << "[DjiCloud] 收到后台对本机设备的回推，判为在线：" << device->sn();
        }
        _refreshCounts();
    }
}

void DjiWsClient::_applyDeviceOnline(const QJsonObject& data)
{
    DjiCloudDeviceInfo* device = _deviceFor(data.value(QStringLiteral("sn")).toString(), true);
    if (!device) {
        return;
    }

    // 只覆盖推送里非空的字段，避免后续推送缺字段时把已知信息擦掉
    QVariantMap info = device->info();
    const QString model = data.value(QStringLiteral("device_model")).toString(
                              data.value(QStringLiteral("model")).toString());
    const QString callsign = data.value(QStringLiteral("device_callsign")).toString(
                                 data.value(QStringLiteral("user_callsign")).toString());
    const QString gatewaySn = data.value(QStringLiteral("gateway_sn")).toString();
    if (!model.isEmpty()) {
        info[QStringLiteral("model")] = model;
    }
    if (!callsign.isEmpty()) {
        info[QStringLiteral("callsign")] = callsign;
    }
    if (!gatewaySn.isEmpty()) {
        info[QStringLiteral("gatewaySn")] = gatewaySn;
    }
    device->setInfo(info);

    if (data.value(QStringLiteral("online_status")).toBool(true)) {
        _noteBackendEvidence(device->sn());
    } else {
        device->setOnline(false);
        device->setLastSeen(nowText());
        _refreshCounts();
    }
}

void DjiWsClient::_applyDeviceOffline(const QJsonObject& data)
{
    DjiCloudDeviceInfo* device = _deviceFor(data.value(QStringLiteral("sn")).toString(), false);
    if (!device) {
        return;
    }
    // 后台明确说它掉线了：这条比任何"还在推"的证据都新，把证据一起清掉，
    // 否则它会在宽限窗口内被下一条扫描又判断成在线
    _lastEvidenceMs.remove(device->sn());
    device->setOnline(false);
    device->setLastSeen(nowText());
    _refreshCounts();
}

void DjiWsClient::_applyDeviceTopo(const QJsonObject& data)
{
    // 拓扑更新：带上 online_status 就当成上下线处理，其余信息更新在 _appendMessage 里留痕
    if (!data.contains(QStringLiteral("online_status"))) {
        return;
    }
    if (data.value(QStringLiteral("online_status")).toBool()) {
        _applyDeviceOnline(data);
    } else {
        _applyDeviceOffline(data);
    }
}

void DjiWsClient::_applyHms(const QJsonObject& data)
{
    const QString sn = data.value(QStringLiteral("sn")).toString();
    const QJsonArray list = data.value(QStringLiteral("host")).toArray();

    for (const QJsonValue& value : list) {
        const QJsonObject hms = value.toObject();
        const QString code = hms.value(QStringLiteral("key")).toString();
        const QString text = hms.value(QStringLiteral("message_zh")).toString(
                                 hms.value(QStringLiteral("message_en")).toString(code));
        const int level = hms.value(QStringLiteral("level")).toInt();
        // hms_id 是平台对同一条告警的稳定标识；缺失时退回 key
        const QString hmsId = hms.value(QStringLiteral("hms_id")).toString(code);

        // 平台每个 OSD 周期都会把「当前活动告警集」整体重推一遍，
        // 直接 append 会让列表和角标几分钟内被同一条告警刷满，所以按 (sn, hms_id) 合并。
        DjiCloudHmsInfo* existing = nullptr;
        for (QObject* obj : *_hms->objectList()) {
            auto* info = qobject_cast<DjiCloudHmsInfo*>(obj);
            if (info && (info->sn() == sn) && (info->hmsId() == hmsId)) {
                existing = info;
                break;
            }
        }
        if (existing) {
            existing->update(level, text);
            continue;
        }
        _hms->append(new DjiCloudHmsInfo(sn, hmsId, level, code, text, _hms));
    }

    trimModel(_hms, MaxHms);
    _refreshCounts();
}

void DjiWsClient::_queueOsd(const QJsonObject& data)
{
    const QString sn = data.value(QStringLiteral("sn")).toString();
    if (sn.isEmpty()) {
        return;
    }
    _pendingOsd.insert(sn, data.value(QStringLiteral("host")).toObject());
    _dirtyOsdSn.insert(sn);
    if (!_osdFlushTimer->isActive()) {
        _osdFlushTimer->start(OsdFlushIntervalMs);
    }
}

void DjiWsClient::onOsdFlushTimeout()
{
    const QSet<QString> dirty = _dirtyOsdSn;
    _dirtyOsdSn.clear();

    for (const QString& sn : dirty) {
        const QJsonObject host = _pendingOsd.take(sn);
        if (host.isEmpty()) {
            continue;
        }
        // OSD 意味着设备正在上报，未知 SN 也补进设备表
        DjiCloudDeviceInfo* device = _deviceFor(sn, true);
        if (!device) {
            continue;
        }

        QVariantMap osd;
        const QJsonObject battery = host.value(QStringLiteral("battery")).toObject();
        // 无人机 osd 的电量在 battery.capacity_percent，遥控器 osd 在顶层 capacity_percent
        const QJsonValue capacity = battery.value(QStringLiteral("capacity_percent")).isUndefined()
                                        ? host.value(QStringLiteral("capacity_percent"))
                                        : battery.value(QStringLiteral("capacity_percent"));
        if (capacity.isDouble()) {
            osd[QStringLiteral("batteryPercent")] = capacity.toInt();
        }
        if (host.contains(QStringLiteral("height"))) {
            osd[QStringLiteral("height")] = host.value(QStringLiteral("height")).toDouble();
        }
        if (host.contains(QStringLiteral("latitude"))) {
            osd[QStringLiteral("latitude")] = host.value(QStringLiteral("latitude")).toDouble();
        }
        if (host.contains(QStringLiteral("longitude"))) {
            osd[QStringLiteral("longitude")] = host.value(QStringLiteral("longitude")).toDouble();
        }
        if (host.contains(QStringLiteral("mode_code"))) {
            osd[QStringLiteral("modeCode")] = host.value(QStringLiteral("mode_code")).toInt();
        }
        device->setOsd(osd);

        // **OSD 到达 = 后台此刻认它在线的证据。**
        // 后台收到我们(或任何设备)的 osd 后，是先 deviceRedisService.setDeviceOnline(...)
        // 再 pushOsdDataToPilot(...) 推给 PiLot 侧的（SDKDeviceService.osdRemoteControl /
        // osdRcDrone），所以这条推送能到我们手里，就说明后台刚把它的在线键续过期过。
        // 反过来，device_online 那条通知在"飞机挂在遥控器下"的稳态路径里后台根本不会发
        // （updateTopoOnline 的推送被重复订阅异常挡掉、deviceOnlineAgain 只打日志不推），
        // 所以本机地面站/飞机那颗点只能靠这里点亮 —— 之前只写 osd 字段不置在线，
        // 表现就是"后台明明一直推着地面站的 osd，抽屉里地面站却是灰的"。
        _noteBackendEvidence(sn);
    }
}

void DjiWsClient::_applyProgress(const QString& bizCode, const QJsonObject& data)
{
    // flighttask_progress 的 data 是 MQTT 事件信封（EventsReceiver），真正的进度在 data.data；
    // 其它进度类消息可能直接推送进度对象，这里两种都兼容。
    const QJsonObject inner = data.value(QStringLiteral("data")).toObject();
    const QJsonObject task = inner.isEmpty() ? data : inner;
    const QJsonObject progress = task.value(QStringLiteral("progress")).toObject();
    const QJsonObject ext = task.value(QStringLiteral("ext")).toObject();

    const bool isTask = (bizCode == QLatin1String("flighttask_progress"));
    const QString kind = isTask ? QStringLiteral("task") : QStringLiteral("upload");

    QString title = isTask ? ext.value(QStringLiteral("wayline_id")).toString() : QString();
    if (title.isEmpty()) {
        title = data.value(QStringLiteral("bid")).toString();
    }
    if (title.isEmpty()) {
        title = task.value(QStringLiteral("wayline_id")).toString();
    }
    if (title.isEmpty()) {
        title = data.value(QStringLiteral("sn")).toString();
    }
    if (title.isEmpty()) {
        title = bizCode;
    }

    // 同一条任务/文件的进度会被反复推送，按 title 复用已存在的条目，避免列表无限增长
    DjiCloudProgressInfo* entry = nullptr;
    for (QObject* obj : *_progress->objectList()) {
        auto* candidate = qobject_cast<DjiCloudProgressInfo*>(obj);
        if (candidate && candidate->title() == title) {
            entry = candidate;
            break;
        }
    }

    if (!entry) {
        entry = new DjiCloudProgressInfo(kind, title, _progress);
        _progress->append(entry);
        trimModel(_progress, MaxProgress);
    }

    if (progress.contains(QStringLiteral("percent"))) {
        entry->setPercent(progress.value(QStringLiteral("percent")).toInt());
    } else if (task.contains(QStringLiteral("percent"))) {
        entry->setPercent(task.value(QStringLiteral("percent")).toInt());
    } else if (task.contains(QStringLiteral("uploaded_count")) &&
               task.contains(QStringLiteral("media_count"))) {
        const int total = task.value(QStringLiteral("media_count")).toInt();
        if (total > 0) {
            entry->setPercent(task.value(QStringLiteral("uploaded_count")).toInt() * 100 / total);
        }
    }

    const QString status = task.value(QStringLiteral("status")).toString(
                               data.value(QStringLiteral("status")).toString());
    entry->setStatusText(status);
}

void DjiWsClient::_appendMessage(const QString& bizCode, const QJsonObject& data)
{
    _messages->append(new DjiCloudMessageInfo(bizCode, _summarize(bizCode, data), _messages));
    trimModel(_messages, MaxMessages);
}

QString DjiWsClient::_summarize(const QString& bizCode, const QJsonObject& data)
{
    const QString sn = data.value(QStringLiteral("sn")).toString();
    QStringList parts;
    parts << bizCode;
    if (!sn.isEmpty()) {
        parts << sn;
    }

    if (bizCode == QLatin1String("device_online") || bizCode == QLatin1String("device_update_topo")) {
        const QString model = data.value(QStringLiteral("device_model")).toString(
                                  data.value(QStringLiteral("model")).toString());
        if (!model.isEmpty()) {
            parts << model;
        }
        parts << (data.value(QStringLiteral("online_status")).toBool(true) ? QStringLiteral("online")
                                                                          : QStringLiteral("offline"));
    } else if (bizCode == QLatin1String("device_hms")) {
        parts << QStringLiteral("x%1").arg(data.value(QStringLiteral("host")).toArray().size());
    } else if (bizCode == QLatin1String("flighttask_progress")) {
        const QJsonObject task = data.value(QStringLiteral("data")).toObject();
        const QString status = task.value(QStringLiteral("status")).toString();
        const int percent = task.value(QStringLiteral("progress")).toObject()
                                .value(QStringLiteral("percent")).toInt();
        if (!status.isEmpty()) {
            parts << status;
        }
        parts << QStringLiteral("%1%").arg(percent);
    } else {
        const QString detail = valueText(data.value(QStringLiteral("bid")));
        if (!detail.isEmpty()) {
            parts << detail;
        }
    }

    return parts.join(QLatin1Char(' '));
}

void DjiWsClient::_refreshCounts()
{
    emit countsChanged();
}

void DjiWsClient::clearHms()
{
    _hms->clearAndDeleteContents();
    _refreshCounts();
}

void DjiWsClient::noteGatewayAck()
{
    const QString gcsSn = SettingsManager::instance()->cloudServerSettings()->gcsSn()->rawValueString();
    if (gcsSn.isEmpty()) {
        return;
    }
    // 后台回了 status_reply(result=0)，说明它已经收下并处理了我们这条 update_topo ——
    // 也就是 Redis 里 online:<gcsSn> 刚被续过期。和 osd 回推是同一种证据，走同一条路。
    _deviceFor(gcsSn, true);
    _noteBackendEvidence(gcsSn);
}

void DjiWsClient::onOnlineSweepTimeout()
{
    // 只扫**本机**设备（gcsSn/droneSn）。别的设备（别人的机场、别人的飞机）后台只往
    // WEB 侧推 RC_OSD/dock_osd，我们收不到它们的持续证据，拿这个窗口扫它们会扫出一堆假掉线。
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    bool changed = false;

    for (QObject* obj : *_devices->objectList()) {
        auto* device = qobject_cast<DjiCloudDeviceInfo*>(obj);
        if (!device || !device->isLocal() || !device->online()) {
            continue;
        }
        const qint64 last = _lastEvidenceMs.value(device->sn(), 0);
        if (last > 0 && (now - last) < BackendEvidenceTimeoutMs) {
            continue;
        }
        device->setOnline(false);
        _lastEvidenceMs.remove(device->sn());
        changed = true;
        qInfo() << "[DjiCloud] 超过" << (BackendEvidenceTimeoutMs / 1000)
                << "秒没收到后台对该设备的任何回推，判为离线：" << device->sn();
    }

    if (changed) {
        _refreshCounts();
    }
}

void DjiWsClient::seedLocalDevices()
{
    CloudServerSettings* settings = SettingsManager::instance()->cloudServerSettings();
    struct LocalSeed {
        QString sn;
        QString callsign;
    };
    const QList<LocalSeed> seeds = {
        { settings->gcsSn()->rawValueString(),   QStringLiteral("本机地面站") },
        { settings->droneSn()->rawValueString(), QStringLiteral("本机飞机")   },
    };

    for (const LocalSeed& seed : seeds) {
        if (seed.sn.isEmpty()) {
            continue;
        }
        DjiCloudDeviceInfo* device = _deviceFor(seed.sn, true);
        if (!device) {
            continue;
        }
        // 本机地面站/飞机始终带 Local 标记，平台上云后也不会丢
        device->setLocal(true);
        QVariantMap info = device->info();
        if (!info.contains(QStringLiteral("callsign"))) {
            info[QStringLiteral("callsign")] = seed.callsign;
            device->setInfo(info);
        }
    }
    _refreshCounts();
}
