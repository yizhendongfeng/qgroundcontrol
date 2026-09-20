#include "MediaFileModel.h"
#include <QDirIterator>
#include <QFileInfo>
#include <QMediaPlayer>
#include <QImageReader>
#include <QVideoSink>
#include <QVideoSink>
#include <QStandardPaths>
#include <QDir>
#include <QCryptographicHash>
#include <QFile>
#include <QMetaObject>
#include <QSet>
#include <QRegularExpression>
#include <QProcess>
#include <QVideoFrame>
#include <QVideoFrameFormat>
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

    // 吊舱文件下载完成后，自动刷新本地文件列表
    connect(m_mediaDownload, &MediaDownload::allDownloadsFinished,
            this, &MediaFileModel::refreshCurrentFolder);
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
    case FileSizeStrRole: return item.fileSizeStr;
    }
    return {};
}

QVariantMap MediaFileModel::get(int row) const {
    QVariantMap m;
    if (row < 0 || row >= m_items.size()) return m;
    const FileItem& item = m_items[row];
    m["filePath"] = item.filePath;
    m["thumbnailUrl"] = item.thumbnailUrl;
    m["isVideo"] = item.isVideo;
    m["thumbReady"] = item.thumbReady;
    m["selected"] = item.selected;
    m["fileSizeStr"] = item.fileSizeStr;
    return m;
}

