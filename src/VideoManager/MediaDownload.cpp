#include "MediaDownload.h"

#include <QRegularExpression>
#include <QUrl>
#include <QNetworkRequest>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QFile>
#include <QAuthenticator>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QDebug>
#include <QQueue>
#include <QSet>
#include <QtConcurrent>

#include <QMediaPlayer>
#include <QVideoSink>
#include <QVideoFrame>
#include <QEventLoop>
#include <QTimer>
#include <QProcess>
#include <QDesktopServices>
#include <cstdio>
#include <vector>

#include "SettingsManager.h"
#include "AppSettings.h"
#include "VideoSettings.h"

MediaDownload::MediaDownload(QObject* parent)
    : QAbstractListModel(parent)
{
    _netManager = new QNetworkAccessManager(this);

    // 本地媒体根目录
    _mediaRootFolder = SettingsManager::instance()->appSettings()->mediaSavePath();

    // 新吊舱文件服务器基地址：http://{podIp}:8554/download/
    QString podIp = SettingsManager::instance()->videoSettings()->podIp()->rawValue().toString();
    if (!podIp.startsWith("http://") && !podIp.startsWith("https://")) {
        _baseUrl = QString("http://%1:8554/download/").arg(podIp);
    } else {
        _baseUrl = podIp;
        if (!_baseUrl.endsWith('/')) _baseUrl += '/';
    }
    qDebug() << "MediaDownload baseUrl:" << _baseUrl;

    // 缩略图磁盘缓存目录
    _thumbCacheDir = SettingsManager::instance()->appSettings()->mediaCachePath() + "/pod_thumbs";
    QDir().mkpath(_thumbCacheDir);

    // 视频首帧提取临时目录（Range 拉下来的前 1MB 落盘）
    _thumbTempDir = SettingsManager::instance()->appSettings()->mediaCachePath() + "/pod_tmp";
    QDir().mkpath(_thumbTempDir);
}

// ----------------------- 模型接口 -----------------------

int MediaDownload::rowCount(const QModelIndex&) const
{
    return _podFileInfos.count();
}

QVariantMap MediaDownload::get(int row) const
{
    QVariantMap m;
    if (row < 0 || row >= _podFileInfos.size()) return m;
    const auto& item = _podFileInfos[row];
    m["filePathStr"] = item.filePath;
    m["fileNameStr"] = item.fileName;
    m["isVideo"] = item.isVideo;
    m["thumbnailUrl"] = item.thumbnailUrl;
    m["thumbReady"] = item.thumbReady;
    m["fileSelected"] = item.selected;
    m["downloadedProgress"] = item.downloadProgress;
    m["fileSizeStr"] = item.fileSizeStr;
    return m;
}

QVariant MediaDownload::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= _podFileInfos.size())
        return {};

    const PodFileInfo& item = _podFileInfos[index.row()];
    switch (role) {
    case FilePathRole:          return item.filePath;
    case FileNameRole:          return item.fileName;
    case FileTypeRole:          return item.fileType;
    case StartTimeRole:         return item.startTime;
    case DurationRole:          return item.duration;
    case SizeRole:              return item.fileSize;
    case SizeStrRole:           return item.fileSizeStr;
    case FileSelectedRole:      return item.selected;
    case DownloadProgressRole:   return item.downloadProgress;
    case IsVideoRole:           return item.isVideo;
    case ThumbnailUrlRole:      return item.thumbnailUrl;
    case ThumbReadyRole:        return item.thumbReady;
    }
    return {};
}

bool MediaDownload::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid() || index.row() >= _podFileInfos.size()) {
        return false;
    }
    bool changed = false;
    PodFileInfo& fileInfo = _podFileInfos[index.row()];
    switch (role) {
    case FileSelectedRole:
        if (fileInfo.selected != value.toBool()) {
            fileInfo.selected = value.toBool();
            changed = true;
        }
        break;
    default:
        break;
    }
    if (changed) {
        emit dataChanged(index, index, {role});
    }
    return changed;
}

QHash<int, QByteArray> MediaDownload::roleNames() const
{
    return {
        { FilePathRole,         "filePathStr" },
        { FileNameRole,         "fileNameStr" },
        { FileTypeRole,         "fileTypeStr" },
        { StartTimeRole,        "startTimeStr" },
        { DurationRole,         "duration" },
        { SizeRole,             "fileSize" },
        { SizeStrRole,          "fileSizeStr" },
        { FileSelectedRole,     "fileSelected" },
        { DownloadProgressRole, "downloadedProgress" },
        { IsVideoRole,          "isVideo" },
        { ThumbnailUrlRole,     "thumbnailUrl" },
        { ThumbReadyRole,       "thumbReady" }
    };
}

