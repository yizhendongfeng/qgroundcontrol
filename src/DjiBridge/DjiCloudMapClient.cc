/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

/// @file
/// @brief DJI 上云 API 地图元素（Pilot 地图标注）客户端实现

#include "DjiCloudMapClient.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonValue>
#include <QtCore/QRegularExpression>
#include <QtCore/QSet>
#include <QtCore/QTimer>
#include <QtCore/QUuid>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

#include "CloudServerSettings.h"
#include "QGCGeoGcj02.h"
#include "QGCMapPolygon.h"
#include "QGCMapPolyline.h"
#include "SettingsManager.h"


namespace {

constexpr int ReplyTimeoutMs = 8000;

/// Web 端控制台的元素配色（Cloud-API-Demo-Web/src/constants/map.ts 的 MapElementColor）
constexpr const char* DefaultElementColor = "#2D8CF0";

/// 后端 properties.color 的约束是 ^#[0-9a-fA-F]{6}$，脏值直接换默认色，别让后端 400。
/// 统一转大写：界面上的配色 swatch 是 DJI 控制台那组大写常量，靠字符串相等判断选中，
/// 后端若回小写就会一个都点不亮。
QString sanitizedColor(const QString& color)
{
    static const QRegularExpression re(QStringLiteral("^#[0-9a-fA-F]{6}$"));
    return re.match(color).hasMatch() ? color.toUpper() : QString::fromLatin1(DefaultElementColor);
}

/// 按协议 content 是 **对象**，但真机上见过被二次编码成 JSON 字符串的（后端部分版本把
/// content 当 String 存 / 转发）。只认 toObject() 的话拿到的永远是空对象，后果是：
/// 颜色解析不出 → 全变默认蓝；geometry 解析不出 → 0 个顶点。用户看到的就是
/// 「平台画的东西本地出不来 / 跟平台不一样」。两种形态都认。
/// 地图元素和飞行区域共用：两边的 content 是同一个结构，坑也一样。
QJsonObject jsonObjectOrEncoded(const QJsonValue& value)
{
    if (value.isObject()) {
        return value.toObject();
    }
    if (value.isString()) {
        QJsonParseError error{};
        const QJsonDocument doc = QJsonDocument::fromJson(value.toString().toUtf8(), &error);
        if (error.error == QJsonParseError::NoError && doc.isObject()) {
            return doc.object();
        }
    }
    return {};
}

/// 取 resource.content（见 jsonObjectOrEncoded）
QJsonObject resourceContent(const QJsonObject& resource)
{
    return jsonObjectOrEncoded(resource.value(QStringLiteral("content")));
}

/// 后端图形是否按 GCJ-02（高德）存。开着就是「进来转 WGS84、出去转回 GCJ-02」。
/// 关掉只适用于"后端老老实实存 WGS84"的情况 —— 那种后端照官方 Web 控制台的写法
/// 是**存 WGS84** 的，但用户的定制后端直接吃高德的坐标，所以默认开着。
bool coordinateTransformEnabled()
{
    // 注意 DEFINE_SETTINGFACT 生成的 getter 是非 const 的（Fact* NAME();），这里不能收成
    // const CloudServerSettings*
    CloudServerSettings* settings = SettingsManager::instance()->cloudServerSettings();
    return settings && settings->coordinateTransform()->rawValue().toBool();
}

/// 按 GeoJSON 的嵌套层数把 coordinates 一路剥到第一个「坐标点」。
/// 线的 [[lon,lat],...] 和面的 [[[lon,lat],...]] 都能剥到；纯给日志对照用，
/// 拿的是**未经纠偏**的原始值 —— 这是判断纠偏方向对不对的唯一依据。
QGeoCoordinate firstRawCoordinate(const QJsonObject& resource)
{
    QJsonValue value = resourceContent(resource)
                           .value(QStringLiteral("geometry")).toObject()
                           .value(QStringLiteral("coordinates"));

    while (value.isArray()) {
        const QJsonArray array = value.toArray();
        if (array.isEmpty()) {
            return {};
        }
        if (array.at(0).isDouble()) {
            return array.size() >= 2
                       ? QGeoCoordinate(array.at(1).toDouble(), array.at(0).toDouble())
                       : QGeoCoordinate();
        }
        value = array.at(0);
    }
    return {};
}

/// GeoJSON 的一个坐标点 [lon,lat] → QGeoCoordinate，按开关决定要不要纠偏。
/// 点不够两个数（或者不是数）时返回无效坐标。
QGeoCoordinate transformedCoordinate(const QJsonArray& point)
{
    if (point.size() < 2) {
        return {};
    }

    double longitude = point.at(0).toDouble();
    double latitude = point.at(1).toDouble();
    if (coordinateTransformEnabled()) {
        QGCGeo::gcj02ToWgs84(longitude, latitude);
    }
    // 注意 QGeoCoordinate 的构造函数是（纬度, 经度）
    return QGeoCoordinate(latitude, longitude);
}

/// 端点（Point）元素的 geometry：coordinates 是裸的 [lon,lat]，
/// 不像线面那样再套一层（ElementPointGeometry.java）。
QGeoCoordinate parsePoint(const QJsonObject& geometry, const char* tag)
{
    const QString geometryType = geometry.value(QStringLiteral("type")).toString();
    if (!geometryType.isEmpty() && geometryType != QStringLiteral("Point")) {
        qWarning() << tag << "期望 Point，实际几何类型：" << geometryType;
        return {};
    }

    const QGeoCoordinate coordinate =
        transformedCoordinate(geometry.value(QStringLiteral("coordinates")).toArray());
    if (!coordinate.isValid()) {
        qWarning() << tag << "端点坐标无效或缺失，跳过";
    }
    return coordinate;
}

/// 把 GeoJSON 的 geometry 解析成 QGeoCoordinate 列表。地图元素和飞行区域共用这一份
/// （飞行区域的 content 结构和地图元素完全一样，只是几何只有 Polygon/Circle）。
///
/// 坐标是 GCJ-02 还是 WGS84 由 coordinateTransform 决定：后端按高德存的时候，
/// 不纠偏就会整体偏 300~600 米，看起来就是"平台画的图形和本地的对不上"。
///
/// @param polygon true 按面的环解析（[[[lon,lat],...]]），false 按线解析（[[lon,lat],...]）
/// @param tag 日志前缀，两条链路（"[云元素]" / "[云飞行区域]"）共用这份代码但要能分辨
QList<QGeoCoordinate> parseCoordinates(const QJsonObject& geometry, bool polygon, const char* tag)
{
    QList<QGeoCoordinate> coordinates;

    const QString geometryType = geometry.value(QStringLiteral("type")).toString();
    const QJsonArray raw = geometry.value(QStringLiteral("coordinates")).toArray();

    auto appendPoint = [&coordinates](const QJsonArray& point) {
        const QGeoCoordinate coordinate = transformedCoordinate(point);
        if (coordinate.isValid()) {
            coordinates.append(coordinate);
        }
    };

    if (polygon) {
        if (!geometryType.isEmpty() && geometryType != QStringLiteral("Polygon")) {
            qWarning() << tag << "期望 Polygon，实际几何类型：" << geometryType
                       << "coordinates 层数：" << raw.size();
            return {};
        }
        // 后端的 Polygon 是 [[[lon,lat],...]]；也容忍少一层的 [[lon,lat],...]（取它当环）
        const QJsonArray ring = raw.isEmpty() ? QJsonArray()
                             : raw.at(0).isArray() && !raw.at(0).toArray().isEmpty()
                                   && raw.at(0).toArray().at(0).isArray()
                               ? raw.at(0).toArray()
                               : raw;
        for (const QJsonValue& point : ring) {
            appendPoint(point.toArray());
        }
    } else {
        if (!geometryType.isEmpty() && geometryType != QStringLiteral("LineString")) {
            qWarning() << tag << "期望 LineString，实际几何类型：" << geometryType
                       << "coordinates 层数：" << raw.size();
            return {};
        }
        for (const QJsonValue& point : raw) {
            appendPoint(point.toArray());
        }
    }

    // GeoJSON 的 Polygon 环是闭合的（首尾同点），而 QGCMapPolygon 自己负责闭合：
    // 把重复的收尾点留着会多出一个重合顶点（地图上多一个拖拽手柄、边上多一段零长度），
    // 看起来就是"平台画的图形和本地的对不上"。这里按 GeoJSON 语义把收尾点去掉。
    if (polygon && coordinates.size() >= 2 && coordinates.first() == coordinates.last()) {
        coordinates.removeLast();
        qInfo() << tag << "去掉 Polygon 环的闭合点，顶点数 =" << coordinates.size();
    }

    // 退化几何直接丢掉：一条 0/1 个点的"线"、一个 0/1/2 个点的"面"喂给
    // QGCMapPolyline/QGCMapPolygon 之后，visuals 的 Repeater 会拿着越界的 index 回调
    // （adjustVertex / splitSegment），命中 QList::operator[] 断言。
    const int minimum = polygon ? 3 : 2;
    if (coordinates.size() < minimum) {
        qWarning() << tag << "几何顶点不足，跳过："
                   << (polygon ? "面" : "线") << "geometry.type =" << geometryType
                   << "解析出" << coordinates.size() << "个顶点（至少" << minimum
                   << "），coordinates 原始层数：" << raw.size();
        return {};
    }

    return coordinates;
}

/// 把网络错误转成能直接显示的一行字；没有错误返回空串。
/// 注意 401/404 这类会被 nginx 直接返回 HTML，errorString() 只有 "Unknown error"，
/// 所以 httpStatus 也要带上。
QString replyErrorText(QNetworkReply* reply)
{
    if (reply->error() == QNetworkReply::NoError) {
        return QString();
    }

    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString errorString = reply->errorString();
    if (httpStatus > 0) {
        return QStringLiteral("HTTP %1 %2").arg(httpStatus).arg(errorString);
    }
    return errorString;
}

}  // namespace

