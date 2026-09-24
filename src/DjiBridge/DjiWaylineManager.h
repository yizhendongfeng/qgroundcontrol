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
#include <QtCore/QObject>
#include <QtCore/QSet>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QUrl>
#include <QtCore/QVariantList>
#include <QtCore/QVariantMap>

#include <functional>

class DjiWaylineListModel;
class QNetworkAccessManager;
class QNetworkReply;

/// DJI 云「航线管理」：列表 / 收藏 / 上传 / 下载 / 重命名 / 删除，
/// 外加本地 .plan 与 DJI .kmz 之间的双向转换。
///
/// 列表数据直接来自 GET /wayline/api/v1/workspaces/{ws}/waylines，
/// 交给 DjiWaylineListModel 承载，QML 侧以 listModel 为数据源。
///
/// 机型和负载的枚举请用 M3E 一套：drone_model_key = "0-77-0"、
/// payload_model_keys = ["1-66-0"]。DJI 文档里 M30 是 67/52，
/// 用错会被后台按「机型不符」拒收。
class DjiWaylineManager : public QObject
{
    Q_OBJECT

    /// 类型刻意写成 QObject*（列表模型的具体类型不外露），QML 侧按运行时类型
    /// 当 model 用，角色名仍然取自 DjiWaylineListModel::roleNames()。
    Q_PROPERTY(QObject* listModel READ listModel CONSTANT)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    /// 单条航线预览（取 kmz + 解析）是不是还在跑。**与 busy 是两码事**：
    /// busy 会让整页（翻页/刷新/批量条/底部动作栏）一起置灰，点一行就把 busy 置起来
    /// 会让整页发灰；previewBusy 只表示「详情面板要的那条航线还没解析回来」
    Q_PROPERTY(bool previewBusy READ previewBusy NOTIFY previewBusyChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY errorOccurred)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY listRefreshed)
    Q_PROPERTY(int currentPage READ currentPage NOTIFY listRefreshed)
    Q_PROPERTY(int totalPages READ totalPages NOTIFY listRefreshed)
    Q_PROPERTY(int pageSize READ pageSize NOTIFY listRefreshed)

    /// 页面头那个 chip 上要显示的东西。都直接取自 CloudServerSettings，
    /// 挂在 listRefreshed 上是因为列表刷新时它们一定是当前值，够用。
    Q_PROPERTY(QString workspaceId   READ workspaceId   NOTIFY listRefreshed)
    Q_PROPERTY(QString serverAddress READ serverAddress NOTIFY listRefreshed)

public:
    explicit DjiWaylineManager(QObject* parent = nullptr);
    ~DjiWaylineManager() override;

    QObject* listModel() const;
    bool busy() const { return _busy; }
    bool previewBusy() const { return _previewBusy; }
    QString lastError() const { return _lastError; }
    int totalCount() const { return _totalCount; }
    int currentPage() const { return _currentPage; }
    int totalPages() const { return _totalPages; }
    int pageSize() const { return _pageSize; }
    QString workspaceId() const;
    /// "ip:6789"，和 makeRequest 拼出来的地址是同一个
    QString serverAddress() const;

    /// 拉取第一页；favoritedOnly 为 true 时只列收藏
    Q_INVOKABLE void refreshList(bool favoritedOnly = false);

    /// 带完整筛选条件的拉取。keyword 为空串表示不过滤；templateType < 0 表示不限。
    /// orderBy 是排序列，后台只认 name / update_time / create_time 三个值，
    /// 空串时用 kDefaultOrderBy（这列是必填的，空着不发后台直接报错）；
    /// orderDesc 为 true 表示倒序。
    Q_INVOKABLE void refreshListEx(int page, int pageSize, const QString& keyword,
                                   const QString& droneModelKey, const QString& payloadModelKey,
                                   int templateType, bool favoritedOnly,
                                   const QString& orderBy, bool orderDesc);

    /// 下载单条：取 {id}/url 得到的 kmz 落到 downloadDir()，
    /// 完成后发 downloadFinished(kmzPath)
    Q_INVOKABLE void downloadWayline(int row);
    /// 批量下载，rows 是模型行号列表（QVariantList 方便 QML 传数组）
    Q_INVOKABLE void downloadWaylines(const QVariantList& rows);
    /// 收藏/取消收藏。ids 是航线 id 列表，favorite 为 true 是收藏、false 是取消
    Q_INVOKABLE void collectWaylines(const QStringList& ids, bool favorite);

    /// 查重名：后台返回的重名清单通过 duplicateNamesChecked 回传。
    /// 注意后台是「按前缀查」——传 "巡检" 会把 "巡检01" 也算重名。
    Q_INVOKABLE void checkDuplicateNames(const QString& name);

    /// 上传本地 kmz。name 为待上传的 .kmz 名（不含扩展名或含均可，内部会归一），
    /// finalName 为空则用 name；重名时 QML 侧可传一个 finalName 规避。
    Q_INVOKABLE void uploadWayline(const QString& name, const QString& finalName);

    Q_INVOKABLE void deleteWayline(int row);
    /// 批量删除，rows 是模型行号列表（QVariantList 方便 QML 传数组）。
    /// 后台没有批量删除接口，这里按 **id** 一条条删（不能按行号 —— 删完会重拉列表，
    /// 行号在删除过程中就变了），全部走完再刷新一次列表。
    /// 后台不支持 DELETE 时，和单条删除同一套降级：把删不掉的那几条取消收藏
    Q_INVOKABLE void deleteWaylines(const QVariantList& rows);
    Q_INVOKABLE void renameWayline(int row, const QString& newName);

    /// 取第 row 条航线的 kmz 下来解析，结果通过 waylinePreviewReady 回传。
    /// 列表接口给不出航点数/起点/长度/航迹，只能解析文件。
    /// 同一条只解析一次（结果和失败都进缓存），重复调用直接回缓存。
    /// 刻意**不动 busy** —— 见 previewBusy 的说明
    Q_INVOKABLE void requestWaylinePreview(int row);

    /// 本地 .plan -> DJI .kmz（含 template.kml + waylines.wpml）。
    /// 失败返回 false，原因见 lastConvertError()。
    Q_INVOKABLE bool convertPlanToKmz(const QString& planPath, const QString& kmzPath);
    /// 本地 .plan -> 单个 .kml 文件（只出 template 那层，给预览用）
    Q_INVOKABLE bool convertPlanToKml(const QString& planPath, const QString& kmlPath);
    /// DJI .kmz -> 本地 .plan，下载后的 kmz 直接转成 QGC 能打开的文件
    Q_INVOKABLE bool convertKmzToPlan(const QString& kmzPath, const QString& planPath);

    /// 下载目录（<任务目录>/DjiWaylines），不存在时自动建
    Q_INVOKABLE QString downloadDir() const;
    Q_INVOKABLE QString lastConvertError() const { return _lastConvertError; }

