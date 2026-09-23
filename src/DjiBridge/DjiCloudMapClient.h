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
#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QString>
#include <QtCore/QUrl>
#include <QtPositioning/QGeoCoordinate>

#include <functional>

#include "QmlObjectListModel.h"

class QNetworkAccessManager;
class QGCMapPolygon;
class QGCMapPolyline;

// 日志用默认类别（qInfo/qWarning），与本目录其他文件一致：
// QGCLogging::msgHandler 会把没有开启 debug 的自定义类别整条丢掉，
// 用 QGC_LOGGING_CATEGORY 建类别反而默认一条日志都看不到。

/// 云平台地图元素（Pilot 地图标注）
///
/// 对应后端 ElementResource，三种都在画（resource.type 0=Point 1=LineString 2=Polygon）。
/// Point 就是官方控制台上那个**菱形图钉**：use-g-map-cover.ts 的 init2DPin() 用
/// pin-<color>.svg 画出来，那个 svg 的路径正好是个菱形，所以地面站也画菱形。
/// 线/面用 QGC 现成的 QGCMapPolyline / QGCMapPolygon 承载，这样地图上显示与编辑
/// 共用同一个对象（QGCMapPolylineVisuals / QGCMapPolygonVisuals 就是按这两个类写的）；
/// Point 只有一个坐标，没有对应的 QGCMap* 类，直接存 QGeoCoordinate。
class CloudMapElement : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString  id         READ id         NOTIFY idChanged)
    Q_PROPERTY(QString  name       READ name       NOTIFY nameChanged)
    Q_PROPERTY(int      type       READ type       CONSTANT)      ///< 0=Point 1=LineString 2=Polygon
    Q_PROPERTY(QString  color      READ color      WRITE setColor  NOTIFY colorChanged)
    /// true=平台下发；false=本机新建（列表里加标记，也是防推送覆盖几何的依据）
    Q_PROPERTY(bool     fromCloud  READ fromCloud  NOTIFY fromCloudChanged)
    /// 有未保存改动（正在编辑）
    Q_PROPERTY(bool     dirty      READ dirty      NOTIFY dirtyChanged)
    /// QGCMapPolyline* 或 QGCMapPolygon*（按 type），Point 时是 null。
    /// 声明成 QObject* 是因为 QGCMapPolyline 没有注册到 QML
    /// （QML 侧按 var 动态访问 path/traceMode 等方法即可）
    Q_PROPERTY(QObject* geometry   READ geometry   CONSTANT)
    /// type==0 时的坐标（QML 里画菱形用）；其余类型返回无效坐标
    Q_PROPERTY(QGeoCoordinate coordinate READ coordinate NOTIFY geometryChanged)
    /// 顶点数（列表里显示，也用来判断能不能保存）；Point 有坐标时算 1
    Q_PROPERTY(int      vertexCount READ vertexCount NOTIFY geometryChanged)
    /// 编辑中：写入时同时把底层几何的 interactive 置位，驱动 visuals 出拖拽手柄
    Q_PROPERTY(bool     editing    READ editing    NOTIFY editingChanged)
    /// 标绘中（点地图加点）。端点没有几何可"标"，这个标志对 type==0 的含义是
    /// **"等一次地图点击放点"**（placePoint 由地图图层在点击时调用）
    Q_PROPERTY(bool     tracing    READ tracing    NOTIFY tracingChanged)

