/**
 *  从新吊舱文件服务器（http://{podIp}:8554/download/）查询并下载图片/视频
 *
 *  目录结构：
 *    /download/YYYY-MM-DD/photo/*.jpeg
 *    /download/YYYY-MM-DD/video/*.mp4 (+ 同名 .srt 字幕，忽略)
 *
 *  查询流程：
 *    1. GET 根目录，解析日期子目录列表
 *    2. 按用户输入的日期范围过滤
 *    3. 并发拉取每个日期目录下的 photo/、video/ 文件列表
 *    4. 组装模型；图片缩略图直接用 URL，视频缩略图 Range 拉前 1MB 后用 QMediaPlayer 截首帧
 *
 *  下载流程：串行下载选中文件，进度通过 DownloadProgressRole 上报。
 */
#ifndef MEDIADOWNLOAD_H
#define MEDIADOWNLOAD_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QAbstractListModel>
#include <QFile>
#include <QElapsedTimer>
#include <QUrl>
#include <QQueue>
#include <QSet>
#include <QString>
#include <QStringList>

class MediaDownload : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
public:
    enum Roles {
        FilePathRole = Qt::UserRole + 1,  // 完整下载 URL
        FileNameRole,                     // 纯文件名
        FileTypeRole,
        StartTimeRole,
        DurationRole,
        SizeRole,
        SizeStrRole,
        FileSelectedRole,
        DownloadProgressRole,
        IsVideoRole,
        ThumbnailUrlRole,
        ThumbReadyRole
    };

    struct PodFileInfo {
        QString filePath;       // 完整 URL，用于下载和图片缩略图
        QString fileName;       // 纯文件名（含扩展名）
        QString fileType;       // "photo" / "video"
        bool    isVideo = false;
        QString startTime;
        float   duration = 0;
        qint64  fileSize = 0;
        QString fileSizeStr;
        bool    selected = false;
        float   downloadProgress = 0;  // 0..1
        QUrl    thumbnailUrl;
        bool    thumbReady = false;
    };

    explicit MediaDownload(QObject* parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    QHash<int, QByteArray> roleNames() const override;

    /// 查询指定日期范围内吊舱中的文件（startDate/endDate 形如 "yyyy-MM-dd"）
    Q_INVOKABLE void refreshMediaInPod(const QString startDate, const QString endDate);

    /// 开始下载所有勾选的文件
    Q_INVOKABLE void startDownloadFiles();

    /// 全选/取消全选（供 QML 调用）
    Q_INVOKABLE void selectAll(bool select);

    /// 切换某一行的选中状态（modifiers: Qt::ControlModifier / Qt::ShiftModifier）
    Q_INVOKABLE void toggleSelection(int row, int modifiers);

    /// 双击打开文件：若已下载直接打开本地文件，否则下载后打开
    Q_INVOKABLE void podOpenFile(int row);

    /// 供 QML 遍历：返回第 row 行的数据 map
    Q_INVOKABLE QVariantMap get(int row) const;

    void downloadFiles();

    QString formatSize(qint64 bytes);
    QString formatSpeed(qint64 bytes);

signals:
    /// 所有选中文件下载完成后发出，由 MediaFileModel 监听并刷新本地文件列表
    void allDownloadsFinished();
    /// 查询列表已加载完成（可用于 UI 提示）
    void podListLoaded(int count);
    void countChanged();

private slots:
    void onAuthenticationRequired(QNetworkReply *reply, QAuthenticator *authenticator);

private:
    // --- HTTP 目录扫描 ---
    void requestDir(const QUrl& url, bool isRoot);
    void kickDirQueue();
    void handleDirReply(QNetworkReply* reply, bool isRoot);
    void parseRootIndex(const QString& html);
    void parseDateDirIndex(const QString& html, const QString& dateDir);
    void parseFilesIndex(const QString& html, const QUrl& baseUrl, const QString& type /*photo/video*/);

    // --- 缩略图 ---
    void startThumbQueue();
    void kickThumbQueue();
    void requestThumbnail(int index);
    void fetchJpegThumb(int index);
    void fetchVideoThumb(int index);
    static QImage extractFirstFrame(const QString& localPath);
    QString thumbCachePathForKey(const QString& key) const;

    // --- 工具 ---
    QString generateLocalFileName(const QString& remoteFileName, const QString& dateDir) const;
    qint64  parseHumanSize(const QString& s) const;
    QString cacheKeyFromUrl(const QUrl& url) const;

    // --- 成员 ---
    QString _baseUrl;                // e.g. http://192.168.144.119:8554/download/
    QNetworkAccessManager* _netManager = nullptr;
    QList<PodFileInfo> _podFileInfos;

    // 目录扫描队列
    struct PendingDir { QUrl url; bool isRoot; };
    QQueue<PendingDir> _dirQueue;
    int _activeDirRequests = 0;
    int _maxConcurrentDirs = 4;
    int _gen = 0;  // 每次重新查询递增，旧请求回调检查后丢弃
    QSet<QNetworkReply*> _activeReplies;  // 所有活跃请求，重新查询时 abort
    QString _startDate;              // "yyyy-MM-dd"
    QString _endDate;

    // 缩略图加载队列
    QQueue<int> _thumbQueue;
    int _activeThumbs = 0;
    int _maxConcurrentThumbs = 2;
    QString _thumbCacheDir;

    // 下载状态
    int     _downloadFileIndex = -1;
    QFile   _downloadFile;
    qint64  _fileSize = 0;
    qint64  _recievedSize = 0;
    QString _mediaRootFolder;
    QElapsedTimer _timerDownload;
    int _readThreshold = 1024 * 256;  // 256KB 写一次盘

    // 缩略图提取的临时目录
    QString _thumbTempDir;

    // 选择状态
    int _lastSelectedRow = -1;

    // 双击打开：下载完成后自动打开本地文件
    int _openAfterDownloadIndex = -1;
    QString _openLocalPath;
};

#endif  // MEDIADOWNLOAD_H
