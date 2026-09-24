/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "DjiWaylineManager.h"
#include "DjiWaylineListModel.h"

#include "SettingsManager.h"
#include "AppSettings.h"
#include "CloudServerSettings.h"

#include "planconvert/planconverter.h"
#include "planconvert/qgcplanparser.h"
#include "planconvert/djiwpmlparser.h"
#include "planconvert/djiwpmlwriter.h"

#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QMessageAuthenticationCode>
#include <QtCore/QUrlQuery>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>
#include <QtPositioning/QGeoCoordinate>

// 日志用默认类别（qInfo/qWarning），与本目录其他文件一致：
// QGCLogging::msgHandler 会把没有开启 debug 的自定义类别整条丢掉，
// 用 QGC_LOGGING_CATEGORY 建类别反而默认一条日志都看不到。

namespace {

// ---------------------------------------------------------------------------
// 后台接口路径
//
// 已对过真实后台的 /v3/api-docs（OpenAPI），下面这些路径全部存在且方法一致。
// 参数名一律以那边为准 —— 有几处和 DJI 官方文档记忆里的写法是反的，见注释。
// ---------------------------------------------------------------------------
const QString kPathWaylines       = QStringLiteral("/wayline/api/v1/workspaces/%1/waylines");
const QString kPathWaylineById    = QStringLiteral("/wayline/api/v1/workspaces/%1/waylines/%2");
const QString kPathWaylineUrl     = QStringLiteral("/wayline/api/v1/workspaces/%1/waylines/%2/url");
const QString kPathDuplicateNames = QStringLiteral("/wayline/api/v1/workspaces/%1/waylines/duplicate-names");
const QString kPathFavorites      = QStringLiteral("/wayline/api/v1/workspaces/%1/favorites");
const QString kPathUploadCallback = QStringLiteral("/wayline/api/v1/workspaces/%1/upload-callback");
const QString kPathSts            = QStringLiteral("/storage/api/v1/workspaces/%1/sts");

// 后台另有一个 POST /waylines/file/upload?file=<objectKey>，是让后台代传对象存储的
// 路子。我们走的是「取 STS → 自己 PUT 到桶 → upload-callback 登记」，用不到它。

// DJI 云后台 API 端口（media / storage / wayline 同一个服务）
constexpr int kServerPort = 6789;

// 列表默认排序列。
//
// 后台的排序参数是 order_by（必填，只认 name / update_time / create_time，
// 其它值一律当成没传）+ 可选的 order_by.desc。少发它、或者发个后台不认的值，
// 都会回「orderBy不能为null」，整个列表拉不出来（实测）。所以调用方给空串时
// 这里兜一个默认值，而不是干脆不发。
//
// 注意 /v3/api-docs 里这个参数写的是 orderBy.column —— 那是 springdoc 按
// GetWaylineListOrderBy 这个嵌套对象拍平出来的名字，照着它发反而会报
// "Invalid property 'orderBy' ... Could not instantiate ... to auto-grow"。
// 真实参数名以实测为准：order_by。
const QString kDefaultOrderBy = QStringLiteral("update_time");

// ---------------------------------------------------------------------------
// M3E 机型枚举
//
// 转换器默认写的是 67/52，那是 M30/M30T 的一套，用错后台会按「机型不符」拒收。
// 这里显式覆盖成 M3E：机巢/飞机 77，子型号 0，负载 66。
//
// **注意 77 是中间的 type 段，不是整个 key。** 云端那些 model_key 字符串是三段
// 式的 domain-type-subType（/v3/api-docs 里 DeviceEnum 的 format 就写着
// "domain-type-subType"，取值形如 0-77-0 / 1-66-0）。无人机 domain=0、
// 负载 domain=1。曾经照 "%1-%2-%3".arg(droneEnumValue) 拼成 "77-0-0"，
// 三位都在但排错了位，后台回的是 Jackson 那句
// "JSON parse error: ... not one of the values accepted for Enum class" ——
// 看着像 JSON 格式问题，其实是枚举取值不合法。
// ---------------------------------------------------------------------------
constexpr int kDroneDomain          = 0;    // 无人机那一类的 domain
constexpr int kPayloadDomain        = 1;    // 负载那一类的 domain
constexpr int kM3eDroneEnumValue    = 77;
constexpr int kM3eDroneSubEnumValue = 0;
constexpr int kM3ePayloadEnumValue  = 66;

wpt::DjiWriteOptions m3eOptions()
{
    wpt::DjiWriteOptions options;
    options.droneEnumValue = kM3eDroneEnumValue;
    options.droneSubEnumValue = kM3eDroneSubEnumValue;
    options.payloadEnumValue = kM3ePayloadEnumValue;
    options.templateType = 0;   // 0 = 航点飞行，本转换器只输出这一种
    return options;
}

// ---------------------------------------------------------------------------

QString workspaceId()
{
    return SettingsManager::instance()->cloudServerSettings()->workSpaceId()->rawValueString();
}

QString serverIp()
{
    return SettingsManager::instance()->cloudServerSettings()->serverIp()->rawValueString();
}

/// 与 MediaManager 一致：x-auth-token 走设置里的 serverToken
QNetworkRequest makeRequest(const QString& path)
{
    QNetworkRequest request;
    request.setRawHeader("x-auth-token",
                         SettingsManager::instance()->cloudServerSettings()->serverToken()->rawValueString().toUtf8());
    request.setUrl(QUrl(QStringLiteral("http://") + serverIp() + QStringLiteral(":") + QString::number(kServerPort) + path));
    return request;
}

QByteArray hmacSha256(const QByteArray& key, const QByteArray& data)
{
    QMessageAuthenticationCode mac(QCryptographicHash::Sha256, key);
    mac.addData(data);
    return mac.result();
}

QString iso8601Now()
{
    return QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd'T'HHmmss'Z'"));
}

/// S3 V4 签名（单次 PUT，无 query 参数）。
/// 与 MediaManager::s3V4Sign 同源，但那边要先签 query 再分块上传，这里用不上。
///
/// 第二个参数是**完整的 URL 路径**（`/bucket/key`，含前导斜杠），不是裸的对象 key。
/// 它必须和真正发出去的路径逐字节一致 —— 少那个前导斜杠 MinIO 就回 403
/// SignatureDoesNotMatch。所以调用方一律传 `url.path()`，别自己拼。
QMap<QString, QString> s3V4Sign(const QString& method, const QString& canonicalPath,
                                const QByteArray& body,
                                const QMap<QString, QString>& headers,
                                const QString& region, const QString& accessKey,
                                const QString& secretKey)
{
    const QString date = headers["X-Amz-Date"].left(8);
    const QString isoTime = headers["X-Amz-Date"];
    const QString service = QStringLiteral("s3");

    // 规范请求：方法 / 编码路径 / 空 query / 规范头 / 签名头列表 / 载荷哈希
    QString canonicalRequest;
    canonicalRequest += method + "\n";
    // 中文文件名要按 UTF-8 逐字节百分号编码，'/' 保留不编。前导斜杠由调用方保证。
    canonicalRequest += QUrl::toPercentEncoding(canonicalPath, "/_.!~*'()") + "\n";
    canonicalRequest += "\n";

    const QByteArray payloadHash = QCryptographicHash::hash(body, QCryptographicHash::Sha256);

    QMap<QString, QString> sortedHeaders;
    for (auto it = headers.cbegin(); it != headers.cend(); ++it) {
        sortedHeaders.insert(it.key().toLower(), it.value().trimmed());
    }
    QString signedHeadersStr;
    for (auto it = sortedHeaders.cbegin(); it != sortedHeaders.cend(); ++it) {
        canonicalRequest += it.key() + ":" + it.value() + "\n";
        signedHeadersStr += (signedHeadersStr.isEmpty() ? QString() : QStringLiteral(";")) + it.key();
    }
    canonicalRequest += "\n";
    canonicalRequest += signedHeadersStr + "\n";
    canonicalRequest += payloadHash.toHex();

    // 待签名字符串
    QString stringToSign;
    stringToSign += QStringLiteral("AWS4-HMAC-SHA256\n");
    stringToSign += isoTime + "\n";
    stringToSign += date + "/" + region + "/" + service + "/aws4_request\n";
    stringToSign += QCryptographicHash::hash(canonicalRequest.toUtf8(), QCryptographicHash::Sha256).toHex();

    // 派生签名密钥
    const QByteArray dateKey = hmacSha256(("AWS4" + secretKey).toUtf8(), date.toUtf8());
    const QByteArray regionKey = hmacSha256(dateKey, region.toUtf8());
    const QByteArray serviceKey = hmacSha256(regionKey, service.toUtf8());
    const QByteArray signingKey = hmacSha256(serviceKey, "aws4_request");
    const QByteArray signature = hmacSha256(signingKey, stringToSign.toUtf8());

    QMap<QString, QString> out;
    for (auto it = headers.cbegin(); it != headers.cend(); ++it) {
        out.insert(it.key(), it.value());
    }
    out["Authorization"] = QStringLiteral("AWS4-HMAC-SHA256 Credential=%1/%2/%3/%4/aws4_request, "
                                          "SignedHeaders=%5, Signature=%6")
                               .arg(accessKey, date, region, service, signedHeadersStr, QString::fromLatin1(signature.toHex()));
    out["x-amz-content-sha256"] = QString::fromLatin1(payloadHash.toHex());
    return out;
}

QString joinMessages(const QVector<QString>& messages)
{
    QStringList list;
    list.reserve(messages.size());
    for (const QString& message : messages) {
        list.append(message);
    }
    return list.join(QStringLiteral("；"));
}

} // namespace