// ----------------------- 查询（目录扫描） -----------------------

void MediaDownload::refreshMediaInPod(const QString startDate, const QString endDate)
{
    // 取消所有进行中的旧请求
    _gen++;
    for (QNetworkReply* r : _activeReplies) {
        r->disconnect(this);
        r->abort();
        r->deleteLater();
    }
    _activeReplies.clear();

    // 重置模型
    beginResetModel();
    _podFileInfos.clear();
    _podFileInfos.reserve(512);
    _dirQueue.clear();
    _activeDirRequests = 0;
    _thumbQueue.clear();
    _activeThumbs = 0;
    endResetModel();

    _startDate = startDate;
    _endDate   = endDate;

    qDebug() << "refreshMediaInPod:" << _startDate << "~" << _endDate;

    // 发起根目录请求
    QUrl rootUrl(_baseUrl);
    requestDir(rootUrl, /*isRoot=*/true);
}

void MediaDownload::requestDir(const QUrl& url, bool isRoot)
{
    _dirQueue.enqueue({ url, isRoot });
    kickDirQueue();
}

void MediaDownload::kickDirQueue()
{
    while (_activeDirRequests < _maxConcurrentDirs && !_dirQueue.isEmpty()) {
        PendingDir p = _dirQueue.dequeue();
        _activeDirRequests++;
        fprintf(stderr, "[POD] GET %s (active=%d)\n", p.url.toString().toUtf8().constData(), _activeDirRequests);
        fflush(stderr);

        QNetworkRequest req(p.url);
        req.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
        QNetworkReply* reply = _netManager->get(req);
        _activeReplies.insert(reply);
        int gen = _gen;
        connect(reply, &QNetworkReply::finished, this, [this, reply, p, gen]() {
            _activeReplies.remove(reply);
            if (gen != _gen) { reply->deleteLater(); return; }
            _activeDirRequests--;
            if (reply->error() != QNetworkReply::NoError) {
                reply->deleteLater();
                kickDirQueue();
                return;
            }
            handleDirReply(reply, p.isRoot);
            reply->deleteLater();
            kickDirQueue();
        });
    }
}

void MediaDownload::handleDirReply(QNetworkReply* reply, bool isRoot)
{
    int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "handleDirReply error:" << reply->error() << reply->errorString()
                   << "url:" << reply->url() << "status:" << status;
        return;
    }
    QString html = QString::fromUtf8(reply->readAll());
    qDebug() << "handleDirReply ok, isRoot=" << isRoot << "url=" << reply->url()
             << "htmlLen=" << html.size();
    if (isRoot) {
        parseRootIndex(html);
    } else {
        QString path = reply->url().path();
        QStringList segs = path.split('/', Qt::SkipEmptyParts);
        qDebug() << "handleDirReply path segs:" << segs;
        if (segs.size() == 2) {
            parseDateDirIndex(html, segs[1]);
        } else if (segs.size() == 3) {
            parseFilesIndex(html, reply->url(), segs[2]);
            fprintf(stderr, "[POD] handleDirReply: parseFilesIndex returned\n"); fflush(stderr);
        } else {
            qWarning() << "Unexpected dir path:" << path;
        }
    }
    fprintf(stderr, "[POD] handleDirReply returning\n"); fflush(stderr);
}

void MediaDownload::parseRootIndex(const QString& html)
{
    // 匹配所有 <a href="xxx">label</a>
    QRegularExpression re("<a\\s+href=\"([^\"]+)\"\\s*[^>]*>([^<]*)</a>");
    auto it = re.globalMatch(html);

    QVector<QString> dateDirs;
    while (it.hasNext()) {
        auto m = it.next();
        QString href  = m.captured(1);
        QString label = m.captured(2);

        // 跳过父目录和绝对路径
        if (href == "../" || href == "./" || href.startsWith("/") || href.startsWith("http"))
            continue;
        // 必须是目录（label 以 / 结尾）
        if (!label.endsWith('/'))
            continue;
        // 只取 YYYY-MM-DD 形式
        QString name = label.chopped(1);  // 去掉结尾 /
        QRegularExpression dateRe("^(\\d{4}-\\d{2}-\\d{2})$");
        if (!dateRe.match(name).hasMatch())
            continue;
        // 按日期范围过滤（YYYY-MM-DD 字符串比较即可）
        if (!_startDate.isEmpty() && name < _startDate) continue;
        if (!_endDate.isEmpty()   && name > _endDate)   continue;

        dateDirs.append(name);
    }

    qDebug() << "parseRootIndex found date dirs:" << dateDirs;

    // 去重并去请求每个日期目录
    QSet<QString> seen;
    for (const QString& d : dateDirs) {
        if (seen.contains(d)) continue;
        seen.insert(d);
        QUrl dateUrl(_baseUrl + d + "/");
        requestDir(dateUrl, /*isRoot=*/false);
    }

    // 如果没有日期目录，也发个空列表通知 UI
    if (dateDirs.isEmpty()) {
        emit podListLoaded(0); emit countChanged();
    }
}

