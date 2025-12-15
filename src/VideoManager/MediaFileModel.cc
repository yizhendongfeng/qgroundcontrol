#include "MediaFileModel.h"
#include <QDirIterator>
#include <QFileInfo>
#include <QMediaPlayer>
#include <QVideoSink>
#include <QEventLoop>
#include <QTimer>
#include <QStandardPaths>
#include <QDir>
#include <QCryptographicHash>
#include <QFile>
#include <QMetaObject>
#include <QSet>
#include <QVideoFrame>
#include <QDebug>

#include "AppSettings.h"
#include "SettingsManager.h"


MediaFileModel::MediaFileModel(QObject *parent)
    : QAbstractListModel(parent), m_cache(5000)
{
    // 磁盘缓存目录
    m_rootFolderVideo = SettingsManager::instance()->appSettings()->videoSavePath();
    m_rootFolderPhoto = SettingsManager::instance()->appSettings()->photoSavePath();
    m_cacheDir = SettingsManager::instance()->appSettings()->mediaCachePath();
    m_rootMediaFolder =  SettingsManager::instance()->appSettings()->mediaSavePath();
    // watcher 信号连接
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged,
            this, &MediaFileModel::onDirectoryChanged);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged,
            this, &MediaFileModel::onFileChanged);
    m_mediaDownload = new MediaDownload(this);
    m_mediaManager = new MediaManager(this);
    // 上传进度更新
    QObject::connect(m_mediaManager, &MediaManager::uploadProgress, this, [&](const int fileIndex, const int progress) {
        m_items[fileIndex].uploadProgress = progress;
        emit dataChanged(this->index(fileIndex), this->index(fileIndex), {UploadProgressRole});
        qDebug() << QString("索引：1%, 进度：%2%").arg(fileIndex).arg(progress);
    });

    // 上传结果
    QObject::connect(m_mediaManager, &MediaManager::uploadFinished, this, [&](const int fileIndex, const bool success, const QString& message) {
        m_items[fileIndex].uploadStatus = success ? Uploaded : UploadFailed;
        emit dataChanged(this->index(fileIndex), this->index(fileIndex), {UploadStatusRole});
        uploadNextFile();
        qDebug() << "upload finished: " << success;
    });

    // 上传错误
    QObject::connect(m_mediaManager, &MediaManager::uploadError, this, [&](const QString& errorMsg) {
        qDebug() << "uploadError:" << errorMsg;
    });
}

MediaFileModel::~MediaFileModel()
{
    // QtConcurrent 任务由框架管理，通常不需要手动等待
}

int MediaFileModel::rowCount(const QModelIndex &) const {
    return m_items.count();
}

QVariant MediaFileModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_items.size())
        return {};

    const FileItem &item = m_items[index.row()];

    switch (role) {
    case FilePathRole: return item.filePath;
    case ThumbnailUrlRole: return item.thumbnailUrl;
    case ThumbnailRole: return item.thumbnail;
    case IsVideoRole: return item.isVideo;
    case ThumbReadyRole: return item.thumbReady;
    case LastModifiedRole: return item.lastModified.toMSecsSinceEpoch();
    case SelectedRole: return item.selected;
    case UploadStatusRole: return item.uploadStatus;
    case UploadProgressRole: return item.uploadProgress;
    }
    return {};
}

QHash<int, QByteArray> MediaFileModel::roleNames() const {
    return {
        { FilePathRole, "filePath" },
        { ThumbnailUrlRole, "thumbnailUrl" },
        { ThumbnailRole, "thumbnail" },
        { IsVideoRole, "isVideo" },
        { ThumbReadyRole, "thumbReady" },
        { LastModifiedRole, "lastModified" },
        { SelectedRole, "selected"},
        { UploadStatusRole, "uploadStatus"},
        { UploadProgressRole, "uploadProgress"},
    };
}

// ----------------------- 年/月/日 列表 API -----------------------
QStringList MediaFileModel::listYears(const QString &rootFolder) {
    QStringList years;
    if (rootFolder.isEmpty()) return years;
    QDir dir(rootFolder);
    const auto entries = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &e : entries) years << e.fileName();
    return years;
}

QStringList MediaFileModel::listMonths(const QString &rootFolder, const QString &year) {
    QStringList months;
    if (rootFolder.isEmpty() || year.isEmpty()) return months;
    QDir dir(rootFolder + "/" + year);
    if (!dir.exists()) return months;
    auto entries = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &e : entries) months << e.fileName();
    return months;
}