// ---------------------------------------------------------------------------

DjiWaylineManager::DjiWaylineManager(QObject* parent)
    : QObject(parent)
    , _networkManager(new QNetworkAccessManager(this))
    , _listModel(new DjiWaylineListModel(this))
{
}

DjiWaylineManager::~DjiWaylineManager() = default;

QObject* DjiWaylineManager::listModel() const
{
    return _listModel;
}

QString DjiWaylineManager::workspaceId() const
{
    // 直接读设置，不走匿名命名空间里那个同名辅助函数 —— 那是个无参自由函数，
    // 在成员函数里按名字调用会解析到本成员，变成无限递归
    return SettingsManager::instance()->cloudServerSettings()->workSpaceId()->rawValueString();
}

QString DjiWaylineManager::serverAddress() const
{
    return serverIp() + QStringLiteral(":") + QString::number(kServerPort);
}

// ---------------------------------------------------------------------------
// 列表
// ---------------------------------------------------------------------------

void DjiWaylineManager::refreshList(bool favoritedOnly)
{
    // 复用上一次的筛选条件，只切收藏过滤，然后回到第一页
    refreshListEx(1, _pageSize, _keyword, _droneModelKey, _payloadModelKey,
                  _templateType, favoritedOnly, _orderBy, _orderDesc);
}

void DjiWaylineManager::refreshListEx(int page, int pageSize, const QString& keyword,
                                      const QString& droneModelKey, const QString& payloadModelKey,
                                      int templateType, bool favoritedOnly,
                                      const QString& orderBy, bool orderDesc)
{
    if (page < 1) {
        page = 1;
    }
    if (pageSize < 1) {
        pageSize = _pageSize;
    }

    // 记住筛选条件：翻页、删除、收藏之后重拉时还要用
    _keyword = keyword;
    _droneModelKey = droneModelKey;
    _payloadModelKey = payloadModelKey;
    _templateType = templateType;
    _favoritedOnly = favoritedOnly;
    _orderBy = orderBy;
    _orderDesc = orderDesc;
    _pageSize = pageSize;

    // 参数名以 /v3/api-docs 为准（order_by 除外，那个按实测，见上面）。注意
    // 无人机和负载这两个键的单复数是反的：请求里是 drone_model_keys 复数、
    // payload_model_key 单数，响应里正好相反。别顺手"改正"，改了后台就收不到。
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("page"), QString::number(page));
    query.addQueryItem(QStringLiteral("page_size"), QString::number(pageSize));
    if (!keyword.isEmpty()) {
        query.addQueryItem(QStringLiteral("key"), keyword);
    }
    if (!droneModelKey.isEmpty()) {
        query.addQueryItem(QStringLiteral("drone_model_keys"), droneModelKey);
    }
    if (!payloadModelKey.isEmpty()) {
        query.addQueryItem(QStringLiteral("payload_model_key"), payloadModelKey);
    }
    if (templateType >= 0) {
        query.addQueryItem(QStringLiteral("template_type"), QString::number(templateType));
    }
    if (favoritedOnly) {
        query.addQueryItem(QStringLiteral("favorited"), QStringLiteral("true"));
    }
    query.addQueryItem(QStringLiteral("order_by"),
                       orderBy.isEmpty() ? kDefaultOrderBy : orderBy);
    query.addQueryItem(QStringLiteral("order_by.desc"),
                       orderDesc ? QStringLiteral("true") : QStringLiteral("false"));

    const QString path = kPathWaylines.arg(workspaceId())
                       + QStringLiteral("?") + query.toString(QUrl::FullyEncoded);

    _setBusy(true);
    _get(path, [this, page, pageSize](QNetworkReply* reply) { _applyListReply(reply, page, pageSize); });
}

void DjiWaylineManager::_applyListReply(QNetworkReply* reply, int requestedPage, int requestedPageSize)
{
    reply->deleteLater();
    _setBusy(false);

    const QByteArray body = reply->readAll();
    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (reply->error() != QNetworkReply::NoError || httpStatus >= 400) {
        _fail(QStringLiteral("获取航线列表失败（HTTP %1：%2）").arg(httpStatus).arg(reply->errorString()));
        return;
    }

    const QJsonObject root = QJsonDocument::fromJson(body).object();
    if (root["code"].toInt() != 0) {
        _fail(QStringLiteral("获取航线列表失败：") + root["message"].toString());
        return;
    }

    const QJsonObject payload = root["data"].toObject();
    const QJsonArray list = payload["list"].toArray();
    _listModel->setList(list);

    const QJsonObject pagination = payload["pagination"].toObject();
    _totalCount = pagination["total"].toInt(list.size());
    _currentPage = pagination["page"].toInt(requestedPage);
    _pageSize = pagination["page_size"].toInt(requestedPageSize);
    if (_pageSize < 1) {
        _pageSize = 20;
    }
    _totalPages = (_totalCount + _pageSize - 1) / _pageSize;
    if (_totalPages < 1) {
        _totalPages = 1;
    }

    qInfo() << "航线列表已刷新：共" << _totalCount << "条，第" << _currentPage << "/" << _totalPages << "页";
    emit listRefreshed(_totalCount, _currentPage);
}