public:
    CloudMapElement(int type, const QString& id, const QString& name,
                    const QString& color, QObject* parent = nullptr);

    QString  id() const { return _id; }
    QString  name() const { return _name; }
    int      type() const { return _type; }
    QString  color() const { return _color; }
    bool     fromCloud() const { return _fromCloud; }
    bool     dirty() const { return _dirty; }
    QObject* geometry() const;
    QGeoCoordinate coordinate() const { return _point; }
    int      vertexCount() const;
    bool     editing() const { return _editing; }
    bool     tracing() const { return _tracing; }

    void setName(const QString& name);
    void setColor(const QString& color);
    void setId(const QString& id);
    /// 平台下发（refresh / ws 推送）时置 true，列表里区分"平台画的"和"本机在画的"
    void setFromCloud(bool fromCloud);
    void setDirty(bool dirty);
    void setEditing(bool editing);
    /// 标绘模式：开着的时候点地图会往几何里加顶点
    Q_INVOKABLE void setTracing(bool tracing);

    /// 用一组 WGS84 坐标整体替换几何（refresh / ws 推送 / 编辑保存回填都用它）
    void setCoordinates(const QList<QGeoCoordinate>& coordinates);
    /// 当前几何的 WGS84 顶点（POST/PUT 时序列化）
    QList<QGeoCoordinate> coordinates() const;
    bool isEmpty() const;

signals:
    void idChanged();
    void nameChanged();
    void colorChanged();
    void fromCloudChanged();
    void dirtyChanged();
    void geometryChanged();
    void editingChanged();
    void tracingChanged();

private:
    QString _id;
    QString _name;
    const int _type;
    QString _color;
    bool    _fromCloud = false;
    bool    _dirty = false;
    bool    _editing = false;
    bool    _tracing = false;
    QGCMapPolyline* _polyline = nullptr;   ///< type==1 时非空
    QGCMapPolygon*  _polygon = nullptr;    ///< type==2 时非空
    QGeoCoordinate  _point;                ///< type==0 时是坐标（菱形图钉），其余类型无效
};

/// 云平台飞行区域（后端 flight-area，界面上叫「任务区域」和「GEO 区域」）
///
/// 和地图元素是**两套完全不同的后端资源**：地图元素走 element-groups（点/线/面，可编辑、
/// 可自定义颜色），飞行区域走 flight-areas（只有面和圆、只有启用/停用、按类型固定配色）。
/// 官方 Web 控制台把它们也画在同一张地图上，所以这里合成一份只读图层。
///
/// type 与界面名的对应关系直接抄官方控制台（types/flight-area.ts 的 FlightAreaTypeTitleMap）：
///   dfence —— "Task Area"，即「任务区域」，绿 #19BE6B
///   nfz    —— "GEO Zone"，即「GEO 区域」（禁飞区），红 #FF0000，且启用时带半透明填充
/// 未启用的区域官方控制台一律画成灰色 #B3B3B3。
///
/// 只读：不接编辑，也不参与上行。QGCMapPolygon 默认 interactive 就是 false。
class CloudFlightArea : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString  id          READ id          NOTIFY idChanged)
    Q_PROPERTY(QString  name        READ name        NOTIFY nameChanged)
    /// 后端的原始 type 字符串："dfence" / "nfz"
    Q_PROPERTY(QString  type        READ type        NOTIFY typeChanged)
    /// 平台上的启用状态（后端字段就叫 status）。停用的区域画成灰色。
    Q_PROPERTY(bool     enabled     READ enabled     NOTIFY enabledChanged)
    /// 是不是禁飞/GEO 区域（QML 里判断要不要给填充色时用，比比字符串干净）
    Q_PROPERTY(bool     geoZone     READ geoZone     NOTIFY typeChanged)
    /// 显示色，由 type + enabled 算出来（官方控制台配色）
    Q_PROPERTY(QString  color       READ color       NOTIFY colorChanged)
    /// QGCMapPolygon*，声明成 QObject* 的原因同 CloudMapElement::geometry
    Q_PROPERTY(QObject* geometry    READ geometry    CONSTANT)
    Q_PROPERTY(int      vertexCount READ vertexCount NOTIFY geometryChanged)

public:
    CloudFlightArea(const QString& id, const QString& name, const QString& type, bool enabled,
                    const QList<QGeoCoordinate>& ring, QObject* parent = nullptr);

    QString id() const { return _id; }
    QString name() const { return _name; }
    QString type() const { return _type; }
    bool    enabled() const { return _enabled; }
    bool    geoZone() const { return _type == QStringLiteral("nfz"); }
    QString color() const;
    /// 定义在 .cc 里：本文件只前向声明了 QGCMapPolygon，写在这里编译器不知道它继承自 QObject
    QObject* geometry() const;
    int      vertexCount() const;

