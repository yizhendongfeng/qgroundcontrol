#include "MediaDownload.h"
#include <qregularexpression.h>
#include <QUrl>
#include <QNetworkRequest>
#include <QAuthenticator>
#include <QUrlQuery>
#include <QDateTime>
#include "SettingsManager.h"
#include "AppSettings.h"
#include "VideoSettings.h"
#include "Fact.h"

MediaDownload::MediaDownload(QObject* parent) :
    QAbstractListModel{parent},
    _netManager(new QNetworkAccessManager)
{
    // connect(_netManager, &QNetworkAccessManager::finished, this, &MediaDownload::onReplyFinished);
    connect(_netManager, &QNetworkAccessManager::authenticationRequired, this, &MediaDownload::onAuthenticationRequired);
    _mediaRootFolder = SettingsManager::instance()->appSettings()->mediaSavePath();
    QString podIp = SettingsManager::instance()->videoSettings()->podIp()->rawValue().toString();
    urlStr = "http://" + podIp + "/cgi-bin/";
}

int MediaDownload::rowCount(const QModelIndex &parent) const
{
    return _podFileInfos.count();
}

QVariant MediaDownload::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= _podFileInfos.size())
        return {};

    const PodFileInfo &item = _podFileInfos[index.row()];
    switch (role) {
    case FilePathRole: return item.filePath;
    case FileTypeRole: return item.fileType;
    case StartTimeRole:return item.startTime;
    case DurationRole: return item.duration;
    case SizeRole:     return item.fileSize;
    case SizeStrRole:  return item.fileSizeStr;
    case FileSelectedRole: return item.selected;
    case DownloadProgressRole: return item.downloadProgress;
    }
    return {};
}

bool MediaDownload::setData(const QModelIndex &index, const QVariant &value, int role)
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
        { FilePathRole, "filePathStr" },
        { FileTypeRole, "fileTypeStr" },
        { StartTimeRole, "startTimeStr"},
        { DurationRole, "duration" },
        { SizeRole, "fileSize" },
        { SizeStrRole, "fileSizeStr" },
        { FileSelectedRole, "fileSelected" },
        { DownloadProgressRole, "downloadedProgress" }
    };
}

void MediaDownload::refreshMediaInPod(const QString startDateTime, const QString endDateTime)
{
    startFindTime = startDateTime + " 00:00:00";
    endFindTime = endDateTime + " 23:59:59";
    qDebug() << "refreshMediaInPod() startTime:" << startFindTime << ", endTime:" << endFindTime;
    QNetworkRequest request(QUrl(urlStr + "mediaFileFind.cgi?action=factory.create"));
    QNetworkReply* reply = _netManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [&](){
        QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
        qDebug() << "服务器返回的状态码：" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()
                 << reply->error() << reply->errorString();
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray responseData = reply->readAll();
            QString responseStr = QString::fromUtf8(responseData);
            findObjectStr = responseStr.mid(responseStr.indexOf('=') + 1);
            qDebug() << "refreshMediaInPod() onReplyFinished:" << responseStr << ", findObjectStr:" << findObjectStr;
            startFindFile();
        }
        reply->deleteLater();
    });
}

void MediaDownload::startDownloadFiles()
{
    _downloadFileIndex = 0;
    downloadFiles();
}

void MediaDownload::getPodInfo()
{


}

void MediaDownload::startFindFile()
{
    QUrl findUrl(urlStr + "mediaFileFind.cgi");
    QUrlQuery query;
    query.addQueryItem("action", "findFile");
    query.addQueryItem("object", findObjectStr);
    query.addQueryItem("condition.Channel", "0");
    // QString startTime = "2021-1-1 00:00:00";
    // QString endTime = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    query.addQueryItem("condition.StartTime", startFindTime);
    query.addQueryItem("condition.EndTime", endFindTime);
    findUrl.setQuery(query);
    qDebug() << "startFindFile() url:" << findUrl;
    _netManager->clearAccessCache();//清除qt认证缓存，qt默认使用旧缓存，而非一次性缓存
    QNetworkReply* reply = _netManager->get(QNetworkRequest(findUrl));
    connect(reply, &QNetworkReply::finished, this, [&](){
        QNetworkReply* reply = qobject_cast<QNetworkReply *>(sender());
        qDebug() << "startFindFile() onReplyFinished error:" << reply->error() << reply->errorString() << "服务器返回的状态码：" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray responseData = reply->readAll();
            QString responseStr = QString::fromUtf8(responseData).trimmed();
            if (responseStr == "OK") {
                findNextFile();
            } else if(responseStr == "Error") {
                qDebug() << "!!! startFindFile receive error";
            }
            qDebug() << "startFindFile() onReplyFinished:" << responseStr;
        }
        reply->deleteLater();
    });
}