signals:
    void busyChanged();
    void previewBusyChanged();
    void errorOccurred(const QString& message);
    void listRefreshed(int totalCount, int currentPage);
    /// 某条航线的 kmz 解析结果。id 对不上的响应调用方应当丢弃（用户已经点到别的行了）。
    /// info 字段：id, ok, error,
    ///           waypointCount, lengthMeters, startLatitude, startLongitude,
    ///           minExecuteHeight, maxExecuteHeight,
    ///           points: [lat0, lon0, lat1, lon1, ...]（拍平的，见 .cc 里的说明），
    ///           bounds: [minLat, minLon, maxLat, maxLon]
    void waylinePreviewReady(const QString& id, const QVariantMap& info);
    void duplicateNamesChecked(const QStringList& duplicates);
    void downloadFinished(const QString& kmzPath);
    /// 一批下载全部跑完了。landed 是拿到的条数，failed 是失败的条数。
    ///
    /// **不要在 QML 侧按 downloadFinished 的累计条数 + errorOccurred 自己数**：
    /// errorOccurred 是全局的（上传失败、预览解析失败、重命名失败都发它），
    /// 批量下载期间任何一条别的错误都会把计数打乱 —— 要么提前汇总，要么永远
    /// 汇总不出来。整批的账只有发请求的这一侧算得准，所以在这里数
    void batchDownloadFinished(int landed, int failed);
    void uploadFinished(const QString& name);
    /// operation 是 "download" / "upload"，percent 为 0..100
    void transferProgress(const QString& operation, int percent);