signals:
    void idChanged();
    void nameChanged();
    void typeChanged();
    void enabledChanged();
    void colorChanged();
    void geometryChanged();

private:
    QString _id;
    QString _name;
    QString _type;
    bool    _enabled = false;
    QGCMapPolygon* _polygon = nullptr;
};

/// DJI 上云 API —— 地图元素客户端
///
/// 两条链路合到一个类里：
///   - 下行 ws：DjiWsClient 把 map_element_create / update / delete / map_group_refresh
///     转发进来。create/update 的推送**自带完整几何**，直接画；delete 只给 id。
///     map_group_refresh 只给 group_id，按协议必须再走 HTTP 拉一次全量列表。
///   - 下行/上行 HTTP：GET element-groups 拉全量（首连 + refresh）、POST 新建、
///     PUT 更新、DELETE 删除。base URL 由 websocketUrl 推导（Web 端自己的 baseURL
///     与 websocketURL 就是同 host 同端口），鉴权头 x-auth-token。
///
/// 上行固定写进 type==2 的 APP 共享图层（后端 GroupTypeEnum 注释：Pilot 默认往这里加）。
///
/// 另有**只读**的飞行区域图层（flight-areas，见 CloudFlightArea），和元素共用同一套
/// HTTP/鉴权设施，但模型、状态、显示开关都是独立的，不影响元素那条链路。
///
/// 坐标：后端图形按 GCJ-02（高德）存时，进来 gcj02->wgs84、出去 wgs84->gcj02，
/// 由 CloudServer 的 coordinateTransform 开关控制（默认开）。详见 QGCGeoGcj02.h。
class DjiCloudMapClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QmlObjectListModel* lines   READ lines   CONSTANT)
    Q_PROPERTY(QmlObjectListModel* areas   READ areas   CONSTANT)
    /// 端点（Point）元素：地图上画成菱形图钉，列表里单独一段
    Q_PROPERTY(QmlObjectListModel* points  READ points  CONSTANT)
    /// 当前编辑中的元素（新建或编辑），null 表示不在编辑态。类型是 CloudMapElement*
    /// 但声明成 QObject*，QML 侧按 object.xxx 动态访问，不需要注册类型
    Q_PROPERTY(QObject* editing READ editing NOTIFY editingChanged)
    Q_PROPERTY(int     count  READ count  NOTIFY countChanged)
    /// 最近一次操作结果，给界面显示用（"已同步 3 个元素" / 失败原因）
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)

    // —— 飞行区域（只读）——
    Q_PROPERTY(QmlObjectListModel* flightAreas READ flightAreas CONSTANT)
    Q_PROPERTY(int     flightAreaCount READ flightAreaCount NOTIFY flightAreasChanged)
    Q_PROPERTY(int     taskAreaCount   READ taskAreaCount   NOTIFY flightAreasChanged)
    Q_PROPERTY(int     geoZoneCount    READ geoZoneCount    NOTIFY flightAreasChanged)
    /// 「任务区域」「GEO 区域」两个图层的显示开关（图标列上的两个按钮切的就是它们）
    Q_PROPERTY(bool    showTaskAreas   READ showTaskAreas   NOTIFY flightAreasChanged)
    Q_PROPERTY(bool    showGeoZones    READ showGeoZones    NOTIFY flightAreasChanged)
    /// 飞行区域自己的状态行：和元素的 status 分开，免得一个 404 把元素的状态顶掉
    Q_PROPERTY(QString flightAreaStatus READ flightAreaStatus NOTIFY flightAreasChanged)