void MediaDownload::parseDateDirIndex(const QString& html, const QString& dateDir)
{
    qDebug() << "parseDateDirIndex:" << dateDir << "htmlLen=" << html.size();
    QRegularExpression re("<a\\s+href=\"([^\"]+)\"\\s*[^>]*>([^<]*)</a>");
    auto it = re.globalMatch(html);
    while (it.hasNext()) {
        auto m = it.next();
        QString href  = m.captured(1);
        QString label = m.captured(2);
        qDebug() << "  dateDir match:" << href << label;
        if (href == "../" || href == "./") continue;
        if (!label.endsWith('/')) continue;

        QString sub = label.chopped(1);
        qDebug() << "  subdir:" << sub;
        if (sub == "photo" || sub == "video") {
            QUrl subUrl(_baseUrl + dateDir + "/" + sub + "/");
            requestDir(subUrl, /*isRoot=*/false);
        }
    }
}

void MediaDownload::parseFilesIndex(const QString& html, const QUrl& baseUrl, const QString& type)
{
    qDebug() << "parseFilesIndex enter, type=" << type << "base=" << baseUrl << "htmlLen=" << html.size();

    std::vector<PodFileInfo> newItems;
    newItems.reserve(64);
    // 用字符串查找代替正则迭代器，避免迭代器问题
    int searchPos = 0;
    while (true) {
        int aPos = html.indexOf("<a ", searchPos, Qt::CaseInsensitive);
        if (aPos < 0) break;
        int hrefStart = html.indexOf("href=\"", aPos, Qt::CaseInsensitive);
        if (hrefStart < 0 || hrefStart - aPos > 200) { searchPos = aPos + 3; continue; }
        hrefStart += 6;
        int hrefEnd = html.indexOf('"', hrefStart);
        if (hrefEnd < 0) break;
        int gtPos = html.indexOf('>', hrefEnd);
        if (gtPos < 0) break;
        int labelEnd = html.indexOf("</a>", gtPos, Qt::CaseInsensitive);
        if (labelEnd < 0) break;

        QString href = html.mid(hrefStart, hrefEnd - hrefStart);
        QString label = html.mid(gtPos + 1, labelEnd - gtPos - 1).trimmed();
        searchPos = labelEnd + 4;

        qDebug() << "  match href=" << href << "label=" << label;

        if (href == "../" || href == "./") continue;
        if (label.endsWith('/')) continue;

        QString lower = label.toLower();
        bool isVideo = false;
        if (lower.endsWith(".mp4"))       isVideo = true;
        else if (lower.endsWith(".jpeg") ||
                 lower.endsWith(".jpg")  ||
                 lower.endsWith(".png"))  isVideo = false;
        else
            continue;

        PodFileInfo info;
        fprintf(stderr, "[POD] step1 info created\n"); fflush(stderr);
        info.filePath  = baseUrl.resolved(QUrl(href)).toString();
        fprintf(stderr, "[POD] step2 filePath=%s\n", info.filePath.toUtf8().constData()); fflush(stderr);
        info.fileName  = label;
        info.fileType  = isVideo ? "video" : "photo";
        info.isVideo   = isVideo;

        QRegularExpression timeRe("^(\\d{4}-\\d{2}-\\d{2})T(\\d{2})-(\\d{2})-(\\d{2})");
        auto tm = timeRe.match(label);
        fprintf(stderr, "[POD] step3 time matched=%d\n", tm.hasMatch()); fflush(stderr);
        if (tm.hasMatch()) {
            info.startTime = QString("%1 %2:%3:%4")
                                 .arg(tm.captured(1), tm.captured(2), tm.captured(3), tm.captured(4));
        } else {
            info.startTime = label;
        }

        // 大小：从 </a> 后面的文本抓，格式为 " 18-Sep-2026 11:27  519.8K"
        // 跳过日期，只匹配数字+K/M/G单位
        int afterStart = labelEnd + 4;
        QString after = html.mid(afterStart, 200);
        QRegularExpression sizeRe2("(\\d+\\.?\\d*)\\s*([KMG])",
                                   QRegularExpression::CaseInsensitiveOption);
        auto sm = sizeRe2.match(after);
        if (sm.hasMatch()) {
            double num = sm.captured(1).toDouble();
            QString unit = sm.captured(2).toUpper();
            if (unit.startsWith("K"))      info.fileSize = (qint64)(num * 1024);
            else if (unit.startsWith("M"))  info.fileSize = (qint64)(num * 1024.0 * 1024);
            else if (unit.startsWith("G"))  info.fileSize = (qint64)(num * 1024.0 * 1024 * 1024);
            else                            info.fileSize = (qint64)num;
            info.fileSizeStr = formatSize(info.fileSize);
        }

        // 视频也等待截帧（用 ffmpeg），照片等待下载缩略图
        info.thumbReady = false;
        fprintf(stderr, "[POD] pre-append, count=%d\n", (int)newItems.size()); fflush(stderr);
        newItems.push_back(info);
        fprintf(stderr, "[POD] post-append, count=%d\n", (int)newItems.size()); fflush(stderr);
    }
    fprintf(stderr, "[POD] loop done, newItems=%d\n", newItems.size());
    fflush(stderr);

    if (newItems.empty()) {
        if (_activeDirRequests == 0 && _dirQueue.isEmpty()) {
            emit podListLoaded(_podFileInfos.count()); emit countChanged();
            // 延迟到下一个事件循环迭代，避免在 reply finished 回调中重入发起网络请求导致死锁
            // 不自动加载缩略图（列表模式）
        }
        return;
    }

    fprintf(stderr, "[POD] before beginInsertRows, oldCount=%d newCount=%d\n",
            _podFileInfos.count(), newItems.size());
    fflush(stderr);
    beginInsertRows(QModelIndex(), _podFileInfos.count(), _podFileInfos.count() + newItems.size() - 1);
    _podFileInfos.reserve(_podFileInfos.count() + newItems.size());
    for (const PodFileInfo& item : newItems) {
        PodFileInfo fi = item;
        QString dateDir;
        QRegularExpression dateRe("/download/(\\d{4}-\\d{2}-\\d{2})/");
        auto dm = dateRe.match(fi.filePath);
        if (dm.hasMatch()) dateDir = dm.captured(1);
        QString localName = generateLocalFileName(fi.fileName, dateDir);
        QString localPath = _mediaRootFolder + "/" + dateDir + "/" + localName;
        if (QFileInfo::exists(localPath)) fi.downloadProgress = 1.0;
        _podFileInfos.append(fi);
    }
    endInsertRows();
    emit podListLoaded(_podFileInfos.count());
    emit countChanged();

    fprintf(stderr, "[POD] parseFilesIndex at end, newItems=%d\n", (int)newItems.size());
    fflush(stderr);
    newItems.clear();
    fprintf(stderr, "[POD] newItems cleared OK\n");
    fflush(stderr);
}