// ---------------------------------------------------------------------------
// 下载
// ---------------------------------------------------------------------------

void DjiWaylineManager::downloadWayline(int row)
{
    const QString id = _listModel->idAt(row);
    const QString name = _listModel->nameAt(row);
    if (id.isEmpty()) {
        // 注意 _noteDownloadOutcome 要在 _fail 之前调：_fail 里可能已经把这批的
        // 账结清了（最后一条失败就发 batchDownloadFinished），顺序反了会漏掉这一笔
        _noteDownloadOutcome(false);
        _fail(QStringLiteral("下载失败：行 %1 没有对应的航线 id").arg(row));
        return;
    }

    _setBusy(true);
    emit transferProgress(QStringLiteral("download"), 0);

    // 先取下载地址（后台是 302 重定向，地址在 Location 头里，见 _getRedirectLocation），
    // 再拉文件
    _getRedirectLocation(kPathWaylineUrl.arg(workspaceId(), id),
                         [this, name](const QString& url, const QString& addressError) {
        if (url.isEmpty()) {
            _setBusy(false);
            _noteDownloadOutcome(false);
            _fail(QStringLiteral("获取下载地址失败：") + addressError);
            return;
        }

        const QString dest = downloadDir() + QStringLiteral("/") + name + QStringLiteral(".kmz");
        _downloadToFile(QUrl(url), dest, [this](const QString& path, const QString& error) {
            _setBusy(false);
            if (path.isEmpty()) {
                _noteDownloadOutcome(false);
                _fail(error);
                return;
            }
            emit transferProgress(QStringLiteral("download"), 100);
            qInfo() << "航线下载完成：" << path;
            _noteDownloadOutcome(true);
            emit downloadFinished(path);
        });
    });
}

void DjiWaylineManager::downloadWaylines(const QVariantList& rows)
{
    if (rows.isEmpty()) {
        return;
    }

    // 每条独立下载、并发进行，进度各自推各自的 100%。
    // 整批的完成情况由 _noteDownloadOutcome 在这里数（见头文件里 batchDownloadFinished
    // 的说明：不能让 QML 侧按 errorOccurred 自己数，那个信号是全局的）
    QStringList ids;
    for (const QVariant& row : rows) {
        const QString id = _listModel->idAt(row.toInt());
        if (!id.isEmpty()) {
            ids.append(id);
        }
    }
    if (ids.isEmpty()) {
        _fail(QStringLiteral("批量下载失败：没有有效的行号"));
        return;
    }

    qInfo() << "开始批量下载" << ids.size() << "条航线";

    // 先把账本支起来再发请求：downloadWayline 对没有 id 的行是**同步** _fail 的，
    // 账本后支的话那几笔就记不上了，整批永远收不了尾
    _batchDownloadActive = true;
    _batchDownloadTotal  = rows.size();
    _batchDownloadLanded = 0;
    _batchDownloadFailed = 0;
    for (const QVariant& row : rows) {
        downloadWayline(row.toInt());
    }
}

void DjiWaylineManager::_noteDownloadOutcome(bool landed)
{
    if (!_batchDownloadActive) {
        return;
    }

    if (landed) {
        ++_batchDownloadLanded;
    } else {
        ++_batchDownloadFailed;
    }

    if (_batchDownloadLanded + _batchDownloadFailed >= _batchDownloadTotal) {
        _batchDownloadActive = false;
        qInfo() << "批量下载结束：成功" << _batchDownloadLanded << "条，失败" << _batchDownloadFailed << "条";
        emit batchDownloadFinished(_batchDownloadLanded, _batchDownloadFailed);
    }
}

// ---------------------------------------------------------------------------
// 单条航线预览（取 kmz + 解析）
// ---------------------------------------------------------------------------

void DjiWaylineManager::requestWaylinePreview(int row)
{
    const QString id = _listModel->idAt(row);
    if (id.isEmpty()) {
        // 到不了这儿：列表里每条都有后台给的 id。真漏了也没法发请求 ——
        // 都不知道回的是哪一条，直接在 QML 侧判空，这里只留个日志
        qWarning() << "航线预览：行" << row << "没有对应的航线 id";
        return;
    }

    // 解析过的（包括失败过的）直接回缓存：点来点去看的就是同一条，
    // 而失败的那条反复重试只会白打后台
    const auto cached = _previewCache.constFind(id);
    if (cached != _previewCache.constEnd()) {
        emit waylinePreviewReady(id, cached.value());
        return;
    }

    if (_previewInFlight.contains(id)) {
        return;
    }
    _previewInFlight.insert(id);
    _setPreviewBusy(true);

    const QString dest = _previewDir() + QStringLiteral("/") + id + QStringLiteral(".kmz");

    _getRedirectLocation(kPathWaylineUrl.arg(workspaceId(), id),
                         [this, id, dest](const QString& url, const QString& addressError) {
        if (url.isEmpty()) {
            _finishPreview(id, QString(), QStringLiteral("获取航线文件地址失败：") + addressError);
            return;
        }
        _downloadToFile(QUrl(url), dest, [this, id, dest](const QString& path, const QString& error) {
            if (path.isEmpty()) {
                // 落到一半的残留（_downloadToFile 是截断后整块写的），扫掉
                QFile::remove(dest);
                _finishPreview(id, QString(), error);
                return;
            }
            _finishPreview(id, path, QString());
        });
    });
}

QString DjiWaylineManager::_previewDir() const
{
    const QString base = SettingsManager::instance()->appSettings()->missionSavePath();
    const QString dir = base + QStringLiteral("/DjiWaylineCache");

    QDir().mkpath(dir);
    return dir;
}