public:
    explicit DjiCloudMapClient(QObject* parent = nullptr);
    ~DjiCloudMapClient() override;

    QmlObjectListModel* lines() const { return _lines; }
    QmlObjectListModel* areas() const { return _areas; }
    QmlObjectListModel* points() const { return _points; }
    QObject* editing() const;
    int      count() const;
    QString  status() const { return _status; }

    QmlObjectListModel* flightAreas() const { return _flightAreas; }
    /// 平台上（能画成面的）区域总数，与显示开关无关；模型里只放当前要画的那些
    int      flightAreaCount() const { return int(_flightAreaData.size()); }
    int      taskAreaCount() const;
    int      geoZoneCount() const;
    bool     showTaskAreas() const { return _showTaskAreas; }
    bool     showGeoZones() const { return _showGeoZones; }
    QString  flightAreaStatus() const { return _flightAreaStatus; }

    /// GET element-groups 拉全量（首连、map_group_refresh、手动刷新都走这里）
    Q_INVOKABLE void refresh();
    /// 新建本机草稿：type 0=端点 1=线段 2=区域
    Q_INVOKABLE void beginCreate(int type);
    /// 地图上点了一下（由 CloudElementMapLayer 的点击处理调用）：
    ///   - 本机草稿 —— **连续放置**：放下、立刻 POST 上云、再准备下一个空草稿，
    ///     所以在地图上连点就连出一串端点，不用每个都回面板按「保存」
    ///   - 平台上的端点（「在地图上重新定位」）—— 只是挪位置，等「保存到平台」走 PUT
    Q_INVOKABLE void placePoint(double latitude, double longitude);
    /// 编辑平台已有元素
    Q_INVOKABLE void beginEdit(const QString& id);
    /// 放弃编辑（新建的草稿会被丢掉）
    Q_INVOKABLE void cancelEdit();
    /// 保存编辑中的元素：平台下发的走 PUT，本机新建的走 POST
    Q_INVOKABLE void saveEdit(const QString& name, const QString& color);
    Q_INVOKABLE void removeElement(const QString& id);
    /// 断开上云 / 退出时清空（元素属于平台，本地不留）
    Q_INVOKABLE void clearAll();

    /// GET flight-areas 拉全量（首连与「刷新」都走这里）
    Q_INVOKABLE void refreshFlightAreas();
    /// 开/关「任务区域」「GEO 区域」图层；首次打开时自动去拉一次
    Q_INVOKABLE void toggleTaskAreas();
    Q_INVOKABLE void toggleGeoZones();

public slots:
    /// DjiWsClient 转发进来的 map_element_* / map_group_refresh
    void onWsMapElement(const QString& bizCode, const QJsonObject& data);
    /// DjiWsClient 转发进来的 flight_areas_update
    void onWsFlightArea(const QString& bizCode, const QJsonObject& data);

signals:
    void countChanged();
    void editingChanged();
    void statusChanged();
    /// 飞行区域的模型/数量/开关/状态行，任意一项变了都发这个（更新频率很低，不值得细分）
    void flightAreasChanged();