void MediaDownload::startThumbQueue()
{
    fprintf(stderr, "[POD] startThumbQueue called, files=%d\n", _podFileInfos.size());
    fflush(stderr);
    for (int i = 0; i < _podFileInfos.size(); ++i) {
        if (!_podFileInfos[i].thumbReady) {
            _thumbQueue.enqueue(i);
        }
    }
    kickThumbQueue();
    fprintf(stderr, "[POD] startThumbQueue done\n");
    fflush(stderr);
}

// ----------------------- 缩略图 -----------------------

void MediaDownload::kickThumbQueue()
{
    while (_activeThumbs < _maxConcurrentThumbs && !_thumbQueue.isEmpty()) {
        int idx = _thumbQueue.dequeue();
        _activeThumbs++;
        requestThumbnail(idx);
    }
}

void MediaDownload::requestThumbnail(int index)
{
    if (index < 0 || index >= _podFileInfos.size()) {
        _activeThumbs--;
        kickThumbQueue();
        return;
    }
    const PodFileInfo& item = _podFileInfos[index];
    if (item.isVideo) {
        fetchVideoThumb(index);
    } else {
        fetchJpegThumb(index);
    }
}

QString MediaDownload::cacheKeyFromUrl(const QUrl& url) const
{
    QByteArray hash = QCryptographicHash::hash(url.toString().toUtf8(), QCryptographicHash::Md5);
    return QString::fromLatin1(hash.toHex());
}

