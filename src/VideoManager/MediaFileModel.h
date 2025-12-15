#pragma once
#include "MediaManager.h"
#include "MediaDownload.h"

#include <QAbstractListModel>
#include <QImage>
#include <QCache>
#include <QtConcurrent>
#include <QFileSystemWatcher>
#include <QDateTime>



class MediaFileModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString mediaRootFolder READ mediaRootFolder WRITE setMediaRootFolder NOTIFY mediaRootFolderChanged FINAL)
    Q_PROPERTY(MediaDownload* mediaDownload READ mediaDownload FINAL)
public:
    enum Roles {
        FilePathRole = Qt::UserRole + 1,
        ThumbnailUrlRole,
        ThumbnailRole,
        IsVideoRole,
        ThumbReadyRole,
        LastModifiedRole,
        SelectedRole,
        UploadStatusRole,
        UploadProgressRole
    };

    enum UploadStatus {
        NotUploaded = 0,
        Uploading,
        Uploaded,
        UploadFailed
    };
    Q_ENUM(UploadStatus)

    struct FileItem {
        QString filePath;
        bool isVideo = false;
        QUrl thumbnailUrl;
        QImage thumbnail;
        bool thumbReady = false;
        QDateTime lastModified;
        bool selected = false;
        int uploadStatus = NotUploaded;
        int uploadProgress = 0;
    };
    explicit MediaFileModel(QObject *parent = nullptr);
    ~MediaFileModel() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // 目录 / 选择相关 API（供 QML 使用）
    Q_INVOKABLE QStringList listYears(const QString &rootFolder);
    Q_INVOKABLE QStringList listMonths(const QString &rootFolder, const QString &year);
    Q_INVOKABLE QStringList listDays(const QString &rootFolder, const QString &year, const QString &month);

    Q_INVOKABLE void changeFolder(const QString &folder);
    Q_INVOKABLE void refreshCurrentFolder(); // 外部触发手动刷新（可选）
    Q_INVOKABLE void clickSelect(int index, int modifiers);
    Q_INVOKABLE QVariantList selectedFilesIndexs() const;
    Q_INVOKABLE void clearAllSelection();

    Q_INVOKABLE void uploadFilesToMinio();
    void uploadNextFile();
    QString mediaRootFolder();
    MediaDownload* mediaDownload() {return m_mediaDownload;};
    void setMediaRootFolder(const QString folder);
signals:
    void mediaRootFolderChanged();
private slots:
    void onDirectoryChanged(const QString &path);
    void onFileChanged(const QString &path);

private:
    QList<FileItem> m_items;
    QCache<QString, QImage> m_cache;
    QString m_cacheDir;
    QString m_rootMediaFolder;
    QString m_rootFolderVideo;
    QString m_rootFolderPhoto;
    QFileSystemWatcher m_watcher;
    int m_lastSelectIndex = -1;
    MediaManager* m_mediaManager;
    MediaDownload* m_mediaDownload;

    QVariantList m_fileIndexsToUpload;
    int m_uploadIndex = -1;
    // 磁盘缓存
    QString cacheKey(const QString &path) const;
    QString cacheFilePath(const QString &path) const;
    void saveThumbToDisk(const QString &path, const QImage &img);
    QImage loadThumbFromDisk(const QString &path);
    void removeThumbFromDisk(const QString &path);

    // 缩略图生成（同步函数，用于后台任务）
    static QImage loadImageThumb(const QString &path);
    static QImage loadVideoThumb(const QString &path);

    // 增量更新相关
    QHash<QString, int> buildPathIndexMap() const;
    void refreshFolderIncremental(); // 增量刷新：add/remove/modify
    void watchDirectoryRecursively(const QString &dir);

    // 异步请求缩略图
    void requestThumbnail(int index);

    // 辅助：插入/移除项（带通知）
    void insertFileItemAt(const FileItem &item, int row);
    void removeFileItemAt(int row);
};