// ---------------------------------------------------------------------------
// 元素条目
// ---------------------------------------------------------------------------

CloudMapElement::CloudMapElement(int type, const QString& id, const QString& name,
                                 const QString& color, QObject* parent)
    : QObject(parent)
    , _id(id)
    , _name(name)
    // 只可能是 0/1/2（DjiCloudMapClient::_isSupportedType 已经把过关，这里对负值兜个底）。
    // 用字面量而不是 DjiCloudMapClient::TypeXxx：那是另一个类的私有常量，这里看不见。
    , _type((type == 0 || type == 1 || type == 2) ? type : 0)
    , _color(sanitizedColor(color))
{
    if (_type == 2) {
        _polygon = new QGCMapPolygon(this);
        connect(_polygon, &QGCMapPolygon::pathChanged, this, &CloudMapElement::geometryChanged);
    } else if (_type == 1) {
        _polyline = new QGCMapPolyline(this);
        connect(_polyline, &QGCMapPolyline::pathChanged, this, &CloudMapElement::geometryChanged);
    }
    // Point：没有对应的 QGCMap* 类（QGC 里没有 QGCMapPoint），坐标就存在 _point 上，
    // 地图上由 CloudElementMarker.qml 画成菱形图钉
}

QObject* CloudMapElement::geometry() const
{
    if (_polygon) {
        return _polygon;
    }
    return _polyline;   // Point 时两个都是空，返回 nullptr
}

int CloudMapElement::vertexCount() const
{
    if (_polygon) {
        return _polygon->count();
    }
    if (_polyline) {
        return _polyline->count();
    }
    return _point.isValid() ? 1 : 0;
}

bool CloudMapElement::isEmpty() const
{
    return vertexCount() == 0;
}

QList<QGeoCoordinate> CloudMapElement::coordinates() const
{
    if (_polygon) {
        return _polygon->coordinateList();
    }
    if (_polyline) {
        return _polyline->coordinateList();
    }
    if (_point.isValid()) {
        return { _point };
    }
    return {};
}

void CloudMapElement::setName(const QString& name)
{
    if (_name == name) {
        return;
    }
    _name = name;
    emit nameChanged();
}

void CloudMapElement::setColor(const QString& color)
{
    const QString value = sanitizedColor(color);
    if (_color == value) {
        return;
    }
    _color = value;
    emit colorChanged();
}

void CloudMapElement::setId(const QString& id)
{
    if (_id == id) {
        return;
    }
    _id = id;
    emit idChanged();
}

void CloudMapElement::setFromCloud(bool fromCloud)
{
    if (_fromCloud == fromCloud) {
        return;
    }
    _fromCloud = fromCloud;
    emit fromCloudChanged();
}

void CloudMapElement::setDirty(bool dirty)
{
    if (_dirty == dirty) {
        return;
    }
    _dirty = dirty;
    emit dirtyChanged();
}

void CloudMapElement::setEditing(bool editing)
{
    if (_editing == editing) {
        return;
    }
    _editing = editing;

    // 几何对象自己的 interactive 才是 visuals 出不出拖拽手柄的依据
    if (_polygon) {
        _polygon->setInteractive(editing);
    } else if (_polyline) {
        _polyline->setInteractive(editing);
    }
    if (!editing) {
        setTracing(false);
    }

    emit editingChanged();
}

void CloudMapElement::setTracing(bool tracing)
{
    if (_tracing == tracing) {
        return;
    }
    _tracing = tracing;
    if (_polygon) {
        _polygon->setTraceMode(tracing);
    } else if (_polyline) {
        _polyline->setTraceMode(tracing);
    }
    emit tracingChanged();
}