QString DjiWaylineManager::_parseWaylinePreview(const QString& kmzPath, QVariantMap& info) const
{
    wpt::DjiWpmlParser parser;
    wpt::Plan plan;
    wpt::Report report;
    if (!parser.parseFile(kmzPath, plan, report)) {
        return report.errors.isEmpty() ? QStringLiteral("解析航线文件失败") : joinMessages(report.errors);
    }

    // 拍平的 [lat0, lon0, lat1, lon1, ...]。
    // **不要**写成 [[lat,lon],...] 的嵌套表：嵌套的 QVariantList 过 QML 边界时会被摊平
    // （实测 5 个点过去成了长度 10 的数数组），QML 那边 pts[i][0] 全是 undefined，
    // 结果航线画不出来、取景也拿不到坐标。拍平的形态没有任何歧义
    QVariantList points;
    QGeoCoordinate previous;
    double length = 0.0;
    double minHeight = 0.0;
    double maxHeight = 0.0;
    double minLat = 0.0, maxLat = 0.0, minLon = 0.0, maxLon = 0.0;
    double startLat = 0.0, startLon = 0.0;
    bool hasStart = false;
    bool hasHeight = false;
    int count = 0;

    // 起飞参考点补成航迹的第 1 个点。
    //
    // DJI 的航线里**没有**「起飞」这一项：起飞点单独放在 template.kml 的
    // takeOffRefPoint 里（"经度,纬度,椭球高"）。而本地 QGC 任务的第 1 项就是起飞点
    // （NAV_TAKEOFF），只画航点的话这条预览就**比本地航线少一个点，本地的第 2 个点
    // 变成了第 1 个** —— 用户报的就是它。上传时 takeOffRefPoint 取自 .plan 的
    // plannedHomePosition，而 QGC 里它就是起飞点那一项，两者本来就是同一个坐标。
    //
    // 只动预览画面：不写回任何文件，上传的 kmz、飞机实际飞的航点都不受影响。
    // 它也不进高度量程（min/maxExecuteHeight）—— 这里给的是椭球高（本机几条任务是
    // 29m），混进执行高度（5m）的量程里会把量程整个带偏
    {
        const QStringList parts = plan.mission.takeOffRefPoint.split(QLatin1Char(','));
        bool okLon = false;
        bool okLat = false;
        const double lon = parts.size() > 0 ? parts.at(0).trimmed().toDouble(&okLon) : 0.0;
        const double lat = parts.size() > 1 ? parts.at(1).trimmed().toDouble(&okLat) : 0.0;
        if (okLon && okLat && !(lat == 0.0 && lon == 0.0)) {
            hasStart = true;
            startLat = lat;
            startLon = lon;
            minLat   = maxLat = lat;
            minLon   = maxLon = lon;
            previous = QGeoCoordinate(lat, lon);
            points.append(lat);
            points.append(lon);
            ++count;
        }
    }

    // 多条航线全并进来。DJI 的 kmz 绝大多数只有一条，真有第二条也不该只画一半
    for (const wpt::Wayline& wayline : plan.waylines) {
        for (const wpt::Waypoint& wp : wayline.waypoints) {
            // (0,0) 是解析失败留下的空点，带上它航线会一路画到几内亚湾
            if (wp.lat == 0.0 && wp.lon == 0.0) {
                continue;
            }

            const QGeoCoordinate coord(wp.lat, wp.lon);
            if (hasStart) {
                length += previous.distanceTo(coord);
                minLat = qMin(minLat, wp.lat);
                maxLat = qMax(maxLat, wp.lat);
                minLon = qMin(minLon, wp.lon);
                maxLon = qMax(maxLon, wp.lon);
            } else {
                hasStart = true;
                startLat = wp.lat;
                startLon = wp.lon;
                minLat = maxLat = wp.lat;
                minLon = maxLon = wp.lon;
            }
            previous = coord;

            if (hasHeight) {
                minHeight = qMin(minHeight, wp.executeHeight);
                maxHeight = qMax(maxHeight, wp.executeHeight);
            } else {
                hasHeight = true;
                minHeight = maxHeight = wp.executeHeight;
            }

            points.append(wp.lat);
            points.append(wp.lon);
            ++count;
        }
    }

    if (count == 0) {
        return QStringLiteral("这条航线里没有可用的航点");
    }

    info[QStringLiteral("waypointCount")]    = count;
    info[QStringLiteral("lengthMeters")]     = length;
    info[QStringLiteral("startLatitude")]    = startLat;
    info[QStringLiteral("startLongitude")]   = startLon;
    info[QStringLiteral("minExecuteHeight")] = minHeight;
    info[QStringLiteral("maxExecuteHeight")] = maxHeight;
    info[QStringLiteral("points")]           = points;
    info[QStringLiteral("bounds")]           = QVariantList{minLat, minLon, maxLat, maxLon};
    return QString();
}

void DjiWaylineManager::_finishPreview(const QString& id, const QString& kmzPath, const QString& error)
{
    QVariantMap info;
    info[QStringLiteral("id")] = id;

    QString failure = error;
    if (failure.isEmpty() && !kmzPath.isEmpty()) {
        failure = _parseWaylinePreview(kmzPath, info);
    }

    if (failure.isEmpty()) {
        info[QStringLiteral("ok")] = true;
    } else {
        info[QStringLiteral("ok")]    = false;
        info[QStringLiteral("error")] = failure;
        qWarning() << "航线预览失败：" << id << failure;
    }

    // 预览用的 kmz 只是中间产物，解析完就扫掉 —— 留在盘上只会越积越多
    // （结果本身已经进 _previewCache 了，不会因为删文件而要重下）
    if (!kmzPath.isEmpty()) {
        QFile::remove(kmzPath);
    }

    _previewInFlight.remove(id);
    _previewCache.insert(id, info);
    _setPreviewBusy(false);
    emit waylinePreviewReady(id, info);
}

// ---------------------------------------------------------------------------
// 收藏 / 取消收藏
// ---------------------------------------------------------------------------

void DjiWaylineManager::collectWaylines(const QStringList& ids, bool favorite)
{
    _collectWaylines(ids, favorite, nullptr);
}

void DjiWaylineManager::_collectWaylines(const QStringList& ids, bool favorite,
                                         std::function<void()> onDone)
{
    if (ids.isEmpty()) {
        if (onDone) {
            onDone();
        }
        return;
    }

    // 后台的收藏/取消收藏是 /favorites?workspace_id=..&id=a&id=b，id 是重复的
    // query 参数（数组），不是 JSON body —— 路由里 workspace_id 已在路径上，
    // 所以这里只拼 id。
    QUrlQuery query;
    for (const QString& id : ids) {
        query.addQueryItem(QStringLiteral("id"), id);
    }

    // 先乐观更新本地列表，失败再改回来
    for (int row = 0; row < _listModel->rowCount(); ++row) {
        if (ids.contains(_listModel->idAt(row))) {
            _listModel->setFavorited(row, favorite);
        }
    }

    const QString action = favorite ? QStringLiteral("收藏") : QStringLiteral("取消收藏");

    _setBusy(true);
    _send(favorite ? QByteArrayLiteral("POST") : QByteArrayLiteral("DELETE"),
          kPathFavorites.arg(workspaceId()) + QStringLiteral("?") + query.toString(QUrl::FullyEncoded),
          QByteArray(),
          QString(),
          [this, ids, favorite, action, onDone](QNetworkReply* reply) {
              reply->deleteLater();
              _setBusy(false);

              const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
              if (reply->error() != QNetworkReply::NoError || root["code"].toInt() != 0) {
                  // 回滚乐观更新
                  for (int row = 0; row < _listModel->rowCount(); ++row) {
                      if (ids.contains(_listModel->idAt(row))) {
                          _listModel->setFavorited(row, !favorite);
                      }
                  }
                  _fail(action + QStringLiteral("失败：") + root["message"].toString(reply->errorString()));
                  if (onDone) {
                      onDone();
                  }
                  return;
              }
              qInfo() << action << "成功：" << ids.size() << "条";
              if (onDone) {
                  onDone();
              }
          });
}

// ---------------------------------------------------------------------------
// 查重名
// ---------------------------------------------------------------------------