QString MediaDownload::thumbCachePathForKey(const QString& key) const
{
    return _thumbCacheDir + "/" + key + ".jpg";
}

void MediaDownload::fetchJpegThumb(int index)
{
    qDebug() << "fetchJpegThumb enter, index=" << index;
    if (index < 0 || index >= _podFileInfos.size()) {
        _activeThumbs--;
        kickThumbQueue();
        return;
    }
    PodFileInfo& item = _podFileInfos[index];
    QString key = cacheKeyFromUrl(QUrl(item.filePath));
    QString cachePath = thumbCachePathForKey(key);

    // 磁盘缓存命中
    if (QFileInfo::exists(cachePath)) {
        item.thumbnailUrl = QUrl::fromLocalFile(cachePath);
        item.thumbReady = true;
        emit dataChanged(this->index(index), this->index(index),
                         { ThumbnailUrlRole, ThumbReadyRole });
        _activeThumbs--;
        kickThumbQueue();
        return;
    }

    // 下载图片到本地缓存
    QNetworkRequest req(QUrl(item.filePath));
    req.setRawHeader("Range", "bytes=0-5242880");
    req.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferNetwork);
    QNetworkReply* reply = _netManager->get(req);
    int gen = _gen;
    connect(reply, &QNetworkReply::finished, this, [this, reply, index, cachePath, gen]() {
        _activeReplies.remove(reply);
        if (gen != _gen) { reply->deleteLater(); _activeThumbs--; kickThumbQueue(); return; }
        _activeThumbs--;
        QNetworkReply::NetworkError err = reply->error();
        QByteArray data = reply->readAll();
        reply->deleteLater();

        if (err != QNetworkReply::NoError || data.isEmpty()) {
            kickThumbQueue();
            return;
        }

        // 缩放并保存
        QImage img;
        if (img.loadFromData(data)) {
            QImage scaled = img.scaled(240, 135, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            scaled.save(cachePath, "JPG", 85);
            if (index >= 0 && index < _podFileInfos.size()) {
                _podFileInfos[index].thumbnailUrl = QUrl::fromLocalFile(cachePath);
                _podFileInfos[index].thumbReady = true;
                emit dataChanged(this->index(index), this->index(index),
                                 { ThumbnailUrlRole, ThumbReadyRole });
            }
        }
        kickThumbQueue();
    });
}