QStringList MediaFileModel::listDays(const QString &rootFolder, const QString &year, const QString &month) {
    QStringList days;
    if (rootFolder.isEmpty() || year.isEmpty() || month.isEmpty()) return days;
    QDir dir(rootFolder + "/" + year + "/" + month);
    if (!dir.exists()) return days;
    auto entries = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &e : entries) days << e.fileName();
    return days;
}

// ----------------------- 切换监听目录（供 QML 调用） -----------------------
void MediaFileModel::changeFolder(const QString &folder) {
    QString folderPath = QUrl(folder).toLocalFile();
    qDebug() << "changeFolder()" << folderPath;
    if (folderPath.isEmpty()) return;
    // if (m_rootMediaFolder == folderPath)
    //     return;

    // 移除旧 watcher（目录与文件）
    auto oldDirs = m_watcher.directories();
    if (!oldDirs.isEmpty()) m_watcher.removePaths(oldDirs);
    auto oldFiles = m_watcher.files();
    if (!oldFiles.isEmpty()) m_watcher.removePaths(oldFiles);

    m_rootMediaFolder = folderPath;
    emit mediaRootFolderChanged();
    refreshFolderIncremental();
    watchDirectoryRecursively(folderPath);

    // 为每个文件添加文件级监听（用于修改检测）
    QStringList filePaths;
    for (const auto &it : m_items) filePaths << it.filePath;
    if (!filePaths.isEmpty()) m_watcher.addPaths(filePaths);
}

void MediaFileModel::refreshCurrentFolder() {
    refreshFolderIncremental();
}

void MediaFileModel::clickSelect(int index, int modifiers)
{
    bool isCtrl = modifiers & Qt::ControlModifier;
    bool isShift = modifiers & Qt::ShiftModifier;

    if (index < 0 || index >= m_items.size())
        return;

    if (isShift && m_lastSelectIndex >= 0) {
        // ---- Shift 连续选择 ----
        int begin = qMin(m_lastSelectIndex, index);
        int end   = qMax(m_lastSelectIndex, index);

        // 清空当前全部
        clearAllSelection();

        for (int i = begin; i <= end; i++)
            m_items[i].selected = true;

        emit dataChanged(this->index(begin), this->index(end), {SelectedRole});
        return;
    }

    if (isCtrl) {
        // ---- Ctrl 多选 ----
        m_items[index].selected = !m_items[index].selected;
        emit dataChanged(this->index(index), this->index(index), {SelectedRole});
        m_lastSelectIndex = index;
        return;
    }

    // ---- 无修饰键：单选 ----
    clearAllSelection();
    m_items[index].selected = true;
    emit dataChanged(this->index(index), this->index(index), {SelectedRole});
    m_lastSelectIndex = index;
}

QVariantList MediaFileModel::selectedFilesIndexs() const
{
    QVariantList list;
    for (int i = 0; i < m_items.count(); i++) {
        if (m_items[i].selected)
            list.append(i);
    }
    return list;
}

void MediaFileModel::clearAllSelection()
{
    for (int i = 0; i < m_items.size(); i++)
        m_items[i].selected = false;

    emit dataChanged(this->index(0), this->index(m_items.size() - 1), {SelectedRole});
}

void MediaFileModel::uploadFilesToMinio()
{
    // m_fi
    m_fileIndexsToUpload = selectedFilesIndexs();
    m_uploadIndex = -1;
    uploadNextFile();
}

void MediaFileModel::uploadNextFile()
{
    if (!m_mediaManager)
        return;
    if (++m_uploadIndex >= m_fileIndexsToUpload.count()) {
        return;
    }
    const int index = m_fileIndexsToUpload.at(m_uploadIndex).toInt();
    const QString filePath = m_items[index].filePath;
    qDebug() << "MeidaFileModel::uploadNextFile() filePath: " << filePath;
    m_mediaManager->startUpload(filePath, index);
}

QString MediaFileModel::mediaRootFolder()
{
    return m_rootMediaFolder;
}

void MediaFileModel::setMediaRootFolder(const QString folder)
{
    m_rootMediaFolder = QUrl(folder).toLocalFile();
    emit mediaRootFolderChanged();
}

// ----------------------- 磁盘缓存实现 -----------------------
QString MediaFileModel::cacheKey(const QString &path) const {
    return QCryptographicHash::hash(path.toUtf8(), QCryptographicHash::Md5).toHex();
}

QString MediaFileModel::cacheFilePath(const QString &path) const {
    return m_cacheDir + "/" + cacheKey(path) + ".png";
}

void MediaFileModel::saveThumbToDisk(const QString &path, const QImage &img) {
    QString file = cacheFilePath(path);
    img.save(file, "PNG");
}