void CloudMapElement::setCoordinates(const QList<QGeoCoordinate>& coordinates)
{
    // clear() + appendVertices() 必须夹在 beginReset/endReset 之间 —— 这是 QGC 自己的写法
    // （QGCMapPolygonVisuals._resetPolygon 就是这么干的）。分开写会走两轮模型 reset：
    // 第一轮把 pathModel 清空 → 在 QML 侧同步销毁全部顶点拖拽手柄 / 拆段手柄（都是
    // mapControl 上的 MapQuickItem，在 Component.onDestruction 里 destroy），第二轮再重建。
    // 元素是平台推来的、随时可能整体换几何，这个窗口里手柄带着已经作废的 index 回调回来，
    // 就会命中 QList::operator[] 的 index out of range 断言。合成一轮 reset 后 QML 只看到
    // 一次「旧的全没了、新的全来了」，不会有中间态。
    QGCMapPolygon* polygon = _polygon;
    QGCMapPolyline* polyline = _polyline;
    if (polygon) {
        polygon->beginReset();
        polygon->clear();
        polygon->appendVertices(coordinates);
        polygon->endReset();
        polygon->setDirty(false);
    } else if (polyline) {
        polyline->beginReset();
        polyline->clear();
        polyline->appendVertices(coordinates);
        polyline->endReset();
        polyline->setDirty(false);
    } else {
        // Point：只取第一个点（后端给的就是单点）
        _point = coordinates.isEmpty() ? QGeoCoordinate() : coordinates.first();
    }
    emit geometryChanged();
}

// ---------------------------------------------------------------------------
// 飞行区域（任务区域 / GEO 区域）
// ---------------------------------------------------------------------------

CloudFlightArea::CloudFlightArea(const QString& id, const QString& name, const QString& type,
                                 bool enabled, const QList<QGeoCoordinate>& ring, QObject* parent)
    : QObject(parent)
    , _id(id)
    , _name(name)
    , _type(type)
    , _enabled(enabled)
    , _polygon(new QGCMapPolygon(this))
{
    connect(_polygon, &QGCMapPolygon::pathChanged, this, &CloudFlightArea::geometryChanged);

    // 只读图层：进来就是最终形状，之后不再改（平台改了就整条重新拉一次）。
    // QGCMapPolygon 默认 interactive == false，所以不会出拖拽手柄。
    _polygon->setPath(ring);
    _polygon->setDirty(false);
}

QObject* CloudFlightArea::geometry() const
{
    return _polygon;
}

int CloudFlightArea::vertexCount() const
{
    return _polygon ? _polygon->count() : 0;
}

QString CloudFlightArea::color() const
{
    // 官方 Web 控制台 use-g-map-cover.ts 的 flightAreaColorMap：
    // 任务区域(dfence) 绿、GEO/禁飞区(nfz) 红；停用的一律灰。
    if (!_enabled) {
        return QStringLiteral("#B3B3B3");
    }
    return geoZone() ? QStringLiteral("#FF0000") : QStringLiteral("#19BE6B");
}

// ---------------------------------------------------------------------------
// 地图元素客户端
// ---------------------------------------------------------------------------

DjiCloudMapClient::DjiCloudMapClient(QObject* parent)
    : QObject(parent)
    , _net(new QNetworkAccessManager(this))
    , _lines(new QmlObjectListModel(this))
    , _areas(new QmlObjectListModel(this))
    , _points(new QmlObjectListModel(this))
    , _flightAreas(new QmlObjectListModel(this))
{
}

DjiCloudMapClient::~DjiCloudMapClient() = default;

QObject* DjiCloudMapClient::editing() const
{
    return _editing;
}

int DjiCloudMapClient::count() const
{
    return _lines->count() + _areas->count() + _points->count();
}

QString DjiCloudMapClient::_token() const
{
    return SettingsManager::instance()->cloudServerSettings()->serverToken()->rawValueString();
}

QString DjiCloudMapClient::_workspaceId() const
{
    return SettingsManager::instance()->cloudServerSettings()->workSpaceId()->rawValueString();
}

QUrl DjiCloudMapClient::_apiUrl(const QString& path) const
{
    // REST base 不能用 serverUrl：那是 pilot 登录页（默认 http://ip:8080/pilot-login），
    // 而平台 HTTP 接口与 WebSocket 同 host 同端口（Web 端 config.ts 里 baseURL 与
    // websocketURL 都是 :6789），所以从 websocketUrl 推导最稳。
    QUrl base(SettingsManager::instance()->cloudServerSettings()->websocketUrl()->rawValueString());
    base.setScheme(base.scheme() == QStringLiteral("wss") ? QStringLiteral("https") : QStringLiteral("http"));
    base.setPath(path);
    base.setQuery(QString());
    base.setFragment(QString());
    return base;
}

void DjiCloudMapClient::_setStatus(const QString& status)
{
    if (_status == status) {
        return;
    }
    _status = status;
    emit statusChanged();
}

CloudMapElement* DjiCloudMapClient::_findById(const QString& id) const
{
    if (id.isEmpty()) {
        return nullptr;
    }
    return _byId.value(id, nullptr);
}

QString DjiCloudMapClient::_typeText(int type)
{
    if (type == TypeArea) {
        return QStringLiteral("区域");
    }
    if (type == TypePoint) {
        return QStringLiteral("端点");
    }
    return QStringLiteral("线段");
}

QmlObjectListModel* DjiCloudMapClient::_modelForType(int type) const
{
    if (type == TypeArea) {
        return _areas;
    }
    if (type == TypePoint) {
        return _points;
    }
    return _lines;
}

void DjiCloudMapClient::_removeElement(CloudMapElement* element)
{
    if (!element) {
        return;
    }
    if (_editing == element) {
        _editing = nullptr;
        emit editingChanged();
    }

    // 先把手柄收掉、几何清空，再把它从模型里摘掉。
    // 模型 removeRows 会在 QML 侧同步销毁对应 delegate，但 Instantiator 的析构不保证
    // 和模型同步；这里让 visuals 先看到一个「空的、不可交互的」几何，它就不会拿着一个
    // 已经 deleteLater 的 QGCMapPolygon* 去读 pathModel。
    element->setEditing(false);
    element->setTracing(false);
    element->setCoordinates({});

    _byId.remove(element->id());
    _lines->removeOne(element);
    _areas->removeOne(element);
    _points->removeOne(element);
    element->deleteLater();
    emit countChanged();
}

void DjiCloudMapClient::_resetAll()
{
    _editing = nullptr;
    _editingOriginal.clear();
    _refreshInFlight = false;
    ++_generation;   // 作废在途的 GET
    _byId.clear();
    _sharedGroupId.clear();
    _lines->clearAndDeleteContents();
    _areas->clearAndDeleteContents();
    _points->clearAndDeleteContents();
    emit editingChanged();
    emit countChanged();
}