void MediaDownload::fetchVideoThumb(int index)
{
    if (index < 0 || index >= _podFileInfos.size()) {
        _activeThumbs--;
        kickThumbQueue();
        return;
    }
    PodFileInfo& item = _podFileInfos[index];
    QString key = cacheKeyFromUrl(QUrl(item.filePath));
    QString cachePath = thumbCachePathForKey(key);

    if (QFileInfo::exists(cachePath)) {
        item.thumbnailUrl = QUrl::fromLocalFile(cachePath);
        item.thumbReady = true;
        emit dataChanged(this->index(index), this->index(index),
                         { ThumbnailUrlRole, ThumbReadyRole });
        _activeThumbs--;
        kickThumbQueue();
        return;
    }

    QString tmpPath = QDir::tempPath() + "/qgc_thumb_" + key + ".mp4";
    int gen = _gen;
    auto tryFfmpeg = [](const QString& tmpPath, const QString& cachePath) -> bool {
        static const QString ffmpegPath = "D:/Program Files/ffmpeg-7.1.1-full_build-shared/bin/ffmpeg.exe";
        QProcess proc;
        proc.start(ffmpegPath, QStringList()
                    << "-y" << "-i" << tmpPath
                    << "-ss" << "0" << "-vframes" << "1"
                    << "-vf" << "scale=240:-1"
                    << "-q:v" << "5" << "-update" << "1" << cachePath);
        proc.waitForFinished(15000);
        return QFileInfo::exists(cachePath);
    };

    QNetworkRequest req(QUrl(item.filePath));
    req.setRawHeader("Range", "bytes=0-5242880");
    QNetworkReply* reply = _netManager->get(req);
    _activeReplies.insert(reply);
    connect(reply, &QNetworkReply::finished, this, [this, reply, index, cachePath, tmpPath, gen, tryFfmpeg]() {
        _activeReplies.remove(reply);
        if (gen != _gen) { reply->deleteLater(); _activeThumbs--; kickThumbQueue(); return; }
        QByteArray data = reply->readAll();
        reply->deleteLater();
        if (data.isEmpty()) { _activeThumbs--; kickThumbQueue(); return; }

        QFile f(tmpPath);
        if (!f.open(QIODevice::WriteOnly)) { _activeThumbs--; kickThumbQueue(); return; }
        f.write(data);
        f.close();

        bool ok = tryFfmpeg(tmpPath, cachePath);
        QFile::remove(tmpPath);
        _activeThumbs--;

        if (ok) {
            if (index >= 0 && index < _podFileInfos.size()) {
                _podFileInfos[index].thumbnailUrl = QUrl::fromLocalFile(cachePath);
                _podFileInfos[index].thumbReady = true;
                emit dataChanged(this->index(index), this->index(index),
                                 { ThumbnailUrlRole, ThumbReadyRole });
            }
        } else {
            // fallback: 下载完整文件
            qWarning() << "Range thumb failed, downloading full video for index" << index;
            QNetworkRequest req2(QUrl(_podFileInfos[index].filePath));
            QNetworkReply* reply2 = _netManager->get(req2);
            _activeReplies.insert(reply2);
            connect(reply2, &QNetworkReply::finished, this, [this, reply2, index, cachePath, tmpPath, gen, tryFfmpeg]() {
                _activeReplies.remove(reply2);
                if (gen != _gen) { reply2->deleteLater(); _activeThumbs--; kickThumbQueue(); return; }
                QByteArray data2 = reply2->readAll();
                reply2->deleteLater();
                bool ok2 = false;
                if (!data2.isEmpty()) {
                    QFile f2(tmpPath);
                    if (f2.open(QIODevice::WriteOnly)) { f2.write(data2); f2.close(); }
                    ok2 = tryFfmpeg(tmpPath, cachePath);
                    QFile::remove(tmpPath);
                }
                _activeThumbs--;
                if (ok2 && index >= 0 && index < _podFileInfos.size()) {
                    _podFileInfos[index].thumbnailUrl = QUrl::fromLocalFile(cachePath);
                    _podFileInfos[index].thumbReady = true;
                    emit dataChanged(this->index(index), this->index(index),
                                     { ThumbnailUrlRole, ThumbReadyRole });
                } else {
                    qWarning() << "ffmpeg thumb failed for index" << index;
                }
                kickThumbQueue();
            });
        }
        kickThumbQueue();
    });
}
QImage MediaDownload::extractFirstFrame(const QString& localPath)
{
    QMediaPlayer player;
    QVideoSink sink;
    player.setVideoSink(&sink);

    QEventLoop loop;
    bool gotFrame = false;
    QObject::connect(&sink, &QVideoSink::videoFrameChanged, &loop, [&]() {
        gotFrame = true;
        loop.quit();
    });

    player.setSource(QUrl::fromLocalFile(localPath));
    player.play();

    QTimer::singleShot(3000, &loop, &QEventLoop::quit);
    loop.exec();

    if (gotFrame) {
        QVideoFrame frame = sink.videoFrame();
        if (frame.isValid()) {
            return frame.toImage();
        }
    }
    return QImage();
}

// ----------------------- 下载 -----------------------

void MediaDownload::selectAll(bool select)
{
    for (int i = 0; i < _podFileInfos.size(); ++i) {
        _podFileInfos[i].selected = select;
    }
    emit dataChanged(index(0), index(_podFileInfos.size() - 1), { FileSelectedRole });
}

void MediaDownload::toggleSelection(int row, int modifiers)
{
    if (row < 0 || row >= _podFileInfos.size()) return;

    const bool ctrl = (modifiers & 0x04000000) != 0;   // Qt::ControlModifier
    const bool shift = (modifiers & 0x02000000) != 0;  // Qt::ShiftModifier

    if (shift && _lastSelectedRow >= 0 && _lastSelectedRow < _podFileInfos.size()) {
        // Shift：范围选择
        int lo = qMin(_lastSelectedRow, row);
        int hi = qMax(_lastSelectedRow, row);
        for (int i = lo; i <= hi; i++) {
            _podFileInfos[i].selected = true;
        }
        emit dataChanged(index(lo), index(hi), { FileSelectedRole });
    } else if (ctrl) {
        // Ctrl：切换当前项
        _podFileInfos[row].selected = !_podFileInfos[row].selected;
        emit dataChanged(index(row), index(row), { FileSelectedRole });
        _lastSelectedRow = row;
    } else {
        // 普通点击：单选，清除其他
        for (int i = 0; i < _podFileInfos.size(); i++) {
            _podFileInfos[i].selected = (i == row);
        }
        emit dataChanged(index(0), index(_podFileInfos.size() - 1), { FileSelectedRole });
        _lastSelectedRow = row;
    }
}