private:
    /// 对象存储临时凭证（POST /storage/.../sts 的 data 字段）
    struct S3Credential {
        QString bucket;
        QString region;
        QString endpoint;
        QString objectKeyPrefix;
        QString accessKey;
        QString secretKey;
        QString sessionToken;
    };

    void _setBusy(bool busy);
    void _setPreviewBusy(bool busy);
    void _fail(const QString& message);
    void _applyListReply(QNetworkReply* reply, int requestedPage, int requestedPageSize);

    /// 预览文件（按 id 命名的临时 kmz）的落地目录，<任务目录>/DjiWaylineCache。
    /// **不能用 downloadDir()**：那是 <任务目录>/DjiWaylines，_resolveLocalKmz 会去那里
    /// 按文件名找上传源，预览文件混进去会被当成上传源
    QString _previewDir() const;

    /// 解析预览用的 kmz，把详情面板要填的字段写进 info。
    /// 返回空串表示成功，否则是错误原因
    QString _parseWaylinePreview(const QString& kmzPath, QVariantMap& info) const;

    /// 收口：解析、写缓存、清 previewBusy、发信号。
    /// kmzPath 为空或 error 非空表示取文件这一步就失败了
    void _finishPreview(const QString& id, const QString& kmzPath, const QString& error);

    /// GET {serverIp}:6789 + path，path 里已含查询串
    void _get(const QString& path, std::function<void(QNetworkReply*)> handler);

    /// 取一个"重定向即结果"的地址：后台的 .../waylines/{id}/url 不返回 JSON，
    /// 而是 302 到对象存储，下载地址在 Location 头里。详见 .cc 里的实现注释。
    /// onDone(下载地址, 失败原因)，失败时地址为空串。
    void _getRedirectLocation(const QString& path,
                              std::function<void(const QString&, const QString&)> onDone);

    /// 其它动词的通用发请求。body 为空时不带请求体。
    void _send(const QByteArray& verb, const QString& path, const QByteArray& body,
               const QString& contentType, std::function<void(QNetworkReply*)> handler);

    /// 取 STS 临时凭证；onDone(凭证是否可用)
    void _requestCredential(std::function<void(bool)> onDone);

    /// 用 V4 签名把数据 PUT 到对象存储。objectKey 是完整 key。
    void _putObject(const QString& objectKey, const QByteArray& data,
                    std::function<void(bool, const QString&)> onDone);

    /// 上传完成后向后台登记航线。objectKey 为对象存储里的 key。
    void _importWayline(const QString& objectKey, const QString& name,
                        std::function<void(bool, const QString&)> onDone);

    /// 把本地 kmz 推到对象存储并登记为 uploadName。uploadWayline 与
    /// renameWayline 共用。onDone(是否成功, 失败原因)
    void _uploadLocalFile(const QString& srcPath, const QString& uploadName,
                          std::function<void(bool, const QString&)> onDone);

    /// 下载远端 url 到 destPath。onDone(本地路径, 错误信息)，失败时路径为空串。
    void _downloadToFile(const QUrl& url, const QString& destPath,
                         std::function<void(const QString&, const QString&)> onDone);

    /// 把 name（可含/不含 .kmz 后缀，也可以是完整路径）解析成本地 kmz 文件路径；
    /// 找不到返回空串
    QString _resolveLocalKmz(const QString& name) const;

    /// 归一化上传名：去掉路径和 .kmz 后缀，并把云端不接受的字符换成 '-'
    /// （云端名字只允许 ^[^<>:"/|?*._\\]+$，详见 .cc 里的注释）
    static QString _normalizeName(const QString& name);

    /// 批量删除的递归步进：删第 index 个，全部走完后统一收尾（见 .cc 里的注释）。
    /// failed 是到目前为止删不掉的 id（后台没实现 DELETE 时就是全部）
    void _deleteByIds(const QStringList& ids, int index, const QStringList& failed);

    /// 批量下载期间，每一条有了结果（成功或失败）都来记一笔；账齐了就发
    /// batchDownloadFinished。不在批量里时什么都不做（单条下载也走 downloadWayline）
    void _noteDownloadOutcome(bool landed);

    /// collectWaylines 的实现体 + 一个"做完了"的回调。
    /// collectWaylines 是 Q_INVOKABLE，参数里不能出现 std::function
    /// （那样 QML 侧就找不到 2 参数的重载，行内那个「收藏/取消收藏」会直接失效），
    /// 所以批量删除需要等收藏请求落地时走这个内部版本
    void _collectWaylines(const QStringList& ids, bool favorite, std::function<void()> onDone);

    QNetworkAccessManager* _networkManager = nullptr;
    DjiWaylineListModel* _listModel = nullptr;

    S3Credential _credential;
    QString _minioBaseUrl;

    bool _busy = false;
    bool _previewBusy = false;

    /// 按航线 id 缓存的预览结果，**失败也缓存** —— 同一条反复点不该反复打后台
    QHash<QString, QVariantMap> _previewCache;
    /// 正在取/解析的 id，防重复请求
    QSet<QString> _previewInFlight;

    /// 批量下载的账。_batchDownloadActive 为 false 时这三个只是残留值，不用管
    bool _batchDownloadActive = false;
    int  _batchDownloadTotal  = 0;
    int  _batchDownloadLanded = 0;
    int  _batchDownloadFailed = 0;

    QString _lastError;
    QString _lastConvertError;
    int _totalCount = 0;
    int _currentPage = 1;
    int _totalPages = 1;
    int _pageSize = 20;

    // 上一次 refreshListEx 的筛选条件，翻页/删除后重拉时复用
    QString _keyword;
    QString _droneModelKey;
    QString _payloadModelKey;
    int _templateType = -1;
    bool _favoritedOnly = false;
    QString _orderBy;
    bool _orderDesc = true;
};