void DjiCloudMapClient::clearAll()
{
    _resetAll();
    _setStatus(QString());

    // 飞行区域属于同一次上云会话，一起清掉。两个显示开关也复位：图标亮着而地图上
    // 什么都没有（数据被清了）最容易被当成 bug。_resetAll() 已经把 _generation +1，
    // 在途的 flight-areas GET 回来时会对不上代数而被丢弃。
    _flightAreaRefreshInFlight = false;
    _showTaskAreas = false;
    _showGeoZones = false;
    _flightAreaStatus.clear();
    _flightAreaData.clear();
    _flightAreas->clearAndDeleteContents();
    emit flightAreasChanged();
}

// ---------------------------------------------------------------------------
// 元素 JSON
// ---------------------------------------------------------------------------

int DjiCloudMapClient::_resourceType(const QJsonObject& resource)
{
    // ElementResourceTypeEnum: 0=Point 1=LineString 2=Polygon
    const QJsonValue value = resource.value(QStringLiteral("type"));
    const int type = value.toInt(-1);
    if (_isSupportedType(type)) {
        return type;
    }

    // resource.type 缺失 / 用了字符串枚举值时退回按 geometry.type 判：
    // 直接返回 -1 会被上层当"不支持"跳过，元素在平台上有、本地却什么都没有。
    const QJsonObject geometry = resourceContent(resource).value(QStringLiteral("geometry")).toObject();
    const QString geometryType = geometry.value(QStringLiteral("type")).toString();
    if (geometryType == QStringLiteral("Point")) {
        qWarning() << "[云元素] resource.type 不可用（" << value << "），按 geometry.type 判为端点";
        return TypePoint;
    }
    if (geometryType == QStringLiteral("LineString")) {
        qWarning() << "[云元素] resource.type 不可用（" << value << "），按 geometry.type 判为线段";
        return TypeLine;
    }
    if (geometryType == QStringLiteral("Polygon")) {
        qWarning() << "[云元素] resource.type 不可用（" << value << "），按 geometry.type 判为区域";
        return TypeArea;
    }
    return type;
}

QString DjiCloudMapClient::_resourceColor(const QJsonObject& resource)
{
    const QJsonObject properties = resourceContent(resource).value(QStringLiteral("properties")).toObject();
    return sanitizedColor(properties.value(QStringLiteral("color")).toString());
}

QList<QGeoCoordinate> DjiCloudMapClient::_resourceCoordinates(const QJsonObject& resource, int type)
{
    const QJsonObject geometry = resourceContent(resource).value(QStringLiteral("geometry")).toObject();
    if (type == TypePoint) {
        const QGeoCoordinate coordinate = parsePoint(geometry, "[云元素]");
        return coordinate.isValid() ? QList<QGeoCoordinate>{ coordinate } : QList<QGeoCoordinate>{};
    }
    return parseCoordinates(geometry, type == TypeArea, "[云元素]");
}

QJsonArray DjiCloudMapClient::_coordinatesToJson(const QList<QGeoCoordinate>& coordinates, int type)
{
    // 本机几何是 WGS84，后端按高德存的时候要转回 GCJ-02，否则平台上的图形会整体偏出去
    // 几百米 —— 和下行是同一套开关，两边必须一起转，只转一边等于偏两倍。
    const bool transform = coordinateTransformEnabled();

    QJsonArray points;
    for (const QGeoCoordinate& coordinate : coordinates) {
        double longitude = coordinate.longitude();
        double latitude = coordinate.latitude();
        if (transform) {
            QGCGeo::wgs84ToGcj02(longitude, latitude);
        }
        QJsonArray point;
        point.append(longitude);
        point.append(latitude);
        points.append(point);
    }

    if (type == TypePoint) {
        // Point 的 coordinates 是裸的 [lon,lat]（不比线面多套一层）
        return points.isEmpty() ? QJsonArray() : points.at(0).toArray();
    }
    if (type == TypeArea) {
        QJsonArray rings;
        rings.append(points);
        return rings;
    }
    return points;
}

QJsonObject DjiCloudMapClient::_elementContent(const CloudMapElement* element, const QString& color) const
{
    QJsonObject properties;
    properties[QStringLiteral("color")] = sanitizedColor(color);
    properties[QStringLiteral("clampToGround")] = true;

    QJsonObject geometry;
    geometry[QStringLiteral("type")] = element->type() == TypeArea  ? QStringLiteral("Polygon")
                                     : element->type() == TypePoint ? QStringLiteral("Point")
                                                                    : QStringLiteral("LineString");
    geometry[QStringLiteral("coordinates")] = _coordinatesToJson(element->coordinates(), element->type());

    QJsonObject content;
    content[QStringLiteral("type")] = QStringLiteral("Feature");
    content[QStringLiteral("properties")] = properties;
    content[QStringLiteral("geometry")] = geometry;

    // 上行也留一行对照：本机的 WGS84 首点 与 实际发出去的（可能已纠偏的）首点。
    // 下行日志里的"原始首点"和这里的两个值放一起，就能判断纠偏方向到底对不对。
    const QList<QGeoCoordinate> sent = element->coordinates();
    if (!sent.isEmpty()) {
        const QJsonValue sentValue = geometry.value(QStringLiteral("coordinates"));
        QJsonValue probe = sentValue;
        while (probe.isArray() && !probe.toArray().isEmpty()) {
            const QJsonArray array = probe.toArray();
            if (array.at(0).isDouble()) {
                qInfo() << "[云元素] 上行" << _typeText(element->type())
                        << "本机首点(84) =" << sent.first()
                        << "实际发出 =" << QGeoCoordinate(array.at(1).toDouble(), array.at(0).toDouble())
                        << "纠偏 =" << (coordinateTransformEnabled() ? "开" : "关");
                break;
            }
            probe = array.at(0);
        }
    }
    return content;
}

// ---------------------------------------------------------------------------
// ws 推送
// ---------------------------------------------------------------------------