void MediaDownload::findNextFile()
{
    QUrl findNextFileUrl(urlStr + "mediaFileFind.cgi");
    QUrlQuery query;
    query.addQueryItem("action", "findNextFile");
    query.addQueryItem("object", findObjectStr);
    query.addQueryItem("count", QString::number(FINDNEXTFILECOUNT));
    findNextFileUrl.setQuery(query);
    _netManager->clearAccessCache();
    QNetworkReply* reply = _netManager->get(QNetworkRequest(findNextFileUrl));
    connect(reply, &QNetworkReply::finished, this, [&]() {
        QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
        qDebug() << "findNextFile() onReplyFinished error:" << reply->error() << reply->errorString() << "服务器返回的状态码：" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray responseData = reply->readAll();
            QString responseStr = QString::fromUtf8(responseData).trimmed();
            // qDebug() << "findNextFile() onReplyFinished:" << responseStr;
            addPodFileInfo(responseStr);
        }
        reply->deleteLater();
    });
}

void MediaDownload::addPodFileInfo(QString &rawText)
{
    // qDebug() << "addPodFileInfo rawText:" << rawText;
    QList<PodFileInfo> infoList;
    PodFileInfo currentInfo;  // 临时存储当前item数据
    int currentItemIndex = -1;
    int itemCount = 0;
    QRegularExpression itemRegex(R"(items\[(\d+)\]\.)");
    QRegularExpressionMatch match;
    QString cleanText = rawText.replace("\r\n", "\n").replace("\r", "\n");
    QStringList lines = cleanText.split("\n");
    for(const QString& line : lines) {
        QString trimedLine =  line.trimmed();
        if (trimedLine.isEmpty()) continue;
        if (trimedLine.startsWith("found=")) {
            itemCount = trimedLine.mid(trimedLine.indexOf('=') + 1).toInt();
            continue;
        }

        if (trimedLine.contains(itemRegex, &match)) {
            int newItemIndex = match.captured(1).toInt();
            // 若切换到新item，且上一个item有有效数据，加入列表
            if (newItemIndex != currentItemIndex) {
                // 非初始状态（currentItemIndex≥0）且路径有效时保存
                if (currentItemIndex >= 0 && !currentInfo.filePath.isEmpty()) {
                    infoList.append(currentInfo);
                }
                // 重置临时数据，更新当前item索引
                currentInfo = PodFileInfo();
                currentItemIndex = newItemIndex;
            }
        } else {
            return;
        }

        // ---------------- 提取核心字段 ----------------
        // 提取文件路径
        if (trimedLine.contains(".FilePath=")) {
            int equalPos = trimedLine.indexOf('=');
            currentInfo.filePath = trimedLine.mid(equalPos + 1).trimmed();
        }

        // 提取时长（Duration）
        if (trimedLine.contains(".Duration=")) {
            int equalPos = trimedLine.indexOf('=');
            QString durStr = trimedLine.mid(equalPos + 1).trimmed();
            bool ok = false;
            float dur = durStr.toFloat(&ok);
            currentInfo.duration = ok ? dur : 0.0f; // 转换失败设为0
        }

        // 提取文件大小（Length，字节）
        if (trimedLine.contains(".Length=")) {
            int equalPos = trimedLine.indexOf('=');
            QString sizeStr = trimedLine.mid(equalPos + 1).trimmed();
            bool ok = false;
            int size = sizeStr.toInt(&ok);
            currentInfo.fileSize = ok ? size : 0.0f; // 转换失败设为0
            currentInfo.fileSizeStr = formatSize(size);
        }

    }
    // 4. 保存最后一个item（循环结束后无切换触发，需手动保存）
    if (currentItemIndex >= 0 && !currentInfo.filePath.isEmpty()) {
        infoList.append(currentInfo);
    }
    beginInsertRows(QModelIndex(), _podFileInfos.count(), _podFileInfos.count() + infoList.count() - 1);
    _podFileInfos.append(infoList);
    endInsertRows();
    if (itemCount < FINDNEXTFILECOUNT) { // 已经查找结束
        closeFileFinder();
    } else {  // 可能还有
        findNextFile();
    }

    // for (int i = 0; i < _podFileInfos.size(); ++i) {
    //     const PodFileInfo& info = _podFileInfos[i];
    //     qDebug() << QString("Item %1:").arg(i);
    //     qDebug() << "  文件路径：" << info.filePath;
    //     qDebug() << "  时长（秒）：" << info.duration;
    //     qDebug() << "  文件大小（字节）：" << info.fileSize;
    //     // 可选：转换为MB显示（便于阅读）
    //     qDebug() << "  文件大小（MB）：" << QString::asprintf("%.2f", info.fileSize / 1024 / 1024);
    //     qDebug() << "---------------------";
    // }

}