void MediaDownload::podOpenFile(int row)
{
    if (row < 0 || row >= _podFileInfos.size()) return;
    PodFileInfo& fi = _podFileInfos[row];

    // 计算本地路径
    QUrl u(fi.filePath);
    QStringList segs = u.path().split('/', Qt::SkipEmptyParts);
    QString dateDir = segs.size() >= 2 ? segs[1] : QString();
    QString localFileName = generateLocalFileName(fi.fileName, dateDir);
    QString localPath = _mediaRootFolder + "/" + dateDir + "/" + localFileName;

    if (QFileInfo::exists(localPath)) {
        // 已下载，直接用系统默认程序打开
        QDesktopServices::openUrl(QUrl::fromLocalFile(localPath));
    } else {
        // 未下载：先下载，完成后自动打开
        _openAfterDownloadIndex = row;
        _openLocalPath = localPath;
        // 先确保目录存在
        QDir().mkpath(QFileInfo(localPath).path());
        // 标记该文件选中并开始下载
        for (int i = 0; i < _podFileInfos.size(); i++) _podFileInfos[i].selected = false;
        fi.selected = true;
        fi.downloadProgress = 0;
        emit dataChanged(index(0), index(_podFileInfos.size() - 1), { FileSelectedRole });
        startDownloadFiles();
    }
}

void MediaDownload::startDownloadFiles()
{
    bool hasSelected = false;
    for (const auto& f : _podFileInfos) {
        if (f.selected) { hasSelected = true; break; }
    }
    if (!hasSelected) {
        qDebug() << "startDownloadFiles: nothing selected";
        return;
    }
    downloadFiles();
}

void MediaDownload::downloadFiles()
{
    QString filePath;
    QString remoteFileName;
    QString dateDir;
    for (int i = 0; i < _podFileInfos.size(); i++) {
        PodFileInfo& fi = _podFileInfos[i];
        if (fi.selected && fi.downloadProgress < 1.0f) {
            _downloadFileIndex = i;
            filePath = fi.filePath;
            remoteFileName = fi.fileName;
            // 从 URL 中解析日期目录 /download/YYYY-MM-DD/...
            QUrl u(filePath);
            QStringList segs = u.path().split('/', Qt::SkipEmptyParts);
            if (segs.size() >= 2) dateDir = segs[1];  // YYYY-MM-DD
            break;
        }
    }
    if (filePath.isEmpty()) {
        _downloadFileIndex = -1;
        qDebug() << "downloadFiles: all done";
        emit allDownloadsFinished();
        return;
    }

    QUrl downloadUrl(filePath);
    QString localFileName = generateLocalFileName(remoteFileName, dateDir);
    QString dateFolder = dateDir;  // YYYY-MM-DD
    QString localFullPath = _mediaRootFolder + "/" + dateFolder + "/" + localFileName;
    _downloadFile.setFileName(localFullPath);

    QFileInfo downloadFileInfo(_downloadFile);
    if (!downloadFileInfo.exists()) {
        QDir dir;
        dir.mkpath(downloadFileInfo.path());
    }
    if (!_downloadFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qCritical() << "文件打开失败：" << _downloadFile.fileName();
        // 标记失败并继续下一个
        _podFileInfos[_downloadFileIndex].downloadProgress = 1.0f;
        emit dataChanged(index(_downloadFileIndex), index(_downloadFileIndex), { DownloadProgressRole });
        downloadFiles();
        return;
    }

    qDebug() << "downloadFiles:" << downloadUrl.toString() << "->" << localFullPath;
    _recievedSize = 0;

    QNetworkRequest request(downloadUrl);
    request.setAttribute(QNetworkRequest::MaximumDownloadBufferSizeAttribute, 512 * 1024);

    QNetworkReply* reply = _netManager->get(request);

    connect(reply, &QNetworkReply::readyRead, this, [this, reply]() {
        if (!_downloadFile.isOpen()) return;
        if (reply->bytesAvailable() < _readThreshold) return;
        QByteArray data = reply->readAll();
        qint64 written = _downloadFile.write(data);
        if (written != data.size()) {
            qCritical() << "download write failed";
            reply->abort();
            return;
        }
        _recievedSize += data.size();
    });

    connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 bytesReceived, qint64 bytesTotal) {
        if (bytesTotal <= 0) return;
        _podFileInfos[_downloadFileIndex].downloadProgress = float(bytesReceived) / float(bytesTotal);
        emit dataChanged(index(_downloadFileIndex), index(_downloadFileIndex), { DownloadProgressRole });
    });

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        QNetworkReply::NetworkError err = reply->error();
        if (err == QNetworkReply::NoError) {
            QByteArray tail = reply->readAll();
            _recievedSize += tail.size();
            _downloadFile.write(tail);
            _downloadFile.flush();
            _downloadFile.close();
            _podFileInfos[_downloadFileIndex].downloadProgress = 1.0f;
            emit dataChanged(index(_downloadFileIndex), index(_downloadFileIndex), { DownloadProgressRole });
            qDebug() << "downloadFiles done, size=" << _recievedSize;
            // 双击打开：下载完成后用系统默认程序打开
            if (_openAfterDownloadIndex == _downloadFileIndex && QFileInfo::exists(_openLocalPath)) {
                QDesktopServices::openUrl(QUrl::fromLocalFile(_openLocalPath));
            }
            _openAfterDownloadIndex = -1;
            _openLocalPath.clear();
        } else {
            qWarning() << "download failed:" << err << reply->errorString();
            _downloadFile.close();
            // 标记为完成（避免卡住队列）
            _podFileInfos[_downloadFileIndex].downloadProgress = 1.0f;
            emit dataChanged(index(_downloadFileIndex), index(_downloadFileIndex), { DownloadProgressRole });
        }
        reply->deleteLater();
        downloadFiles();
    });

    _timerDownload.start();
}