void DjiCloudMapClient::onWsMapElement(const QString& bizCode, const QJsonObject& data)
{
    if (bizCode == QStringLiteral("map_group_refresh")) {
        // 后端批量改过元素（网页端拖拽等），协议要求客户端收到后再拉一次全量列表
        const QJsonArray ids = data.value(QStringLiteral("ids")).toArray();
        qInfo() << "[云元素] map_group_refresh，重新拉取元素列表，批次数：" << ids.size();
        refresh();
        return;
    }

    const QString id = data.value(QStringLiteral("id")).toString();
    if (id.isEmpty()) {
        qWarning() << "[云元素]" << bizCode << "缺少 id，忽略";
        return;
    }

    if (bizCode == QStringLiteral("map_element_delete")) {
        if (CloudMapElement* element = _findById(id)) {
            const QString name = element->name();
            _removeElement(element);
            _setStatus(tr("元素已被平台删除：%1").arg(name));
        } else {
            _byId.remove(id);  // 我们自己删的：平台回推的 delete 到这里已经没有对应条目
        }
        return;
    }

    if (bizCode == QStringLiteral("map_element_create") ||
        bizCode == QStringLiteral("map_element_update")) {
        const QJsonObject resource = data.value(QStringLiteral("resource")).toObject();
        const int type = _resourceType(resource);
        if (!_isSupportedType(type)) {
            // Circle 等新几何不认。
            // 打全 geometry.type / content 形态，好判断是"平台真的发了新形状"还是"解析错了"
            const QJsonObject content = resourceContent(resource);
            qInfo() << "[云元素] 跳过不支持的元素类型 type =" << type << "id =" << id
                    << "geometry.type =" << content.value(QStringLiteral("geometry")).toObject()
                                                  .value(QStringLiteral("type")).toString()
                    << "content 是对象 =" << resource.value(QStringLiteral("content")).isObject();
            return;
        }

        const QString name = data.value(QStringLiteral("name")).toString();
        CloudMapElement* element = _findById(id);
        if (element && element->editing() && element->dirty()) {
            // 正在编辑：几何以本机为准，只跟随平台的名称/颜色，否则用户拖的点会被回推的旧几何盖掉
            element->setName(name);
            element->setColor(_resourceColor(resource));
            qInfo() << "[云元素] 编辑中的元素收到平台回推，保留本机几何，id =" << id;
            return;
        }

        _applyResource(id, name, resource, true);
        _setStatus(tr("已同步平台元素"));
    }
}

CloudMapElement* DjiCloudMapClient::_applyResource(const QString& id, const QString& name,
                                                  const QJsonObject& resource, bool fromCloud)
{
    const int type = _resourceType(resource);
    if (!_isSupportedType(type)) {
        return nullptr;
    }

    const QString color = _resourceColor(resource);
    CloudMapElement* element = _findById(id);
    if (element && element->type() != type) {
        // 后端把元素类型改了（线上不常见，但别让模型错位）
        _removeElement(element);
        element = nullptr;
    }

    if (!element) {
        element = new CloudMapElement(type, id, name, color, this);
        _byId.insert(id, element);
        _modelForType(type)->append(element);
        emit countChanged();
    }

    element->setName(name);
    element->setColor(color);
    element->setFromCloud(fromCloud);
    element->setDirty(false);
    element->setCoordinates(_resourceCoordinates(resource, type));

    // 一行一个元素，用来和平台控制台上看到的位置/形状对账：
    //   顶点数对不上        → 解析问题
    //   顶点数对而位置偏    → 坐标系问题，拿"原始首点"和"落图首点"比一比，
    //                        两者差 300~600 米说明纠偏方向反了（改 CloudServer 的坐标纠偏开关）
    //   两个都对但形状不一样 → 平台那边就是这样画的
    const QList<QGeoCoordinate> applied = element->coordinates();
    qInfo() << "[云元素] 应用" << (fromCloud ? "平台" : "本机") << "元素"
            << _typeText(type) << "name =" << element->name()
            << "色 =" << element->color() << "顶点数 =" << applied.size()
            << "原始首点 =" << firstRawCoordinate(resource)
            << "落图首点 =" << (applied.isEmpty() ? QGeoCoordinate() : applied.first())
            << "纠偏 =" << (coordinateTransformEnabled() ? "开" : "关")
            << "id =" << id;
    return element;
}

// ---------------------------------------------------------------------------
// HTTP
// ---------------------------------------------------------------------------

void DjiCloudMapClient::_sendRequest(const QByteArray& verb, const QString& path,
                                     const QJsonObject& body, const QString& opText,
                                     const std::function<void()>& onSuccess)
{
    const QString token = _token();
    if (token.isEmpty()) {
        _setStatus(tr("%1：缺少 serverToken，请先在网页端登录").arg(opText));
        qWarning() << "[云元素]" << opText << "缺少 serverToken，跳过";
        return;
    }

    QNetworkRequest request(_apiUrl(path));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("x-auth-token", token.toUtf8());
    request.setTransferTimeout(ReplyTimeoutMs);

    const QByteArray payload =
        body.isEmpty() ? QByteArray() : QJsonDocument(body).toJson(QJsonDocument::Compact);

    QNetworkReply* reply = nullptr;
    if (verb == "POST") {
        reply = _net->post(request, payload);
    } else if (verb == "PUT") {
        reply = _net->put(request, payload);
    } else {
        reply = _net->deleteResource(request);
    }

    connect(reply, &QNetworkReply::finished, this, [this, reply, opText, onSuccess]() {
        reply->deleteLater();

        const QByteArray data = reply->readAll();
        const QString error = replyErrorText(reply);
        if (!error.isEmpty()) {
            qWarning() << "[云元素]" << opText << "失败：" << error;
            _setStatus(tr("%1 失败：%2").arg(opText, error));
            return;
        }

        const QJsonObject result = QJsonDocument::fromJson(data).object();
        const int code = result.value(QStringLiteral("code")).toInt(-1);
        if (code != 0) {
            const QString message = result.value(QStringLiteral("message")).toString();
            qWarning() << "[云元素]" << opText << "被平台拒绝：" << code << message;
            _setStatus(tr("%1 失败：%2").arg(opText, message));
            return;
        }

        qInfo() << "[云元素]" << opText << "成功";
        _setStatus(tr("%1 成功").arg(opText));

        if (onSuccess) {
            onSuccess();
        }
    });
}

void DjiCloudMapClient::refresh()
{
    const QString workspaceId = _workspaceId();
    if (workspaceId.isEmpty()) {
        _setStatus(tr("刷新元素失败：workspaceId 为空"));
        qWarning() << "[云元素] workspaceId 为空，跳过刷新";
        return;
    }

    const QString token = _token();
    if (token.isEmpty()) {
        _setStatus(tr("刷新元素失败：缺少 serverToken"));
        return;
    }
    if (_refreshInFlight) {
        return;  // 首连与 map_group_refresh 撞在一起时不重复发
    }
    _refreshInFlight = true;

    QNetworkRequest request(
        _apiUrl(QStringLiteral("/map/api/v1/workspaces/%1/element-groups").arg(workspaceId)));
    request.setRawHeader("x-auth-token", token.toUtf8());
    request.setTransferTimeout(ReplyTimeoutMs);

    const quint64 generation = _generation;
    QNetworkReply* reply = _net->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation]() {
        reply->deleteLater();
        _refreshInFlight = false;

        if (generation != _generation) {
            return;   // 期间 clearAll() 过，这次结果作废
        }

        const QByteArray data = reply->readAll();
        const QString error = replyErrorText(reply);
        if (!error.isEmpty()) {
            qWarning() << "[云元素] 拉取元素列表失败：" << error;
            _setStatus(tr("拉取元素列表失败：%1").arg(error));
            return;
        }

        const QJsonObject result = QJsonDocument::fromJson(data).object();
        const int code = result.value(QStringLiteral("code")).toInt(-1);
        if (code != 0) {
            const QString message = result.value(QStringLiteral("message")).toString();
            qWarning() << "[云元素] 拉取元素列表被拒绝：" << code << message;
            _setStatus(tr("拉取元素列表失败：%1").arg(message));
            return;
        }

        _applyGroups(result.value(QStringLiteral("data")).toArray());
    });
}