private:
    void _setStatus(const QString& status);
    void _setFlightAreaStatus(const QString& status);

    QUrl _apiUrl(const QString& path) const;
    QString _token() const;
    QString _workspaceId() const;

    /// POST / PUT 的 body（resource.content 结构）
    QJsonObject _elementContent(const CloudMapElement* element, const QString& color) const;
    static QJsonArray _coordinatesToJson(const QList<QGeoCoordinate>& coordinates, int type);

    /// 解析 resource（ws 推送与 GET 返回的是同一结构）
    /// @return type（0/1/2），不认识的返回 -1
    static int _resourceType(const QJsonObject& resource);
    static QString _resourceColor(const QJsonObject& resource);
    /// Point 返回单点列表（0 或 1 个元素），线/面返回顶点列表
    static QList<QGeoCoordinate> _resourceCoordinates(const QJsonObject& resource, int type);
    /// 0/1/2 都支持（Point 也画），其余（Circle 等新几何）跳过
    static bool _isSupportedType(int type) { return type == TypePoint || type == TypeLine || type == TypeArea; }
    /// 日志用的类型名：端点/线段/区域
    static QString _typeText(int type);

    CloudMapElement* _findById(const QString& id) const;
    /// 元素该进哪个模型：0=点 1=线 2=面
    QmlObjectListModel* _modelForType(int type) const;
    CloudMapElement* _applyResource(const QString& id, const QString& name,
                                    const QJsonObject& resource, bool fromCloud);
    /// 建一个本机草稿并选中它（beginCreate 与端点的连续放置共用）
    CloudMapElement* _startDraft(int type);
    /// 把本机草稿 POST 上云（saveEdit 的「新建」分支与端点连续放置共用）。
    /// @return false = 连请求都没发出去（缺 workspaceId / APP 共享图层），元素仍是本机草稿
    bool _postNewElement(CloudMapElement* element);
    void _removeElement(CloudMapElement* element);
    void _resetAll();

    /// 写操作（POST / PUT / DELETE）；GET 走 refresh() 自己的回调，不共用。
    /// onSuccess 在平台确认成功（HTTP 2xx 且 code == 0）后调用，失败不调。
    void _sendRequest(const QByteArray& verb, const QString& path, const QJsonObject& body,
                      const QString& opText, const std::function<void()>& onSuccess = {});
    /// GET element-groups 返回的 data（组数组）合并进模型
    void _applyGroups(const QJsonArray& groups);
    /// GET flight-areas 返回的 data（区域数组）：解析进 _flightAreaData，再按开关重建模型
    void _applyFlightAreas(const QJsonArray& areas);
    /// 按 _showTaskAreas / _showGeoZones 把 _flightAreaData 里该显示的挑进模型。
    /// 过滤放在 C++ 而不是 QML 里：visuals 的 MapPolygon 是在 mapControl 上命令式创建的，
    /// 给 delegate 的 Item 设 visible 根本藏不住它，只有"不进模型"才真的不画。
    void _rebuildFlightAreaModel();

    static constexpr int TypePoint = 0;
    static constexpr int TypeLine  = 1;
    static constexpr int TypeArea  = 2;
    static constexpr int SharedGroupType = 2;   ///< GroupTypeEnum.SHARED：Pilot 的默认落点

    QNetworkAccessManager* _net = nullptr;

    QmlObjectListModel* _lines = nullptr;
    QmlObjectListModel* _areas = nullptr;
    QmlObjectListModel* _points = nullptr;
    QHash<QString, CloudMapElement*> _byId;

    CloudMapElement* _editing = nullptr;
    /// 进入编辑时的几何快照：取消编辑时用它把平台元素还原回去（本机操作，不用等刷新）
    QList<QGeoCoordinate> _editingOriginal;
    QString _sharedGroupId;    ///< type==2 的共享图层 id（GET 时缓存，上行 POST 用）
    QString _status;
    bool    _refreshInFlight = false;
    /// clearAll() 一清就把代数 +1；在途的 GET 回来时代数对不上就丢弃结果，
    /// 否则断开上云后一个慢响应会把刚清空的元素重新灌回地图
    quint64 _generation = 0;

    // —— 飞行区域（只读图层）——
    /// 从平台解析出来的一条区域，未经显示开关过滤
    struct FlightAreaData
    {
        QString id;
        QString name;
        QString type;      ///< 后端原始值："dfence"（任务区域）/ "nfz"（GEO 区域）
        bool    enabled = false;
        QList<QGeoCoordinate> ring;
    };

    QmlObjectListModel* _flightAreas = nullptr;   ///< 只放"当前该画"的，见 _rebuildFlightAreaModel
    QList<FlightAreaData> _flightAreaData;        ///< 平台上的全部（面），两个开关在这一层筛
    QString _flightAreaStatus;
    bool    _showTaskAreas = false;   ///< 两个图层默认都关着，用户点了才显示
    bool    _showGeoZones = false;
    bool    _flightAreaRefreshInFlight = false;
};