QVariantMap MediaFileModel::getByPath(const QString& path) const {
    for (int i = 0; i < m_items.size(); i++) {
        if (m_items[i].filePath == path) return get(i);
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
        { FileSizeStrRole, "fileSizeStr"},
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

QStringList MediaFileModel::listDateDirs() {
    QStringList result;
    if (m_rootMediaFolder.isEmpty()) return result;
    QDir dir(m_rootMediaFolder);
    auto entries = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::Reversed);
    for (const QFileInfo &e : entries) {
        QString name = e.fileName();
        if (name.contains(QRegularExpression("^\\d{4}-\\d{2}-\\d{2}$"))) {
            result << name;
        }
    }
    return result;
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
// 将视频帧转换为 QImage。Qt 的 QVideoFrame::toImage() 对部分 YUV / 硬件加速
// 格式会返回空图像，这里对常见 YUV 格式做手动转换作为兜底。
static QImage videoFrameToImage(const QVideoFrame &frame)
{
    if (!frame.isValid()) return QImage();

    QImage img = frame.toImage();
    if (!img.isNull()) return img;

    QVideoFrame f = frame;
    if (!f.map(QVideoFrame::ReadOnly)) return QImage();

    const int w = f.width();
    const int h = f.height();
    QImage out(w, h, QImage::Format_RGB32);
    const QVideoFrameFormat::PixelFormat fmt = f.pixelFormat();

    auto writePixel = [&out](int x, int y, int yy, int uu, int vv) {
        const int r = yy + ((359 * vv) >> 8);
        const int g = yy - ((88 * uu + 183 * vv) >> 8);
        const int b = yy + ((454 * uu) >> 8);
        out.setPixel(x, y, qRgb(qBound(0, r, 255), qBound(0, g, 255), qBound(0, b, 255)));
    };

    switch (fmt) {
    case QVideoFrameFormat::Format_YUV420P:
    case QVideoFrameFormat::Format_YV12: {
        const uchar *Y = f.bits(0);
        const uchar *U = f.bits(1);
        const uchar *V = f.bits(2);
        const bool swapped = (fmt == QVideoFrameFormat::Format_YV12);
        const uchar *Up = swapped ? V : U;
        const uchar *Vp = swapped ? U : V;
        const int yStride = f.bytesPerLine(0);
        const int uStride = f.bytesPerLine(1);
        const int vStride = f.bytesPerLine(2);
        for (int y = 0; y < h; ++y) {
            const uchar *yLine = Y + y * yStride;
            for (int x = 0; x < w; ++x) {
                const int yy = yLine[x];
                const int uu = Up[(y >> 1) * uStride + (x >> 1)] - 128;
                const int vv = Vp[(y >> 1) * vStride + (x >> 1)] - 128;
                writePixel(x, y, yy, uu, vv);
            }
        }
        break;
    }
    case QVideoFrameFormat::Format_NV12:
    case QVideoFrameFormat::Format_NV21: {
        const uchar *Y = f.bits(0);
        const uchar *UV = f.bits(1);
        const bool swapped = (fmt == QVideoFrameFormat::Format_NV21);
        const int yStride = f.bytesPerLine(0);
        const int uvStride = f.bytesPerLine(1);
        for (int y = 0; y < h; ++y) {
            const uchar *yLine = Y + y * yStride;
            for (int x = 0; x < w; ++x) {
                const int yy = yLine[x];
                const uchar *uv = UV + (y >> 1) * uvStride + (x & ~1);
                const int uu = uv[swapped ? 1 : 0] - 128;
                const int vv = uv[swapped ? 0 : 1] - 128;
                writePixel(x, y, yy, uu, vv);
            }
        }
        break;
    }
    case QVideoFrameFormat::Format_YUYV:
    case QVideoFrameFormat::Format_UYVY: {
        const uchar *data = f.bits(0);
        const bool uyvy = (fmt == QVideoFrameFormat::Format_UYVY);
        const int stride = f.bytesPerLine(0);
        for (int y = 0; y < h; ++y) {
            const uchar *line = data + y * stride;
            for (int x = 0; x < w; x += 2) {
                const uchar *p = line + x * 2;
                const int y0 = p[uyvy ? 1 : 0];
                const int y1 = p[uyvy ? 3 : 2];
                const int uu = p[uyvy ? 0 : 1] - 128;
                const int vv = p[uyvy ? 2 : 3] - 128;
                writePixel(x, y, y0, uu, vv);
                if (x + 1 < w) writePixel(x + 1, y, y1, uu, vv);
            }
        }
        break;
    }
    default:
        out = QImage();
        break;
    }

    f.unmap();
    return out;
}

QImage MediaFileModel::loadVideoThumb(const QString &path) {
    // 切换到 Qt 内置 FFmpeg 后端（Windows Media Foundation 后端无法打开某些文件）
    static bool backendSet = false;
    if (!backendSet) {
        qputenv("QT_MEDIA_BACKEND", "ffmpeg");
        backendSet = true;
    }

    // 用 QFile 直接打开并喂给播放器：Qt 的 FFmpeg 后端在 Windows 上对 file://
    // 形式的本地路径会交给 avformat 的 file 协议打开，后者无法正确打开带盘符、
    // 空格或中文的路径（报 "Could not open file"）。直接传 QIODevice 可以绕过
    // 这条路径，QFile 自身能正确处理这些路径。
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        fprintf(stderr, "[THUMB] cannot open QFile: %s\n", file.errorString().toUtf8().constData()); fflush(stderr);
        return QImage();
    }

    QMediaPlayer player;
    QVideoSink sink;
    player.setVideoSink(&sink);

    QEventLoop loop;
    QImage result;

    QObject::connect(&sink, &QVideoSink::videoFrameChanged, &loop,
                     [&](const QVideoFrame &frame) {
        if (!result.isNull()) return;
        QImage img = videoFrameToImage(frame);
        if (img.isNull()) return;
        result = img.scaled(200, 200, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        loop.quit();
    });

    QObject::connect(&player, &QMediaPlayer::errorOccurred, &loop,
                     [&](QMediaPlayer::Error err, const QString &errString) {
        fprintf(stderr, "[THUMB] error=%d msg=%s\n", (int)err, errString.toUtf8().constData()); fflush(stderr);
        loop.quit();
    });

    player.setSourceDevice(&file, QUrl::fromLocalFile(path));
    player.play();

    QTimer::singleShot(3000, &loop, &QEventLoop::quit);
    loop.exec();
    player.stop();
    file.close();

    fprintf(stderr, "[THUMB] result null=%d\n", result.isNull()); fflush(stderr);
    return result;
}

QHash<QString, int> MediaFileModel::buildPathIndexMap() const {
    QHash<QString,int> map;
    for (int i = 0; i < m_items.size(); ++i) map.insert(m_items[i].filePath, i);
    return map;
}
void MediaFileModel::refreshFolderIncremental() {
    if (m_rootMediaFolder.isEmpty()) return;

    QStringList filters = { "*.jpg", "*.png", "*.jpeg", "*.bmp", "*.gif",
                           "*.mp4", "*.avi", "*.mov", "*.mkv", "*.webm" };
    QStringList currentFiles;
    QDirIterator it(m_rootMediaFolder, filters, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) currentFiles << it.next();
    QSet<QString> currentSet(currentFiles.begin(), currentFiles.end());
    QHash<QString,int> oldMap = buildPathIndexMap();
    QList<int> removeRows;
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
            { qint64 sz = info.size(); if (sz >= 1048576) item.fileSizeStr = QString::number(sz/1048576.0,'f',1)+" MB"; else if (sz >= 1024) item.fileSizeStr = QString::number(sz/1024.0,'f',1)+" KB"; else item.fileSizeStr = QString::number(sz)+" B"; }
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
    emit countChanged();
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
// ----------------------- 异步缩略图请求 -----------------------
// ----------------------- 异步缩略图请求 -----------------------
void MediaFileModel::requestThumbnail(int index) {
    if (index < 0 || index >= m_items.size()) return;
    FileItem &item = m_items[index];
    if (item.thumbReady) return;

    const QString path = item.filePath;
    const bool isVideo = item.isVideo;

    QtConcurrent::run([this, index, path, isVideo]() {
        QImage thumb;

        if (isVideo) {
            // QMediaPlayer（FFmpeg 后端）必须在主线程上运行：在后台线程里创建
            // QMediaPlayer 会导致 avformat_open_input 失败（报 "Could not open file"）。
            // 这里用阻塞队列调用把抓帧工作切回主线程执行。
            QMetaObject::invokeMethod(this, [this, path, &thumb]() {
                thumb = loadVideoThumb(path);
            }, Qt::BlockingQueuedConnection);
        } else {
            thumb = loadImageThumb(path);
        }

        // 缓存（QCache）与模型更新全部回到主线程，避免跨线程访问
        QMetaObject::invokeMethod(this, [this, index, path, thumb]() {
            if (index < 0 || index >= m_items.size()) return;
            if (!thumb.isNull()) {
                m_cache.insert(path, new QImage(thumb));
                saveThumbToDisk(path, thumb);
            }
            m_items[index].thumbnail = thumb;
            m_items[index].thumbnailUrl = thumb.isNull() ? QUrl() : QUrl::fromLocalFile(cacheFilePath(path));
            m_items[index].thumbReady = !thumb.isNull();
            emit dataChanged(this->index(index), this->index(index),
                             { ThumbnailUrlRole, ThumbnailRole, ThumbReadyRole });
        });
    });
}