void DjiCloudMapClient::_applyGroups(const QJsonArray& groups)
{
    // 全量刷新：平台是权威。按 id 合并，本机还没上传的草稿（fromCloud == false）保留。
    QSet<QString> seen;

    for (const QJsonValue& groupValue : groups) {
        const QJsonObject group = groupValue.toObject();
        if (group.value(QStringLiteral("type")).toInt(-1) == SharedGroupType) {
            // 上行 POST 要落在这个组；协议规定一个工作空间有且只有一个 APP 共享图层
            _sharedGroupId = group.value(QStringLiteral("id")).toString();
        }

        const QJsonArray elements = group.value(QStringLiteral("elements")).toArray();
        for (const QJsonValue& elementValue : elements) {
            const QJsonObject elementJson = elementValue.toObject();
            const QString id = elementJson.value(QStringLiteral("id")).toString();
            if (id.isEmpty()) {
                continue;
            }

            const QJsonObject resource = elementJson.value(QStringLiteral("resource")).toObject();
            const int type = _resourceType(resource);
            if (!_isSupportedType(type)) {
                continue;   // Circle 等新几何不支持，静默跳过
            }

            if (CloudMapElement* existing = _findById(id)) {
                if (existing->editing() && existing->dirty()) {
                    // 正在编辑：几何以本机为准，别被刷新冲掉
                    seen.insert(id);
                    continue;
                }
            }

            const QString name = elementJson.value(QStringLiteral("name")).toString();
            if (_applyResource(id, name, resource, true)) {
                seen.insert(id);
            }
        }
    }

    // 平台上已经不存在、而本地还标着"平台下发"的条目要删掉；
    // 本机草稿不动（它本来就还没上传）。先在哈希外收集，避免边遍历边改。
    QList<CloudMapElement*> stale;
    for (auto it = _byId.constBegin(); it != _byId.constEnd(); ++it) {
        CloudMapElement* element = it.value();
        if (element->fromCloud() && !seen.contains(element->id())) {
            stale.append(element);
        }
    }
    for (CloudMapElement* element : stale) {
        _removeElement(element);
    }

    // 注意：本机还没上传的草稿（fromCloud == false）不能在这里被改成"平台下发"，
    // 否则下一次刷新会把它当平台元素删掉。fromCloud 只由 _applyResource 置位。

    qInfo() << "[云元素] 拉取完成：端点" << _points->count() << "个，线段" << _lines->count()
            << "个，区域" << _areas->count() << "个";
    _setStatus(tr("已同步 %1 个元素").arg(count()));
    emit countChanged();
}

// ---------------------------------------------------------------------------
// 编辑会话
// ---------------------------------------------------------------------------

void DjiCloudMapClient::beginCreate(int type)
{
    if (!_isSupportedType(type)) {
        return;
    }
    if (_editing) {
        cancelEdit();
    }

    _startDraft(type);
    _setStatus(type == TypePoint ? tr("在地图上点击放置端点，可以连着点")
                                 : tr("新建元素：在地图上点击加点，完成后保存"));
}

CloudMapElement* DjiCloudMapClient::_startDraft(int type)
{
    // id 由客户端生成：后端 CreateMapElementRequest.id 就是 @NotNull 的 uuid，
    // 本地一建就用最终 id，平台把 create 回推回来时按 id 合并即可，不会有第二条。
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString name = (type == TypeArea  ? tr("区域")
                        : type == TypePoint ? tr("端点")
                                            : tr("线段"))
                       + QLatin1Char(' ') + QString::number(count() + 1);

    auto* element = new CloudMapElement(type, id, name, QString::fromLatin1(DefaultElementColor), this);
    _byId.insert(id, element);
    _modelForType(type)->append(element);

    _editing = element;
    _editingOriginal.clear();   // 本机草稿没有"原样"可还原，取消就整个丢掉
    element->setEditing(true);
    element->setDirty(true);
    // 建完就直接进"点地图"状态，和官方控制台一样：选完工具不用再按一次「开始标绘」。
    // 端点没有几何可标，tracing 对它的含义是"等地图点击放点"（见 placePoint）。
    element->setTracing(true);
    emit countChanged();
    emit editingChanged();
    return element;
}

void DjiCloudMapClient::placePoint(double latitude, double longitude)
{
    CloudMapElement* element = _editing;
    if (!element || element->type() != TypePoint) {
        return;
    }

    const QGeoCoordinate coordinate(latitude, longitude);
    if (!coordinate.isValid()) {
        qWarning() << "[云元素] 端点坐标无效，丢弃这次地图点击：" << latitude << longitude;
        return;
    }

    element->setCoordinates({ coordinate });

    // 编辑平台上的端点（面板上的「在地图上重新定位」）：点地图只是挪个位置，
    // 改动等「保存到平台」走 PUT，不能在这里又变出一个新端点
    if (element->fromCloud()) {
        element->setDirty(true);
        _setStatus(tr("端点已移动：点「保存到平台」生效"));
        return;
    }

    // 连续放置：本机草稿点一下就直接上云，然后马上准备下一个空草稿 —— 在地图上连点
    // 就连出一串端点，不用每放一个都回面板按一次「保存」。发送失败（没有 workspaceId
    // 或共享图层）时不建新草稿、保留这一个，状态里已经写了原因。
    const QString placedName = element->name();
    if (!_postNewElement(element)) {
        return;
    }
    element->setDirty(false);
    element->setEditing(false);
    _startDraft(TypePoint);
    _setStatus(tr("%1 已放置并同步，继续点击可以接着放").arg(placedName));
}

void DjiCloudMapClient::beginEdit(const QString& id)
{
    CloudMapElement* element = _findById(id);
    if (!element) {
        _setStatus(tr("找不到该元素"));
        return;
    }
    if (_editing && (_editing != element)) {
        cancelEdit();
        element = _findById(id);   // cancelEdit 可能把本机草稿删掉，重新取一次
        if (!element) {
            return;
        }
    }

    _editing = element;
    _editingOriginal = element->coordinates();   // 取消编辑时还原用
    element->setEditing(true);
    element->setDirty(true);
    emit editingChanged();
    _setStatus(element->type() == TypePoint ? tr("编辑端点：可改名、改颜色")
                                            : tr("编辑元素：拖动顶点，或点击「开始标绘」重新画"));
}