void DjiWaylineManager::checkDuplicateNames(const QString& name)
{
    // 后台是 GET .../waylines/duplicate-names?name=xxx，name 是数组型 query 参数
    // （单个也走这个位置），响应里 data 直接就是重名数组，没有再套一层对象。
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("name"), name);

    _setBusy(true);
    _get(kPathDuplicateNames.arg(workspaceId()) + QStringLiteral("?") + query.toString(QUrl::FullyEncoded),
         [this](QNetworkReply* reply) {
             reply->deleteLater();
             _setBusy(false);

             const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
             if (reply->error() != QNetworkReply::NoError || root["code"].toInt() != 0) {
                 _fail(QStringLiteral("重名检查失败：") + root["message"].toString(reply->errorString()));
                 return;
             }

             QStringList duplicates;
             const QJsonArray names = root["data"].toArray();
             for (const QJsonValue& value : names) {
                 duplicates.append(value.toString());
             }
             qInfo() << "重名检查：" << duplicates.size() << "个同名航线";
             emit duplicateNamesChecked(duplicates);
         });
}

// ---------------------------------------------------------------------------
// 上传
// ---------------------------------------------------------------------------

void DjiWaylineManager::uploadWayline(const QString& name, const QString& finalName)
{
    const QString src = _resolveLocalKmz(name);
    if (src.isEmpty()) {
        _fail(QStringLiteral("找不到要上传的航线文件：%1").arg(name));
        return;
    }

    const QString uploadName = _normalizeName(finalName.isEmpty() ? name : finalName);
    if (uploadName.isEmpty()) {
        _fail(QStringLiteral("上传失败：航线名不能为空"));
        return;
    }

    _setBusy(true);
    _uploadLocalFile(src, uploadName, [this, uploadName](bool ok, const QString& error) {
        _setBusy(false);
        if (!ok) {
            _fail(error);
            return;
        }
        qInfo() << "航线上传成功：" << uploadName;
        emit uploadFinished(uploadName);
    });
}

void DjiWaylineManager::_uploadLocalFile(const QString& srcPath, const QString& uploadName,
                                         std::function<void(bool, const QString&)> onDone)
{
    QFile file(srcPath);
    if (!file.open(QIODevice::ReadOnly)) {
        onDone(false, QStringLiteral("无法读取 %1：%2").arg(srcPath, file.errorString()));
        return;
    }
    const QByteArray bytes = file.readAll();
    file.close();

    emit transferProgress(QStringLiteral("upload"), 0);

    // 1. 取临时凭证
    _requestCredential([this, bytes, uploadName, onDone](bool ok) {
        if (!ok) {
            onDone(false, _lastError);
            return;
        }

        // 2. 推到对象存储
        const QString objectKey = _credential.objectKeyPrefix.isEmpty()
                                      ? (uploadName + QStringLiteral(".kmz"))
                                      : (_credential.objectKeyPrefix + QStringLiteral("/") + uploadName + QStringLiteral(".kmz"));

        emit transferProgress(QStringLiteral("upload"), 30);
        _putObject(objectKey, bytes, [this, objectKey, uploadName, onDone](bool putOk, const QString& putError) {
            if (!putOk) {
                onDone(false, putError);
                return;
            }

            // 3. 登记航线
            emit transferProgress(QStringLiteral("upload"), 70);
            _importWayline(objectKey, uploadName, [this, onDone](bool importOk, const QString& importError) {
                if (importOk) {
                    emit transferProgress(QStringLiteral("upload"), 100);
                }
                onDone(importOk, importError);
            });
        });
    });
}

void DjiWaylineManager::_requestCredential(std::function<void(bool)> onDone)
{
    _send(QByteArrayLiteral("POST"), kPathSts.arg(workspaceId()), QByteArray(),
          QStringLiteral("application/json; charset=utf-8"),
          [this, onDone](QNetworkReply* reply) {
              reply->deleteLater();

              const QByteArray bodyData = reply->readAll();
              const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
              if (reply->error() != QNetworkReply::NoError || httpStatus >= 400) {
                  _fail(QStringLiteral("获取上传凭证失败（HTTP %1：%2）").arg(httpStatus).arg(reply->errorString()));
                  onDone(false);
                  return;
              }

              const QJsonObject root = QJsonDocument::fromJson(bodyData).object();
              if (root["code"].toInt() != 0) {
                  _fail(QStringLiteral("获取上传凭证失败：") + root["message"].toString());
                  onDone(false);
                  return;
              }

              const QJsonObject data = root["data"].toObject();
              _credential.bucket = data["bucket"].toString();
              _credential.region = data["region"].toString();
              _credential.endpoint = data["endpoint"].toString();
              _credential.objectKeyPrefix = data["object_key_prefix"].toString();

              const QJsonObject credentials = data["credentials"].toObject();
              _credential.accessKey = credentials["access_key_id"].toString();
              _credential.secretKey = credentials["access_key_secret"].toString();
              _credential.sessionToken = credentials["security_token"].toString();

              // endpoint 里带 path 的先丢掉，只留 scheme://host:port
              const QUrl endpointUrl(_credential.endpoint);
              _minioBaseUrl = endpointUrl.scheme() + QStringLiteral("://") + endpointUrl.authority();

              if (_credential.accessKey.isEmpty() || _credential.bucket.isEmpty() || _minioBaseUrl.isEmpty()) {
                  _fail(QStringLiteral("获取上传凭证失败：凭证信息不完整"));
                  onDone(false);
                  return;
              }

              qInfo() << "上传凭证获取成功，bucket：" << _credential.bucket;
              onDone(true);
          });
}

void DjiWaylineManager::_putObject(const QString& objectKey, const QByteArray& data,
                                   std::function<void(bool, const QString&)> onDone)
{
    const QString isoTime = iso8601Now();
    const QString region = _credential.region.isEmpty() ? QStringLiteral("us-east-1") : _credential.region;

    // 参与签名的头集合与 MediaManager 保持一致（那边已对同一后台跑通）：
    // Host / X-Amz-Security-Token / X-Amz-Date / Content-Type / Content-Length 参与签名，
    // Authorization 和 x-amz-content-sha256 由 s3V4Sign 在签完之后追加。
    QString token = _credential.sessionToken.trimmed();
    QMap<QString, QString> headers;
    headers["Host"] = QUrl(_minioBaseUrl).authority();
    headers["X-Amz-Date"] = isoTime;
    headers["Content-Type"] = QStringLiteral("application/octet-stream");
    headers["Content-Length"] = QString::number(data.size());
    if (!token.isEmpty()) {
        headers["X-Amz-Security-Token"] = token;
    }

    // objectKey 是"前缀/文件名"（如 wayline/xxx.kmz），bucket 不在里面，这里补上。
    // 拼出来的 URL 形如 http://host:9000/cloud-bucket/wayline/xxx.kmz。
    const QUrl putUrl(_minioBaseUrl + QStringLiteral("/") + _credential.bucket
                      + QStringLiteral("/") + objectKey);

    // 取 QUrl::path() 而不是自己拼字符串：签名的规范 URI 必须和真正发出去的路径
    // 逐字节一致，**包括那个前导斜杠**。少一个斜杠 MinIO 一律回 403
    // SignatureDoesNotMatch（A/B 实测过：不带 403，带上 200）。MediaManager 也是
    // 用 requestUrl.path()，照它来。中文文件名由 QUrl 负责按 UTF-8 百分号编码。
    const QMap<QString, QString> signedHeaders = s3V4Sign(QStringLiteral("PUT"), putUrl.path(), data, headers,
                                                          region, _credential.accessKey, _credential.secretKey);

    QNetworkRequest request(putUrl);
    for (auto it = signedHeaders.cbegin(); it != signedHeaders.cend(); ++it) {
        request.setRawHeader(it.key().toUtf8(), it.value().toUtf8());
    }

    QNetworkReply* reply = _networkManager->put(request, data);
    connect(reply, &QNetworkReply::uploadProgress, this, [this](qint64 sent, qint64 total) {
        if (total > 0) {
            // 30~70 段留给对象存储上传，取的是上传流程里的区间
            emit transferProgress(QStringLiteral("upload"), 30 + static_cast<int>(sent * 40 / total));
        }
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, onDone]() {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();

        if (reply->error() != QNetworkReply::NoError || status >= 400) {
            onDone(false, QStringLiteral("上传到对象存储失败（HTTP %1：%2）%3")
                              .arg(status).arg(reply->errorString(), QString::fromUtf8(body.left(200))));
            return;
        }
        onDone(true, QString());
    });
}