void MediaDownload::closeFileFinder()
{
    QUrl closeUrl(urlStr + "mediaFileFind.cgi");
    QUrlQuery query;
    query.addQueryItem("action", "close");
    query.addQueryItem("object", findObjectStr);
    closeUrl.setQuery(query);
    qDebug() << "closeFileFinder() url:" << closeUrl;
    _netManager->clearAccessCache();//清除qt认证缓存，qt默认使用旧缓存，而非一次性缓存
    QNetworkReply* reply = _netManager->get(QNetworkRequest(closeUrl));
    connect(reply, &QNetworkReply::finished, this, [&](){
        QNetworkReply* reply = qobject_cast<QNetworkReply *>(sender());
        qDebug() << "closeFileFinder() onReplyFinished error:" << reply->error() << reply->errorString() << "服务器返回的状态码：" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray responseData = reply->readAll();
            QString responseStr = QString::fromUtf8(responseData).trimmed();
            if (responseStr == "OK") {
                destroyFileFinder();
            } else if(responseStr == "Error") {
                qDebug() << "closeFileFinder receive error";
            }
            qDebug() << "closeFileFinder() onReplyFinished:" << responseStr;
        }
        reply->deleteLater();
    });
}

void MediaDownload::destroyFileFinder()
{
    QUrl destroyUrl(urlStr + "mediaFileFind.cgi");
    QUrlQuery query;
    query.addQueryItem("action", "destroy");
    query.addQueryItem("object", findObjectStr);
    destroyUrl.setQuery(query);
    qDebug() << "destroyFileFinder() url:" << destroyUrl;
    _netManager->clearAccessCache();//清除qt认证缓存，qt默认使用旧缓存，而非一次性缓存
    QNetworkReply* reply = _netManager->get(QNetworkRequest(destroyUrl));
    connect(reply, &QNetworkReply::finished, this, [&](){
        QNetworkReply* reply = qobject_cast<QNetworkReply *>(sender());
        qDebug() << "destroyFileFinder() onReplyFinished error:" << reply->error() << reply->errorString() << "服务器返回的状态码：" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray responseData = reply->readAll();
            QString responseStr = QString::fromUtf8(responseData).trimmed();
            qDebug() << "destroyFileFinder() onReplyFinished:" << responseStr;
            if (responseStr == "OK") {
                // downloadFiles();
            } else if(responseStr == "Error") {
                qDebug() << "destroyFileFinder receive error";
            }
        }
        reply->deleteLater();
    });
}