void DjiCloudMapClient::cancelEdit()
{
    CloudMapElement* element = _editing;
    if (!element) {
        return;
    }

    element->setEditing(false);
    _editing = nullptr;
    emit editingChanged();

    if (element->fromCloud()) {
        // 平台元素：丢弃改动，几何还原到进入编辑时的快照（本机操作，不用等刷新）
        element->setCoordinates(_editingOriginal);
        element->setDirty(false);
    } else {
        // 本机草稿：直接丢掉
        _removeElement(element);
    }
    _editingOriginal.clear();
    _setStatus(tr("已取消编辑"));
}

void DjiCloudMapClient::saveEdit(const QString& name, const QString& color)
{
    CloudMapElement* element = _editing;
    if (!element) {
        return;
    }

    // 端点只要有坐标就能存（后端 Point 也是单点）
    const int minVertices = (element->type() == TypeArea) ? 3
                          : (element->type() == TypePoint ? 1 : 2);
    if (element->vertexCount() < minVertices) {
        _setStatus(tr("至少需要 %1 个顶点").arg(minVertices));
        return;
    }

    const QString trimmed = name.trimmed();
    element->setName(trimmed.isEmpty() ? element->name() : trimmed);
    element->setColor(color);

    const QString workspaceId = _workspaceId();
    if (workspaceId.isEmpty()) {
        _setStatus(tr("保存失败：workspaceId 为空"));
        return;
    }

    if (element->fromCloud()) {
        // PUT /map/api/v1/workspaces/{ws}/elements/{id}
        QJsonObject body;
        body[QStringLiteral("name")] = element->name();
        body[QStringLiteral("content")] = _elementContent(element, element->color());
        _sendRequest("PUT",
                     QStringLiteral("/map/api/v1/workspaces/%1/elements/%2").arg(workspaceId, element->id()),
                     body, tr("更新元素"));
    } else if (!_postNewElement(element)) {
        // 连发都没发出去（缺 workspaceId / 共享图层）：保持编辑态，草稿不丢
        return;
    }

    element->setDirty(false);
    element->setEditing(false);
    element->setTracing(false);
    _editing = nullptr;
    _editingOriginal.clear();
    emit editingChanged();
}

bool DjiCloudMapClient::_postNewElement(CloudMapElement* element)
{
    // POST /map/api/v1/workspaces/{ws}/element-groups/{group_id}/elements
    const QString workspaceId = _workspaceId();
    if (workspaceId.isEmpty()) {
        _setStatus(tr("保存失败：workspaceId 为空"));
        return false;
    }
    if (_sharedGroupId.isEmpty()) {
        _setStatus(tr("保存失败：工作空间没有 APP 共享图层（type=2），先在平台建一个"));
        return false;
    }

    QJsonObject resource;
    resource[QStringLiteral("type")] = element->type();
    resource[QStringLiteral("user_name")] = QStringLiteral("pilot");
    resource[QStringLiteral("content")] = _elementContent(element, element->color());

    QJsonObject body;
    body[QStringLiteral("id")] = element->id();
    body[QStringLiteral("name")] = element->name();
    body[QStringLiteral("resource")] = resource;
    // fromCloud 必须等平台确认后再置位：POST 失败时它还是本机草稿，
    // 否则下一次 refresh 会把它当成"平台上已删除"清掉。用 QPointer 防止期间元素被删。
    QPointer<CloudMapElement> guarded(element);
    _sendRequest("POST",
                 QStringLiteral("/map/api/v1/workspaces/%1/element-groups/%2/elements")
                     .arg(workspaceId, _sharedGroupId),
                 body, tr("新建元素"), [guarded]() {
                     if (guarded) {
                         guarded->setFromCloud(true);   // 已在平台，之后改动走 PUT
                     }
                 });
    return true;
}

void DjiCloudMapClient::removeElement(const QString& id)
{
    CloudMapElement* element = _findById(id);
    if (!element) {
        return;
    }

    const QString name = element->name();
    const bool onCloud = element->fromCloud();
    _removeElement(element);

    if (!onCloud) {
        _setStatus(tr("已删除草稿：%1").arg(name));
        return;
    }

    const QString workspaceId = _workspaceId();
    if (workspaceId.isEmpty()) {
        _setStatus(tr("删除失败：workspaceId 为空"));
        return;
    }
    _sendRequest("DELETE",
                 QStringLiteral("/map/api/v1/workspaces/%1/elements/%2").arg(workspaceId, id),
                 {}, tr("删除元素"));
}

// ---------------------------------------------------------------------------
// 飞行区域（只读图层）
// ---------------------------------------------------------------------------

int DjiCloudMapClient::taskAreaCount() const
{
    int count = 0;
    for (const FlightAreaData& area : _flightAreaData) {
        if (area.type != QStringLiteral("nfz")) {
            ++count;
        }
    }
    return count;
}

int DjiCloudMapClient::geoZoneCount() const
{
    int count = 0;
    for (const FlightAreaData& area : _flightAreaData) {
        if (area.type == QStringLiteral("nfz")) {
            ++count;
        }
    }
    return count;
}

void DjiCloudMapClient::_setFlightAreaStatus(const QString& status)
{
    if (_flightAreaStatus == status) {
        return;
    }
    _flightAreaStatus = status;
    emit flightAreasChanged();
}

void DjiCloudMapClient::toggleTaskAreas()
{
    _showTaskAreas = !_showTaskAreas;

    if (_showTaskAreas && _flightAreaData.isEmpty()) {
        refreshFlightAreas();   // 第一次打开才去拉；拉取结果里会带上状态行
        return;
    }

    _rebuildFlightAreaModel();
    _setFlightAreaStatus(_showTaskAreas ? tr("任务区域：%1 个").arg(taskAreaCount())
                                        : tr("已隐藏任务区域"));
}

void DjiCloudMapClient::toggleGeoZones()
{
    _showGeoZones = !_showGeoZones;

    if (_showGeoZones && _flightAreaData.isEmpty()) {
        refreshFlightAreas();
        return;
    }

    _rebuildFlightAreaModel();
    _setFlightAreaStatus(_showGeoZones ? tr("GEO 区域：%1 个").arg(geoZoneCount())
                                       : tr("已隐藏 GEO 区域"));
}