QString MediaDownload::generateLocalFileName(const QString& remoteFileName, const QString& dateDir) const
{
    // 远程文件名形如：2026-09-18T11-40-34-084.mp4
    // 本地统一成：  yyyyMMdd_hhmmss.ext
    QString base = remoteFileName;
    QRegularExpression re("^(\\d{4})-(\\d{2})-(\\d{2})T(\\d{2})-(\\d{2})-(\\d{2})(?:-\\d+)?\\.[A-Za-z0-9]+$");
    auto m = re.match(base);
    if (m.hasMatch()) {
        return QString("%1%2%3_%4%5%6.%7")
            .arg(m.captured(1), m.captured(2), m.captured(3),
                 m.captured(4), m.captured(5), m.captured(6),
                 base.section('.', -1).toLower());
    }
    Q_UNUSED(dateDir);
    return base;
}

QString MediaDownload::formatSize(qint64 bytes)
{
    if (bytes < 1024) return QString("%1 B").arg(bytes);
    else if (bytes < 1024*1024) return QString("%1 KB").arg(bytes/1024.0, 0, 'f', 2);
    else if (bytes < 1024*1024*1024) return QString("%1 MB").arg(bytes/(1024.0*1024), 0, 'f', 2);
    else return QString("%1 GB").arg(bytes/(1024.0*1024*1024), 0, 'f', 2);
}

QString MediaDownload::formatSpeed(qint64 bytes)
{
    qint64 elapsed = _timerDownload.elapsed();
    if (elapsed == 0) return "0 B/s";
    double speed = bytes / (elapsed/1000.0);
    return formatSize(speed) + "/s";
}

qint64 MediaDownload::parseHumanSize(const QString& s) const
{
    QRegularExpression re("(\\d+\\.?\\d*)\\s*([KMG]?i?B?)", QRegularExpression::CaseInsensitiveOption);
    auto m = re.match(s.trimmed());
    if (!m.hasMatch()) return 0;
    double num = m.captured(1).toDouble();
    QString unit = m.captured(2).toUpper();
    if (unit.startsWith("K"))      return (qint64)(num * 1024);
    if (unit.startsWith("M"))      return (qint64)(num * 1024.0 * 1024);
    if (unit.startsWith("G"))      return (qint64)(num * 1024.0 * 1024 * 1024);
    return (qint64)num;
}

void MediaDownload::onAuthenticationRequired(QNetworkReply* /*reply*/, QAuthenticator* /*authenticator*/)
{
    // 新文件服务器不需要认证，留空避免默认弹窗
}