void DjiWaylineManager::_importWayline(const QString& objectKey, const QString& name,
                                       std::function<void(bool, const QString&)> onDone)
{
    // 后台没有 POST /waylines，对象存好之后要靠 upload-callback 登记，
    // body 里的 metadata 是必填的，缺一个后台直接拒。
    //
    // body 的字段名是 snake_case（实测：发 object_key 能过，发 objectKey 会被
    // 当成没传）。但后台的报错信息用的是 Java 字段名，会说"objectKey不能为null"，
    // 别照着报错里的拼法去改 JSON。
    //
    // 这里的机型/负载/模板是按本文件顶部那套 M3E 常量写的 —— 和转换器
    // 生成 KMZ 时用的是同一组值，所以自己转出来的文件一定对得上。
    // 用户直接丢进来的外来 KMZ 就未必了：那种文件本该按它自己的机型报，
    // 但 KMZ 里的机型字段我们没解析，这里一律按 M3E 报。
    // model_key 是三段式 domain-type-subType，别漏掉第一段 domain，
    // 详见文件上面 kDroneDomain 那段注释
    QJsonObject metadata;
    metadata.insert(QStringLiteral("drone_model_key"),
                    QStringLiteral("%1-%2-%3").arg(kDroneDomain)
                                               .arg(kM3eDroneEnumValue)
                                               .arg(kM3eDroneSubEnumValue));
    metadata.insert(QStringLiteral("payload_model_keys"),
                    QJsonArray{ QStringLiteral("%1-%2-0").arg(kPayloadDomain)
                                                         .arg(kM3ePayloadEnumValue) });
    metadata.insert(QStringLiteral("template_types"), QJsonArray{ 0 });

    QJsonObject body;
    body.insert(QStringLiteral("name"), name);
    body.insert(QStringLiteral("object_key"), objectKey);
    body.insert(QStringLiteral("metadata"), metadata);

    _send(QByteArrayLiteral("POST"), kPathUploadCallback.arg(workspaceId()),
          QJsonDocument(body).toJson(QJsonDocument::Compact),
          QStringLiteral("application/json; charset=utf-8"),
          [this, onDone](QNetworkReply* reply) {
              reply->deleteLater();
              const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
              if (reply->error() != QNetworkReply::NoError || root["code"].toInt() != 0) {
                  onDone(false, QStringLiteral("航线登记失败：") + root["message"].toString(reply->errorString()));
                  return;
              }
              onDone(true, QString());
          });
}

// ---------------------------------------------------------------------------
// 删除 / 重命名
// ---------------------------------------------------------------------------

void DjiWaylineManager::deleteWayline(int row)
{
    const QString id = _listModel->idAt(row);
    const QString name = _listModel->nameAt(row);
    if (id.isEmpty()) {
        _fail(QStringLiteral("删除失败：行 %1 没有对应的航线 id").arg(row));
        return;
    }

    _setBusy(true);
    const QString path = kPathWaylineById.arg(workspaceId(), id);

    _send(QByteArrayLiteral("DELETE"), path, QByteArray(), QString(),
          [this, id, name](QNetworkReply* reply) {
              reply->deleteLater();
              _setBusy(false);

              const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
              const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
              const bool accepted = reply->error() == QNetworkReply::NoError
                                    && httpStatus < 400 && root["code"].toInt() == 0;
              if (accepted) {
                  qInfo() << "航线已删除：" << name;
                  refreshList(_favoritedOnly);
                  return;
              }

              // 后台没实现 DELETE 时走和重命名同一套降级思路：取消收藏，让它从
              // 列表里消失。云端那条记录仍然在，只是不再展示 —— 这是和用户确认过的做法。
              qWarning() << "删除接口不可用，降级为取消收藏：" << name
                         << "（HTTP" << httpStatus << "，" << root["message"].toString() << "）";
              _lastError = QStringLiteral("后台不支持删除，已将「%1」取消收藏").arg(name);
              emit errorOccurred(_lastError);
              collectWaylines({id}, false);
          });
}

void DjiWaylineManager::deleteWaylines(const QVariantList& rows)
{
    // 先把行号翻成 id：后面一条条删的时候不能再碰行号，理由见下面 _deleteByIds
    QStringList ids;
    for (const QVariant& row : rows) {
        const QString id = _listModel->idAt(row.toInt());
        if (!id.isEmpty() && !ids.contains(id)) {
            ids.append(id);
        }
    }
    if (ids.isEmpty()) {
        _fail(QStringLiteral("批量删除失败：没有有效的航线 id"));
        return;
    }

    qInfo() << "开始批量删除" << ids.size() << "条航线";
    _setBusy(true);
    _deleteByIds(ids, 0, QStringList());
}

void DjiWaylineManager::_deleteByIds(const QStringList& ids, int index, const QStringList& failed)
{
    // 一条一条删，全部走完再统一收尾。**中途不能刷新列表**：refreshList 会把行号
    // 全打乱，正在往下走的这个序列就指到别的航线上去了 —— 所以这里从头发到尾
    // 只认 id，一次列表都不拉
    if (index >= ids.size()) {
        _setBusy(false);

        if (failed.isEmpty()) {
            qInfo() << "批量删除完成：" << ids.size() << "条";
            refreshList(_favoritedOnly);
            return;
        }

        // 后台不支持 DELETE（和单条删除同一套降级）。先报一条汇总，再去取消收藏；
        // 等收藏真的落地了才刷列表 —— 抢在它前面刷会把这几行又原样拉回来
        _lastError = QStringLiteral("后台不支持删除，已将 %1 条取消收藏").arg(failed.size());
        emit errorOccurred(_lastError);
        _collectWaylines(failed, false, [this]() { refreshList(_favoritedOnly); });
        return;
    }

    const QString id = ids.at(index);
    const QString path = kPathWaylineById.arg(workspaceId(), id);

    _send(QByteArrayLiteral("DELETE"), path, QByteArray(), QString(),
          [this, ids, index, failed](QNetworkReply* reply) {
              reply->deleteLater();

              const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
              const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
              const bool accepted = reply->error() == QNetworkReply::NoError
                                    && httpStatus < 400 && root["code"].toInt() == 0;

              QStringList nextFailed = failed;
              if (!accepted) {
                  qWarning() << "批量删除：这条删不掉，降级为取消收藏" << ids.at(index)
                             << "（HTTP" << httpStatus << "，" << root["message"].toString() << "）";
                  nextFailed.append(ids.at(index));
              }

              _deleteByIds(ids, index + 1, nextFailed);
          });
}