void DjiCloudMapClient::refreshFlightAreas()
{
    const QString workspaceId = _workspaceId();
    if (workspaceId.isEmpty()) {
        _setFlightAreaStatus(tr("拉取任务区域失败：workspaceId 为空"));
        qWarning() << "[云飞行区域] workspaceId 为空，跳过拉取";
        return;
    }

    const QString token = _token();
    if (token.isEmpty()) {
        _setFlightAreaStatus(tr("拉取任务区域失败：缺少 serverToken"));
        return;
    }
    if (_flightAreaRefreshInFlight) {
        return;   // 首连与手动点按钮撞在一起时不重复发
    }
    _flightAreaRefreshInFlight = true;

    QNetworkRequest request(
        _apiUrl(QStringLiteral("/map/api/v1/workspaces/%1/flight-areas").arg(workspaceId)));
    request.setRawHeader("x-auth-token", token.toUtf8());
    request.setTransferTimeout(ReplyTimeoutMs);

    const quint64 generation = _generation;
    QNetworkReply* reply = _net->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation]() {
        reply->deleteLater();
        _flightAreaRefreshInFlight = false;

        if (generation != _generation) {
            return;   // 期间 clearAll() 过，这次结果作废
        }

        const QByteArray data = reply->readAll();
        const QString error = replyErrorText(reply);
        if (!error.isEmpty()) {
            qWarning() << "[云飞行区域] 拉取失败：" << error;
            _setFlightAreaStatus(tr("拉取任务区域失败：%1").arg(error));
            return;
        }

        const QJsonObject result = QJsonDocument::fromJson(data).object();
        const int code = result.value(QStringLiteral("code")).toInt(-1);
        if (code != 0) {
            const QString message = result.value(QStringLiteral("message")).toString();
            qWarning() << "[云飞行区域] 拉取被平台拒绝：" << code << message;
            _setFlightAreaStatus(tr("拉取任务区域失败：%1").arg(message));
            return;
        }

        _applyFlightAreas(result.value(QStringLiteral("data")).toArray());
    });
}

void DjiCloudMapClient::_applyFlightAreas(const QJsonArray& areas)
{
    // 全量替换。飞行区域是只读的，本地不会有草稿，所以不搞增量合并：
    // 增量改模型会让 QML 侧拿着过期 index 回调回来（地图元素那边踩过这个坑，
    // 命中的是 QList::operator[] 断言）。整表换掉，QML 只看到一次"全换"。
    _flightAreaData.clear();
    int skippedCircles = 0;
    int skippedOtherGeometry = 0;
    int skippedDegenerate = 0;

    for (const QJsonValue& value : areas) {
        const QJsonObject area = value.toObject();

        // GET 返回的字段名是 area_id；容错一下 id（POST body 用的才是 id）
        QString id = area.value(QStringLiteral("area_id")).toString();
        if (id.isEmpty()) {
            id = area.value(QStringLiteral("id")).toString();
        }
        if (id.isEmpty()) {
            continue;
        }

        const QJsonObject geometry = jsonObjectOrEncoded(area.value(QStringLiteral("content")))
                                         .value(QStringLiteral("geometry")).toObject();
        const QString geometryType = geometry.value(QStringLiteral("type")).toString();
        if (geometryType != QStringLiteral("Polygon")) {
            if (geometryType == QStringLiteral("Circle")) {
                ++skippedCircles;   // Circle：坐标是"圆心 + radius"，QGC 没有圆图元，本次只显示面
            } else {
                // 空字符串多半是 content 没解析出来（结构不对），单独记一笔，
                // 别把它当成"平台上有圆"而掩盖掉真正的解析问题
                ++skippedOtherGeometry;
                qWarning() << "[云飞行区域] 几何类型不是 Polygon，跳过：name ="
                           << area.value(QStringLiteral("name")).toString()
                           << "geometry.type =" << geometryType
                           << "content 是对象 =" << area.value(QStringLiteral("content")).isObject()
                           << "id =" << id;
            }
            continue;
        }

        const QList<QGeoCoordinate> ring = parseCoordinates(geometry, true, "[云飞行区域]");
        if (ring.size() < 3) {
            ++skippedDegenerate;   // 具体原因 parseCoordinates 里已经记过日志
            continue;
        }

        FlightAreaData parsed;
        parsed.id = id;
        parsed.name = area.value(QStringLiteral("name")).toString();
        parsed.type = area.value(QStringLiteral("type")).toString();
        parsed.enabled = area.value(QStringLiteral("status")).toBool();
        parsed.ring = ring;
        _flightAreaData.append(parsed);

        // 一行一条，和地图元素一样的对账方式：拿"原始首点"和"落图首点"比，判断纠偏方向
        qInfo() << "[云飞行区域] 区域" << (parsed.type == QStringLiteral("nfz") ? "GEO/禁飞" : "任务")
                << "name =" << parsed.name << "启用 =" << parsed.enabled
                << "顶点数 =" << ring.size() << "原始首点 =" << firstRawCoordinate(area)
                << "落图首点 =" << ring.first() << "id =" << id;
    }

    qInfo() << "[云飞行区域] 拉取完成：任务区域" << taskAreaCount() << "个，GEO 区域" << geoZoneCount()
            << "个；跳过圆" << skippedCircles << "个、其他几何" << skippedOtherGeometry
            << "个、退化面" << skippedDegenerate
            << "个，平台共返回" << areas.size() << "条";
    _setFlightAreaStatus(tr("任务区域 %1 个 / GEO 区域 %2 个（另有 %3 个圆形区域暂不显示）")
                             .arg(taskAreaCount()).arg(geoZoneCount()).arg(skippedCircles));

    // 注意：这里不按显示开关判断"要不要重建"—— 开关都关着时重建出来的是空模型，
    // 正好把上一次的图形收掉（refresh 期间用户可能已经把图层关了）。
    _rebuildFlightAreaModel();
}

void DjiCloudMapClient::_rebuildFlightAreaModel()
{
    QObjectList built;
    for (const FlightAreaData& area : _flightAreaData) {
        const bool geoZone = (area.type == QStringLiteral("nfz"));
        if (geoZone ? !_showGeoZones : !_showTaskAreas) {
            continue;
        }
        built.append(new CloudFlightArea(area.id, area.name, area.type, area.enabled, area.ring, this));
    }

    // swapObjectList 只做一次 beginReset/endReset，不会在 reset 中间夹 beginInsertRows
    // （模型那套 insertRows 是给增量用的，套在 reset 里信号顺序是错的）。
    // 旧条目不马上 delete —— visuals 的 delegate 可能还在析构路径上碰它的几何，
    // 跟 clearAndDeleteContents 一样交给事件循环。
    const QObjectList old = _flightAreas->swapObjectList(built);
    for (QObject* object : old) {
        object->deleteLater();
    }

    emit flightAreasChanged();
}

void DjiCloudMapClient::onWsFlightArea(const QString& bizCode, const QJsonObject& data)
{
    if (bizCode != QStringLiteral("flight_areas_update")) {
        return;
    }

    // 推送只给"这一条"区域（operation + area_id），而本地是只读全量图层：
    // 不做增量手术，直接整表重拉，和 map_group_refresh 一个思路。
    qInfo() << "[云飞行区域] flight_areas_update：operation ="
            << data.value(QStringLiteral("operation")).toString()
            << "area_id =" << data.value(QStringLiteral("area_id")).toString()
            << "→ 重新拉取列表";
    refreshFlightAreas();
}