void MediaDownload::downloadFiles()
{
    QString filePath;
    for (int i = 0; i < _podFileInfos.size(); i++) {
        PodFileInfo fileInfo = _podFileInfos[i];
        if (fileInfo.selected && fileInfo.downloadProgress == 0) {
            _downloadFileIndex = i;
            filePath = fileInfo.filePath;
            break;
        }
    }
    if (filePath.isEmpty()) {
        _downloadFileIndex = 0; // 已经下载完成，重置索引
        return;
    }
    QUrl downloadUrl(urlStr + "RPC_Loadfile" + filePath);
    QString localFileName = generateLocalFileName(filePath);
    QString fileDate = localFileName.split('_').first();
    _downloadFile.setFileName(_mediaRootFolder + "/" + fileDate + "/" + localFileName);
    qDebug() << "downloadFiles() url:" << downloadUrl << _downloadFile.fileName();
    QFileInfo downloadFileInfo(_downloadFile);
    if (!downloadFileInfo.exists()) {
        QDir dir;
        dir.mkdir(downloadFileInfo.path());
    }
    if (!_downloadFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qCritical() << "文件打开失败：" << _downloadFile.fileName();
    }
    _netManager->clearAccessCache();//清除qt认证缓存，qt默认使用旧缓存，而非一次性缓存
    QNetworkRequest request(downloadUrl);
    // 告诉服务端：支持分块传输（可选，多数服务端默认开启）
    request.setRawHeader("Accept-Encoding", "identity");
    request.setAttribute(QNetworkRequest::MaximumDownloadBufferSizeAttribute, 1024 * 1024); // 1MB
    QNetworkReply* reply = _netManager->get(request);
    connect(reply, &QNetworkReply::readyRead, this, [&]() {
        if(!_downloadFile.isOpen()) return;
        QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
        if (reply->bytesAvailable() < _readThreshold) {
            return;
        }
        QByteArray data = reply->readAll();
        qint64 writeSize = _downloadFile.write(data);
        if (writeSize != data.size()) {
            qCritical() << "downloadFiles()文件写入失败，写入大小：" << writeSize << "实际大小：" << data.size();
            reply->abort();
            return;
        }
        _recievedSize += data.size();
        // qInfo() << "已接收：" << formatSize(_recievedSize)
        //         << " 速度：" << formatSpeed(data.size())
        //         << "写入：" << formatSize(writeSize);
    });
    connect(reply, &QNetworkReply::downloadProgress, this, [&](qint64 bytesReceived, qint64 bytesTotal) {
        _fileSize = bytesTotal;
        _podFileInfos[_downloadFileIndex].downloadProgress = bytesReceived * 1.0 / bytesTotal;
        emit dataChanged(this->index(_downloadFileIndex), this->index(_downloadFileIndex), {DownloadProgressRole});
        // qInfo() <<  "downloadProgress:" << QString::asprintf("%.1f%%", bytesTotal > 0 ? (bytesReceived*100.0/bytesTotal) : 0) << "received:" << bytesReceived << ",bytesTotal:" << bytesTotal << ",_downloadFileIndex:" << _downloadFileIndex << this->index(_downloadFileIndex);
    });

    connect(reply, &QNetworkReply::finished, this, [&](){
        QNetworkReply* reply = qobject_cast<QNetworkReply *>(sender());
        qDebug() << "downloadFiles() onReplyFinished error:" << reply->error() << reply->errorString() << "服务器返回的状态码：" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray responseData = reply->readAll();
            _recievedSize += responseData.size();
            _downloadFile.write(responseData);
            _downloadFile.flush();
            _downloadFile.close();
            // _podFileInfos[_downloadFileIndex].downloadProgress = 1;
            // emit dataChanged(this->index(_downloadFileIndex), this->index(_downloadFileIndex), {DownloadProgressRole});
            downloadFiles();
            qDebug() << "downloadFiles() onReplyFinished remain buffer size:" << responseData.size();
        }
        reply->deleteLater();
    });
    _timerDownload.start(); // 计时，可选
}

QString MediaDownload::generateLocalFileName(const QString str)
{
    QString fileName("");
    if (str.endsWith(".mp4")) {
        QRegularExpression regexFileName(R"(^.*?/(\d{4}-\d{2}-\d{2})/.*?/(\d{2}.\d{2}.\d{2})-.*?\.\w+$)");
        QRegularExpressionMatch matchFileName = regexFileName.match(str);
        matchFileName = regexFileName.match(str);
        if (matchFileName.hasMatch()) {
            fileName = matchFileName.captured(1).remove('-') + "_" + matchFileName.captured(2).remove('.') + ".mp4";
        }
    } else if (str.endsWith(".jpg")) {
        fileName = str.split('/').last();
    }
    return fileName;
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

void MediaDownload::onReplyFinished(QNetworkReply *reply)
{
    qDebug() << "onReplyFinished() error:" << reply->error() << reply->errorString();
    qDebug() << "服务器返回的状态码：" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    qDebug() << "认证相关Header：" << reply->rawHeader("WWW-Authenticate"); // 查看服务器要求的认证类型
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray responseData = reply->readAll();
        QString responseStr = QString::fromUtf8(responseData);

        qDebug() << "onReplyFinished:" << responseStr;
    }
    reply->deleteLater();
}

void MediaDownload::onAuthenticationRequired(QNetworkReply *reply, QAuthenticator *authenticator)
{
    authenticator->setUser("user");
    authenticator->setPassword("0000");

    qDebug() << "触发Digest认证，已填充用户名：" << authenticator->user();
    qDebug() << "认证域(realm)：" << authenticator->realm(); // 可验证是否匹配服务器的realm
}