void DjiWaylineManager::renameWayline(int row, const QString& newName)
{
    const QString id = _listModel->idAt(row);
    const QString oldName = _listModel->nameAt(row);
    const QString uploadName = _normalizeName(newName);

    if (id.isEmpty()) {
        _fail(QStringLiteral("重命名失败：行 %1 没有对应的航线 id").arg(row));
        return;
    }
    if (uploadName.isEmpty()) {
        _fail(QStringLiteral("重命名失败：新名字不能为空"));
        return;
    }
    if (uploadName == oldName) {
        return;
    }

    // 后台不支持直接改名。走用户确认过的降级方案：
    // 下载 -> 用新名字重新上传 -> 把旧的那条取消收藏。
    // 云端会同时留着旧的一条，只是被取消收藏不再出现在常用列表里。
    qInfo() << "重命名走降级流程（下载-改名-重传-旧条目取消收藏）：" << oldName << "->" << uploadName;

    _setBusy(true);
    // 同一个 .../url 接口，同样是 302，见 _getRedirectLocation
    _getRedirectLocation(kPathWaylineUrl.arg(workspaceId(), id),
                         [this, id, uploadName](const QString& url, const QString& addressError) {
        if (url.isEmpty()) {
            _setBusy(false);
            _fail(QStringLiteral("重命名失败：取不到原航线文件地址（") + addressError + QStringLiteral("）"));
            return;
        }

        const QString tmp = downloadDir() + QStringLiteral("/") + uploadName + QStringLiteral(".kmz");
        _downloadToFile(QUrl(url), tmp, [this, id, uploadName](const QString& path, const QString& error) {
            if (path.isEmpty()) {
                _setBusy(false);
                _fail(QStringLiteral("重命名失败：") + error);
                return;
            }

            _uploadLocalFile(path, uploadName, [this, id, uploadName](bool ok, const QString& uploadError) {
                _setBusy(false);
                if (!ok) {
                    _fail(QStringLiteral("重命名失败：") + uploadError);
                    return;
                }

                // 新的一条已经上去了，把旧的取消收藏
                qInfo() << "重命名已上传新航线：" << uploadName << "，现在取消收藏旧条目";
                collectWaylines({id}, false);
                emit uploadFinished(uploadName);
            });
        });
    });
}

// ---------------------------------------------------------------------------
// 转换
// ---------------------------------------------------------------------------

bool DjiWaylineManager::convertPlanToKmz(const QString& planPath, const QString& kmzPath)
{
    _lastConvertError.clear();

    if (!QFileInfo::exists(planPath)) {
        _lastConvertError = QStringLiteral("找不到本地航线文件：%1").arg(planPath);
        return false;
    }

    wpt::Report report;
    if (!wpt::PlanConverter::qgcToDji(planPath, kmzPath, m3eOptions(), report)) {
        _lastConvertError = report.errors.isEmpty() ? QStringLiteral("转换失败") : joinMessages(report.errors);
        qWarning() << "plan -> kmz 失败：" << _lastConvertError;
        return false;
    }

    if (!report.warnings.isEmpty()) {
        qWarning() << "plan -> kmz 告警：" << joinMessages(report.warnings);
    }
    qInfo() << "plan -> kmz 完成：" << kmzPath << "，航点" << report.convertedWaypoints << "个";
    return true;
}

bool DjiWaylineManager::convertPlanToKml(const QString& planPath, const QString& kmlPath)
{
    _lastConvertError.clear();

    if (!QFileInfo::exists(planPath)) {
        _lastConvertError = QStringLiteral("找不到本地航线文件：%1").arg(planPath);
        return false;
    }

    // 只出 template.kml 那一层，给预览用；不打包 kmz
    wpt::QgcPlanParser parser;
    wpt::Plan plan;
    wpt::Report report;
    if (!parser.parseFile(planPath, plan, report)) {
        _lastConvertError = report.errors.isEmpty() ? QStringLiteral("解析 plan 失败") : joinMessages(report.errors);
        return false;
    }

    wpt::DjiWpmlWriter writer;
    const QByteArray xml = writer.buildTemplateXml(plan, m3eOptions(), report);
    if (xml.isEmpty()) {
        _lastConvertError = report.errors.isEmpty() ? QStringLiteral("生成 KML 失败") : joinMessages(report.errors);
        return false;
    }

    QFile file(kmlPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        _lastConvertError = QStringLiteral("无法写入 %1：%2").arg(kmlPath, file.errorString());
        return false;
    }
    if (file.write(xml) != xml.size()) {
        _lastConvertError = QStringLiteral("写入 %1 不完整：%2").arg(kmlPath, file.errorString());
        return false;
    }
    file.close();

    qInfo() << "plan -> kml 完成：" << kmlPath;
    return true;
}

bool DjiWaylineManager::convertKmzToPlan(const QString& kmzPath, const QString& planPath)
{
    _lastConvertError.clear();

    if (!QFileInfo::exists(kmzPath)) {
        _lastConvertError = QStringLiteral("找不到航线文件：%1").arg(kmzPath);
        return false;
    }

    wpt::Report report;
    if (!wpt::PlanConverter::djiToQgc(kmzPath, planPath, report)) {
        _lastConvertError = report.errors.isEmpty() ? QStringLiteral("转换失败") : joinMessages(report.errors);
        qWarning() << "kmz -> plan 失败：" << _lastConvertError;
        return false;
    }

    if (!report.warnings.isEmpty()) {
        qWarning() << "kmz -> plan 告警：" << joinMessages(report.warnings);
    }
    qInfo() << "kmz -> plan 完成：" << planPath;
    return true;
}

// ---------------------------------------------------------------------------
// 工具
// ---------------------------------------------------------------------------

QString DjiWaylineManager::downloadDir() const
{
    const QString base = SettingsManager::instance()->appSettings()->missionSavePath();
    const QString dir = base + QStringLiteral("/DjiWaylines");

    QDir().mkpath(dir);
    return dir;
}

QString DjiWaylineManager::_resolveLocalKmz(const QString& name) const
{
    // 1. 直接给的完整路径或当前目录下的相对路径
    if (QFileInfo::exists(name) && QFileInfo(name).isFile()) {
        return QFileInfo(name).absoluteFilePath();
    }

    // 2~4. 依次在下载目录和任务目录里找，含/不含 .kmz 后缀都试
    const QStringList dirs = {
        downloadDir(),
        SettingsManager::instance()->appSettings()->missionSavePath(),
    };
    for (const QString& dir : dirs) {
        for (const QString& suffix : {QStringLiteral(".kmz"), QStringLiteral("")}) {
            const QString candidate = dir + QStringLiteral("/") + name + suffix;
            if (QFileInfo::exists(candidate)) {
                return candidate;
            }
        }
    }
    return QString();
}