QImage MediaFileModel::loadThumbFromDisk(const QString &path) {
    QString file = cacheFilePath(path);
    if (QFile::exists(file)) {
        QImage img(file);
        if (!img.isNull()) return img;
    }
    return QImage();
}

void MediaFileModel::removeThumbFromDisk(const QString &path) {
    QString file = cacheFilePath(path);
    if (QFile::exists(file)) QFile::remove(file);
}

// ----------------------- 缩略图生成（同步函数） -----------------------
QImage MediaFileModel::loadImageThumb(const QString &path) {
    QImage img(path);
    if (img.isNull()) return QImage();
    return img.scaled(200, 200, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

QImage MediaFileModel::loadVideoThumb(const QString &path) {
    // 注意：在某些平台/Qt版本中，QMediaPlayer 可能要求在主线程使用。如果遇到问题，请改用 FFmpeg 后端。
    QMediaPlayer player;
    QVideoSink sink;
    player.setVideoSink(&sink);

    QEventLoop loop;
    QObject::connect(&sink, &QVideoSink::videoFrameChanged, &loop, &QEventLoop::quit);

    player.setSource(QUrl::fromLocalFile(path));
    player.play();

    QTimer::singleShot(300, &loop, &QEventLoop::quit);
    loop.exec();

    QVideoFrame frame = sink.videoFrame();
    if (frame.isValid()) {
        QImage img = frame.toImage();
        if (!img.isNull()) return img.scaled(200, 200, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    return QImage();
}

// ----------------------- 构建路径索引映射 -----------------------
QHash<QString, int> MediaFileModel::buildPathIndexMap() const {
    QHash<QString,int> map;
    for (int i = 0; i < m_items.size(); ++i) map.insert(m_items[i].filePath, i);
    return map;
}

// ----------------------- 增量刷新实现（add/remove/modify） -----------------------
void MediaFileModel::refreshFolderIncremental() {
    if (m_rootMediaFolder.isEmpty()) return;

    // 扫描磁盘当前文件
    QStringList filters = { "*.jpg", "*.png", "*.jpeg", "*.bmp", "*.gif",
                           "*.mp4", "*.avi", "*.mov", "*.mkv", "*.webm" };
    QStringList currentFiles;
    QDirIterator it(m_rootMediaFolder, filters, QDir::Files);
    while (it.hasNext()) currentFiles << it.next();
    qDebug() << "refreshFolderIncremental:" << currentFiles;
    QSet<QString> currentSet(currentFiles.begin(), currentFiles.end());
    QHash<QString,int> oldMap = buildPathIndexMap();

    // 1. 删除不存在的（反向删除保证索引正确）
    QList<int> removeRows;
    for (int i = 0; i < m_items.size(); ++i) {
        if (!currentSet.contains(m_items[i].filePath)) removeRows.prepend(i);
    }
    for (int row : removeRows) {
        QString path = m_items[row].filePath;
        if (m_watcher.files().contains(path)) m_watcher.removePath(path);
        if (m_cache.contains(path)) { delete m_cache.take(path); }
        removeThumbFromDisk(path);
        beginRemoveRows(QModelIndex(), row, row);
        m_items.removeAt(row);
        endRemoveRows();
    }

    // 2. 新增文件 -> 插入（append）
    for (const QString &path : currentFiles) {
        if (!oldMap.contains(path)) {
            QFileInfo info(path);
            FileItem item;
            item.filePath = path;
            item.isVideo = QStringList({"mp4","avi","mov","mkv","webm"}).contains(info.suffix().toLower());
            item.lastModified = info.lastModified();
            item.thumbReady = false;

            // 尝试内存 / 磁盘缓存
            if (m_cache.contains(path)) {
                item.thumbnail = *m_cache[path];
                item.thumbnailUrl = QUrl::fromLocalFile(cacheFilePath(path));
                item.thumbReady = true;
            } else {
                QImage disk = loadThumbFromDisk(path);
                if (!disk.isNull()) {
                    item.thumbnail = disk;
                    item.thumbReady = true;
                    item.thumbnailUrl = QUrl::fromLocalFile(cacheFilePath(path));
                    m_cache.insert(path, new QImage(disk));
                }
            }

            int row = m_items.size();
            beginInsertRows(QModelIndex(), row, row);
            m_items.append(item);
            endInsertRows();

            // 监听新文件（用于文件修改）
            m_watcher.addPath(path);

            // 异步生成缩略图（如果未就绪）
            requestThumbnail(row);
        }
    }

    // 3. 修改检测（存在但 lastModified 不同）
    for (const QString &path : currentFiles) {
        if (oldMap.contains(path)) {
            int idx = oldMap.value(path);
            QFileInfo info(path);
            QDateTime newMod = info.lastModified();
            if (newMod != m_items[idx].lastModified) {
                m_items[idx].lastModified = newMod;
                m_items[idx].thumbReady = false;
                m_items[idx].thumbnail = QImage();
                if (m_cache.contains(path)) { delete m_cache.take(path); }
                removeThumbFromDisk(path);
                emit dataChanged(index(idx), index(idx), { ThumbnailRole, ThumbReadyRole, LastModifiedRole });
                requestThumbnail(idx);
            }
        }
    }

    // 4. 确保 watcher 覆盖所有目录与文件
    QStringList watchedFiles = m_watcher.files();
    QStringList toAdd;
    for (const QString &path : currentFiles) if (!watchedFiles.contains(path)) toAdd << path;
    if (!toAdd.isEmpty()) m_watcher.addPaths(toAdd);
    watchDirectoryRecursively(m_rootFolderVideo);
}

// 递归注册目录（QFileSystemWatcher 不支持递归）
void MediaFileModel::watchDirectoryRecursively(const QString &dir) {
    if (dir.isEmpty()) return;
    if (!m_watcher.directories().contains(dir)) m_watcher.addPath(dir);
    QDir d(dir);
    QFileInfoList entries = d.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &entry : entries) watchDirectoryRecursively(entry.absoluteFilePath());
}

// ----------------------- watcher 回调 -----------------------
void MediaFileModel::onDirectoryChanged(const QString &path) {
    Q_UNUSED(path);
    // 差异化刷新（增量）
    refreshFolderIncremental();
}

void MediaFileModel::onFileChanged(const QString &path) {
    Q_UNUSED(path);
    // 文件被修改或删除 -> 差异化刷新
    refreshFolderIncremental();
}

// ----------------------- 插入/移除辅助 -----------------------
void MediaFileModel::insertFileItemAt(const FileItem &item, int row) {
    beginInsertRows(QModelIndex(), row, row);
    m_items.insert(row, item);
    endInsertRows();
}

void MediaFileModel::removeFileItemAt(int row) {
    beginRemoveRows(QModelIndex(), row, row);
    m_items.removeAt(row);
    endRemoveRows();
}

// ----------------------- 异步缩略图请求 -----------------------
void MediaFileModel::requestThumbnail(int index) {
    if (index < 0 || index >= m_items.size()) return;
    FileItem &item = m_items[index];
    if (item.thumbReady) return;

    // 后台任务
    QtConcurrent::run([this, index]() {
        // 复制当前项信息（避免跨线程访问 m_items 直接读取）
        FileItem local;
        {
            QMetaObject::invokeMethod(const_cast<MediaFileModel*>(this), [this, index, &local]() {
                if (index < 0 || index >= m_items.size()) return;
                local = m_items[index];
            }, Qt::BlockingQueuedConnection);
        }

        QImage thumb;
        QUrl thumbnailUrl;
        // 先检查内存缓存
        if (m_cache.contains(local.filePath)) {
            thumb = *m_cache[local.filePath];
            thumbnailUrl = QUrl::fromLocalFile(cacheFilePath(local.filePath));
        } else {
            // 再检查磁盘缓存
            QImage disk = loadThumbFromDisk(local.filePath);
            if (!disk.isNull()) {
                thumb = disk;
                thumbnailUrl = QUrl::fromLocalFile(cacheFilePath(local.filePath));
                m_cache.insert(local.filePath, new QImage(disk));
            } else {
                // 生成缩略图
                if (local.isVideo) thumb = loadVideoThumb(local.filePath);
                else thumb = loadImageThumb(local.filePath);

                if (!thumb.isNull()) {
                    m_cache.insert(local.filePath, new QImage(thumb));
                    saveThumbToDisk(local.filePath, thumb);
                    local.thumbnailUrl = QUrl::fromLocalFile(cacheFilePath(local.filePath));
                    thumbnailUrl = QUrl::fromLocalFile(cacheFilePath(local.filePath));
                }
            }
        }

        // 更新 UI 线程
        QMetaObject::invokeMethod(this, [this, index, thumb, thumbnailUrl]() {
            if (index < 0 || index >= m_items.size()) return;
            m_items[index].thumbnail = thumb;
            m_items[index].thumbnailUrl = thumbnailUrl;
            m_items[index].thumbReady = !thumb.isNull();
            emit dataChanged(this->index(index), this->index(index),
                             { ThumbnailUrlRole, ThumbnailRole, ThumbReadyRole });
        });
    });
}