QString DjiWaylineManager::_normalizeName(const QString& name)
{
    QString out = name.trimmed();

    // 去掉可能的目录部分
    const int slash = qMax(out.lastIndexOf(QLatin1Char('/')), out.lastIndexOf(QLatin1Char('\\')));
    if (slash >= 0) {
        out = out.mid(slash + 1);
    }

    // 去掉 .kmz / .plan 后缀
    if (out.endsWith(QStringLiteral(".kmz"), Qt::CaseInsensitive)) {
        out.chop(4);
    } else if (out.endsWith(QStringLiteral(".plan"), Qt::CaseInsensitive)) {
        out.chop(5);
    }

    out = out.trimmed();

    // 云端对航线名有字符集限制，含下面任一字符一律 210002：
    //     < > : " / | ? * . _ \
    // （后台 DTO 上的 @Pattern "^[^<>:\"/|?*._\\]+$"，报错原文见下）
    //
    // 这个坑比看上去难缠，登记和查询两条接口不是一个态度：
    //   - POST upload-callback（登记）**不校验**名字，带 `_` 照样入库、上传"成功"；
    //   - GET waylines（列表）对**每一条返回记录**都过一遍校验（cloud-sdk 的
    //     CloudSDKHandler 切面 checkResponse -> validData），只要库里躺着一条
    //     非法名字，整个列表接口就恒回 210002，页面显示"获取航线列表失败"，
    //     而且是**永久**的 —— 删都删不了，因为删除要先从列表拿到 wayline_id。
    // 实测：上传 20241230_1Test2025副本.kmz 时登记成功，随后列表全挂。
    //
    // 所以必须在**发出之前**换掉，不能指望后台拒绝：统一换成 '-'（在允许集里，
    // 且和 _ / . 视觉上最接近）。本地 .plan 名字里带日期、下划线是常态。
    static const QString kForbidden = QStringLiteral("<>:\"/|?*._\\");
    for (QChar& ch : out) {
        if (kForbidden.contains(ch)) {
            ch = QLatin1Char('-');
        }
    }

    return out.trimmed();
}

// ---------------------------------------------------------------------------
// 网络底层
// ---------------------------------------------------------------------------

void DjiWaylineManager::_get(const QString& path, std::function<void(QNetworkReply*)> handler)
{
    QNetworkReply* reply = _networkManager->get(makeRequest(path));
    connect(reply, &QNetworkReply::finished, this, [handler, reply]() { handler(reply); });
}

void DjiWaylineManager::_getRedirectLocation(const QString& path,
                                            std::function<void(const QString&, const QString&)> onDone)
{
    // 后台这个 .../waylines/{id}/url 不返回 JSON —— 它 **302 到对象存储**，下载地址
    // 在 Location 头里。实测：
    //     HTTP/1.1 302
    //     Location: http://192.168.144.110:9000/cloud-bucket/wayline/<名字>.kmz?X-Amz-...
    //     Content-Length: 0
    //
    // 所以这里必须**把自动跟随关掉**。QNetworkAccessManager 默认是 NoLessSafe
    // （http->http 允许），会把 302 一路跟掉、把整个 kmz 拉进 reply 里；上层再拿
    // 这堆二进制去 QJsonDocument::fromJson，解析失败得到空对象，而空对象的
    // ["code"].toInt() 恰好是 0 —— 于是通过了 code!=0 那道检查，最后死在
    // 「后台返回的 url 为空」上。看着像后台没给地址，其实是地址在响应头上没读，
    // 而且文件白下了一遍。
    QNetworkRequest request = makeRequest(path);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);

    QNetworkReply* reply = _networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [reply, onDone]() {
        reply->deleteLater();

        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        // 关掉自动跟随后重定向就是本回复的最终状态。302 在 QNetworkReply 里不算
        // 错误（error() 是 NoError），但有的 Qt 版本会给 RedirectionError，
        // 所以只看状态码，不看 error()。
        if (status >= 300 && status < 400) {
            const QString location = QString::fromUtf8(reply->rawHeader("Location")).trimmed();
            if (location.isEmpty()) {
                onDone(QString(), QStringLiteral("HTTP %1 没有 Location 头").arg(status));
                return;
            }
            // 相对地址按请求地址解析（这份后台给的是绝对地址，顺手兜住）
            onDone(reply->request().url().resolved(QUrl(location)).toString(), QString());
            return;
        }

        // 不是重定向：退回按 JSON 读，兼容后台改回 data.url 写法的那天
        const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
        const QString url = root["data"].toObject()["url"].toString();
        if (!url.isEmpty()) {
            onDone(url, QString());
            return;
        }

        onDone(QString(), QStringLiteral("HTTP %1：%2")
                              .arg(status)
                              .arg(root["message"].toString(reply->errorString())));
    });
}

void DjiWaylineManager::_send(const QByteArray& verb, const QString& path, const QByteArray& body,
                              const QString& contentType, std::function<void(QNetworkReply*)> handler)
{
    QNetworkRequest request = makeRequest(path);
    if (!contentType.isEmpty()) {
        request.setHeader(QNetworkRequest::ContentTypeHeader, contentType);
    }
    if (!body.isEmpty()) {
        request.setHeader(QNetworkRequest::ContentLengthHeader, body.size());
    }

    QNetworkReply* reply = _networkManager->sendCustomRequest(request, verb, body);
    connect(reply, &QNetworkReply::finished, this, [handler, reply]() { handler(reply); });
}

void DjiWaylineManager::_downloadToFile(const QUrl& url, const QString& destPath,
                                        std::function<void(const QString&, const QString&)> onDone)
{
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply* reply = _networkManager->get(request);
    connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
        if (total > 0) {
            emit transferProgress(QStringLiteral("download"), static_cast<int>(received * 100 / total));
        }
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, destPath, onDone]() {
        reply->deleteLater();

        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError || status >= 400) {
            onDone(QString(), QStringLiteral("下载失败（HTTP %1：%2）").arg(status).arg(reply->errorString()));
            return;
        }

        const QByteArray data = reply->readAll();
        if (data.isEmpty()) {
            onDone(QString(), QStringLiteral("下载失败：文件内容为空"));
            return;
        }

        QDir().mkpath(QFileInfo(destPath).absolutePath());
        QFile file(destPath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            onDone(QString(), QStringLiteral("无法写入 %1：%2").arg(destPath, file.errorString()));
            return;
        }
        if (file.write(data) != data.size()) {
            onDone(QString(), QStringLiteral("写入 %1 不完整：%2").arg(destPath, file.errorString()));
            return;
        }
        file.close();

        onDone(destPath, QString());
    });
}

// ---------------------------------------------------------------------------

void DjiWaylineManager::_setBusy(bool busy)
{
    if (_busy == busy) {
        return;
    }
    _busy = busy;
    emit busyChanged();
}

void DjiWaylineManager::_setPreviewBusy(bool busy)
{
    if (_previewBusy == busy) {
        return;
    }
    _previewBusy = busy;
    emit previewBusyChanged();
}

void DjiWaylineManager::_fail(const QString& message)
{
    _lastError = message;
    qWarning() << "航线管理：" << message;
    emit errorOccurred(message);
}
