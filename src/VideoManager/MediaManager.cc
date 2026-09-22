#include "MediaManager.h"
#include <QUrl>
#include <QUrlQuery>
#include <QFileInfo>
#include <QDateTime>
#include <QDebug>
#include <QJsonArray>
#include <QMessageAuthenticationCode>
#include <QDomDocument>
#include <QDomElement>
#include "SettingsManager.h"
#include "CloudServerSettings.h"

namespace {
// DJI 设备/负载型号 key（domain-type-subType）。当前为 Mavic 3E 无人机 + M3E 相机，
// 对应 DJI Cloud API DeviceEnum：M3E="0-77-0"、M3E_CAMERA="1-66-0"。
// 后端会拿 payload_model_key 反查设备型号，非法值会导致上传回调 500，更换机型时需同步修改。
const QString kDroneModelKey   = QStringLiteral("0-77-0");
const QString kPayloadModelKey = QStringLiteral("1-66-0");

// 媒体子文件类型（sub_file_type）：0=普通图，1=全景图
const int kSubFileTypeNormal = 0;
} // namespace

MediaManager::MediaManager(QObject *parent)
    : QObject{parent}
{
    m_manager = new QNetworkAccessManager(this);
}


// 启动完整上传流程（入口）
void MediaManager::startUpload(const QString &localFilePath, const int index)
{
    m_localFilePath = localFilePath;
    m_fileIndex = index;

    QFileInfo fileInfo(localFilePath);
    if (!fileInfo.exists()) {
        emit uploadError("本地文件不存在：" + localFilePath);
        return;
    }

    m_fileName      = fileInfo.fileName();
    m_fileTotalSize = fileInfo.size();
    m_totalUploadedBytes = 0;
    m_fileMd5       = calculateFileMD5(localFilePath);
    m_tinyFingerprint = calculateTinyFingerprint(localFilePath);
    if (m_fileMd5.isEmpty()) {
        emit uploadError("计算文件指纹失败：" + localFilePath);
        return;
    }

    const QString gcsSn = SettingsManager::instance()->cloudServerSettings()->gcsSn()->rawValueString();

    // fast-upload（秒传检查）：按完整指纹判断文件是否已上传过
    QJsonObject ext;
    ext["drone_model_key"]   = kDroneModelKey;
    ext["is_original"]       = true;
    ext["payload_model_key"] = kPayloadModelKey;
    ext["tinny_fingerprint"] = m_tinyFingerprint;
    ext["sn"]                = gcsSn;

    QJsonObject reqObj;
    reqObj["ext"]         = ext;
    reqObj["fingerprint"] = m_fileMd5;
    reqObj["name"]        = m_fileName;
    reqObj["path"]        = ""; // 非航线内拍摄，路径为空

    QNetworkRequest request;
    applyAuthHeader(request);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json; charset=utf-8");
    request.setUrl(serverApiUrl(QStringLiteral("/media/api/v1/workspaces/")
                                + workspaceId() + QStringLiteral("/fast-upload")));

    QNetworkReply* reply = m_manager->post(request, QJsonDocument(reqObj).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QByteArray body = reply->readAll();
        const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QNetworkReply::NetworkError netErr = reply->error();
        const QString netErrString = reply->errorString();
        const QString reqUrl = reply->url().toString();
        reply->deleteLater();

        // 网络层失败或 HTTP 4xx/5xx：请求根本没被服务端正常处理。
        // 此时响应体通常为空或非 JSON，不能默认 code=0，否则会误报"秒传成功"并跳过实际上传。
        if (netErr != QNetworkReply::NoError || httpStatus >= 400) {
            emit uploadFinished(m_fileIndex, false,
                QStringLiteral("秒传检查失败（HTTP %1：%2），请确认云后台地址可达：%3")
                    .arg(httpStatus).arg(netErrString).arg(reqUrl));
            return;
        }

        QJsonParseError parseErr{};
        const QJsonDocument doc = QJsonDocument::fromJson(body, &parseErr);
        if (parseErr.error != QJsonParseError::NoError || !doc.isObject()
                || !doc.object().contains("code")) {
            emit uploadFinished(m_fileIndex, false,
                QStringLiteral("秒传检查响应解析失败：%1").arg(QString::fromUtf8(body).left(200)));
            return;
        }

        const QJsonObject jsonObjReply = doc.object();
        const int code = jsonObjReply["code"].toInt();
        if (code == 0) {
            // 文件已存在（秒传），无需再上传
            qDebug() << "文件已存在于服务器，秒传成功：" << m_fileName;
            emit uploadFinished(m_fileIndex, true, "秒传成功");
        } else if (code == -1) {
            // 服务器中没有该文件，走正常上传流程
            getTemporaryCredential();
        } else {
            emit uploadFinished(m_fileIndex, false, "秒传检查失败：" + jsonObjReply["message"].toString());
        }
    });

    qDebug() << "开始上传文件：" << m_fileName << "serverIp:" << request.url();
    qDebug() << "文件大小：" << m_fileTotalSize / 1024 / 1024 << "MB";
    qDebug() << "文件MD5：" << m_fileMd5;
    qDebug() << "文件小指纹：" << m_tinyFingerprint;
}

// 1. 向服务端获取MinIO临时上传凭证
void MediaManager::getTemporaryCredential()
{
    QNetworkRequest request;
    applyAuthHeader(request);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json; charset=utf-8");
    request.setUrl(serverApiUrl(QStringLiteral("/storage/api/v1/workspaces/")
                                + workspaceId() + QStringLiteral("/sts")));

    QNetworkReply* reply = m_manager->post(request, QByteArray());
    connect(reply, &QNetworkReply::finished, this, &MediaManager::onCredentialReplyFinished);
}

// 2. 计算文件MD5指纹（完整指纹）
QString MediaManager::calculateFileMD5(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        emit uploadError("计算MD5失败：文件打开失败");
        return "";
    }

    QCryptographicHash hash(QCryptographicHash::Md5);
    while (!file.atEnd()) {
        hash.addData(file.read(4096));  // 分批读取，避免占用过多内存
    }
    file.close();

    return hash.result().toHex();
}

// 2b. 计算文件小指纹（前 10KB MD5 + "_" + 拍摄时间 y_M_d_H_m_s，无前导零）
QString MediaManager::calculateTinyFingerprint(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return "";
    }
    const QByteArray head = file.read(TINY_FINGERPRINT_BYTES);
    file.close();

    const QString md5 = QCryptographicHash::hash(head, QCryptographicHash::Md5).toHex();

    // 时间戳：优先用文件修改时间（DJI 文件的时间戳内嵌在文件名里，本地文件用 mtime 近似）
    const QDateTime t = QFileInfo(filePath).lastModified();
    const QString ts = QString("%1_%2_%3_%4_%5_%6")
                           .arg(t.date().year())
                           .arg(t.date().month())
                           .arg(t.date().day())
                           .arg(t.time().hour())
                           .arg(t.time().minute())
                           .arg(t.time().second());
    return md5 + "_" + ts;
}

// 3. 初始化MinIO分块上传（获取UploadId）
void MediaManager::initMultipartUpload()
{
    // 计算分块数量并初始化分块信息
    m_chunkList.clear();
    const int chunkCount = (m_fileTotalSize + CHUNK_SIZE - 1) / CHUNK_SIZE;
    for (int i = 0; i < chunkCount; ++i) {
        ChunkInfo chunk;
        chunk.partNumber = i + 1;
        chunk.isUploaded = false;
        m_chunkList.append(chunk);
    }

    QUrlQuery query;
    query.addQueryItem("uploads", "");
    const QUrl requestUrl = buildMinioUrl(m_objectKey, query);
    QNetworkRequest request(requestUrl);

    QMap<QString, QString> headers;
    headers["Host"] = buildMinioHostHeader();
    headers["X-Amz-Security-Token"] = m_credential.sessionToken.trimmed();
    headers["X-Amz-Date"] = getIso8601Time();
    headers["Content-Type"] = "application/octet-stream";
    headers["Content-Length"] = QString::number(0);

    const QMap<QString, QString> signedHeaders = s3V4Sign("POST", requestUrl.path(), QByteArray(), headers, requestUrl.query());
    for (auto it = signedHeaders.begin(); it != signedHeaders.end(); ++it) {
        request.setRawHeader(it.key().toUtf8(), it.value().toUtf8());
    }

    m_currentMinioReply = m_manager->post(request, QByteArray());
    connect(m_currentMinioReply, &QNetworkReply::finished, this, &MediaManager::onMinioReplyFinished);
}

// 4. 上传单个分块
void MediaManager::uploadNextChunk()
{
    // 找到下一个未上传的分块
    m_currentChunkIndex = -1;
    for (int i = 0; i < m_chunkList.size(); ++i) {
        if (!m_chunkList[i].isUploaded) {
            m_currentChunkIndex = i;
            break;
        }
    }

    // 所有分块上传完成，开始合并
    if (m_currentChunkIndex == -1) {
        completeMultipartUpload();
        return;
    }

    ChunkInfo& currentChunk = m_chunkList[m_currentChunkIndex];
    const qint64 chunkOffset = m_currentChunkIndex * CHUNK_SIZE;
    const qint64 chunkSize = qMin(CHUNK_SIZE, m_fileTotalSize - chunkOffset);

    // 打开文件并定位到分块起始位置
    if (!m_file.isOpen()) {
        m_file.setFileName(m_localFilePath);
        if (!m_file.open(QIODevice::ReadOnly)) {
            emit uploadError("文件打开失败：" + m_file.errorString());
            return;
        }
    }
    m_file.seek(chunkOffset);
    const QByteArray chunkData = m_file.read(chunkSize);

    QUrlQuery query;
    query.addQueryItem("partNumber", QString::number(currentChunk.partNumber));
    query.addQueryItem("uploadId", m_uploadId);
    const QUrl requestUrl = buildMinioUrl(m_objectKey, query);
    QNetworkRequest request(requestUrl);

    QMap<QString, QString> headers;
    headers["Host"] = buildMinioHostHeader();
    headers["Content-Type"] = "application/octet-stream";
    headers["Content-Length"] = QString::number(chunkData.size());
    headers["X-Amz-Security-Token"] = m_credential.sessionToken.trimmed();
    headers["X-Amz-Date"] = getIso8601Time();

    const QMap<QString, QString> signedHeaders = s3V4Sign("PUT", requestUrl.path(), chunkData, headers, requestUrl.query());
    for (auto it = signedHeaders.begin(); it != signedHeaders.end(); ++it) {
        request.setRawHeader(it.key().toUtf8(), it.value().toUtf8());
    }

    m_currentMinioReply = m_manager->put(request, chunkData);
    connect(m_currentMinioReply, &QNetworkReply::finished, this, &MediaManager::onMinioReplyFinished);
}

// 5. 完成MinIO分块上传（合并分块）
void MediaManager::completeMultipartUpload()
{
    QUrlQuery query;
    query.addQueryItem("uploadId", m_uploadId);
    const QUrl requestUrl = buildMinioUrl(m_objectKey, query);

    // 构造合并分块的请求体（S3 标准 XML 格式）
    QString xmlBody = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    xmlBody += "<CompleteMultipartUpload>\n";
    for (const ChunkInfo& chunk : m_chunkList) {
        QString eTag = chunk.eTag;
        if (!eTag.startsWith("\"")) {
            eTag = "\"" + eTag + "\"";
        }
        xmlBody += "  <Part>\n";
        xmlBody += QString("    <PartNumber>%1</PartNumber>\n").arg(chunk.partNumber);
        xmlBody += QString("    <ETag>%1</ETag>\n").arg(eTag);
        xmlBody += "  </Part>\n";
    }
    xmlBody += "</CompleteMultipartUpload>";
    const QByteArray requestBody = xmlBody.toUtf8();

    QNetworkRequest request(requestUrl);
    QMap<QString, QString> headers;
    headers["Host"] = buildMinioHostHeader();
    headers["Content-Type"] = "application/xml";
    headers["Content-Length"] = QString::number(requestBody.size());
    headers["X-Amz-Security-Token"] = m_credential.sessionToken.trimmed();
    headers["X-Amz-Date"] = getIso8601Time();

    const QMap<QString, QString> signedHeaders = s3V4Sign("POST", requestUrl.path(), requestBody, headers, requestUrl.query());
    for (auto it = signedHeaders.begin(); it != signedHeaders.end(); ++it) {
        request.setRawHeader(it.key().toUtf8(), it.value().toUtf8());
    }

    m_currentMinioReply = m_manager->post(request, requestBody);
    connect(m_currentMinioReply, &QNetworkReply::finished, this, &MediaManager::onMinioReplyFinished);
}

// 6. 向服务端上报上传结果（mediafile-upload-result-report）
void MediaManager::reportUploadResult(const QString &fileMd5)
{
    const QString droneSn = SettingsManager::instance()->cloudServerSettings()->droneSn()->rawValueString();

    QJsonObject ext;
    ext["drone_model_key"]   = kDroneModelKey;
    ext["file_group_id"]     = QString(); // 独立文件无分组（@NotNull 需非 null，用空串）
    ext["is_original"]       = true;
    ext["payload_model_key"] = kPayloadModelKey;
    ext["tinny_fingerprint"] = m_tinyFingerprint;
    ext["sn"]                = droneSn;

    QJsonObject metadata;
    // 以下飞行姿态/位置暂无真实遥测来源（MediaManager 不持有 Vehicle），先用占位值，
    // 后续接入遥测后再回读实际数据。
    metadata["absolute_altitude"]  = 0;
    metadata["gimbal_yaw_degree"]  = 0;
    metadata["relative_altitude"]  = 0;
    QJsonObject shootPosition;
    shootPosition["lat"] = 0;
    shootPosition["lng"] = 0;
    metadata["shoot_position"]     = shootPosition;
    metadata["created_time"]       = QFileInfo(m_localFilePath).lastModified().toUTC()
                                         .toString("yyyy-MM-dd'T'HH:mm:ss'Z'");

    // TODO 文件的业务路径，可能跟航线在一起，是所在文件夹名。
    // DJI 规范中 path 表示媒体所属的航线任务文件夹名，非航线拍摄时应为空。
    // 后端 HTTP 回调不做推导，原样写入 media_file.file_path。
    QJsonObject reqObj;
    reqObj["ext"]           = ext;
    reqObj["fingerprint"]   = fileMd5;          // 文件完整 MD5 指纹（服务端校验/去重）
    reqObj["name"]          = m_fileName;       // 文件名
    reqObj["path"]          = "";               // 非航线内拍摄，路径为空
    reqObj["object_key"]    = m_objectKey;      // MinIO 中的对象 key（与实际上传一致）
    reqObj["sub_file_type"] = kSubFileTypeNormal;
    reqObj["metadata"]      = metadata;

    QNetworkRequest request;
    applyAuthHeader(request);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json; charset=utf-8");
    request.setUrl(serverApiUrl(QStringLiteral("/media/api/v1/workspaces/")
                                + workspaceId() + QStringLiteral("/upload-callback")));

    QNetworkReply* reply = m_manager->post(request, QJsonDocument(reqObj).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, &MediaManager::onReportReplyFinished);
}

// 批量查询已存在的小指纹（obtain-exited-tiny-fingerprint）
void MediaManager::checkTinyFingerprints(const QStringList& tinyFingerprints)
{
    QJsonArray arr;
    for (const QString& fp : tinyFingerprints) {
        arr.append(fp);
    }
    QJsonObject reqObj;
    reqObj["tiny_fingerprints"] = arr;

    QNetworkRequest request;
    applyAuthHeader(request);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json; charset=utf-8");
    request.setUrl(serverApiUrl(QStringLiteral("/media/api/v1/workspaces/")
                                + workspaceId() + QStringLiteral("/files/tiny-fingerprints")));

    QNetworkReply* reply = m_manager->post(request, QJsonDocument(reqObj).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, &MediaManager::onTinyFingerprintReplyFinished);
}

// 分组上传完成回调（group-upload-callback）
void MediaManager::reportGroupUploadResult(const QString& fileGroupId, int fileCount, int fileUploadedCount)
{
    QJsonObject reqObj;
    reqObj["file_group_id"]        = fileGroupId;
    reqObj["file_count"]           = fileCount;
    reqObj["file_uploaded_count"]  = fileUploadedCount;

    QNetworkRequest request;
    applyAuthHeader(request);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json; charset=utf-8");
    request.setUrl(serverApiUrl(QStringLiteral("/media/api/v1/workspaces/")
                                + workspaceId() + QStringLiteral("/group-upload-callback")));

    QNetworkReply* reply = m_manager->post(request, QJsonDocument(reqObj).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [reply]() {
        const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        qDebug() << "group-upload-callback result:" << obj;
        reply->deleteLater();
    });
}

// 7. S3 V4签名实现（核心）
QMap<QString, QString> MediaManager::s3V4Sign(const QString &method, const QString &objectName,
    const QByteArray &requestBody, const QMap<QString, QString> &headers, const QString &queryParams)
{
    const QString date = headers["X-Amz-Date"].left(8);
    const QString isoTime = headers["X-Amz-Date"];
    const QString region = m_credential.region.isEmpty() ? "us-east-1" : m_credential.region;
    const QString service = "s3";

    // 步骤1：构造规范请求（CanonicalRequest）
    QString canonicalRequest;
    canonicalRequest += method + "\n";  // HTTP方法（POST/PUT）
    canonicalRequest += QUrl::toPercentEncoding(objectName, "/_.!~*'()") + "\n";  // 编码后的路径
    canonicalRequest += queryParams + "\n";  // 查询参数

    // 请求体哈希（SHA256）
    const QByteArray payloadHash = QCryptographicHash::hash(requestBody, QCryptographicHash::Sha256);

    // 规范请求头（按字母排序，小写key）
    QMap<QString, QString> sortedHeaders;
    for (auto it = headers.begin(); it != headers.end(); ++it) {
        sortedHeaders[it.key().toLower()] = it.value().trimmed();
    }
    QString signedHeadersStr;
    for (auto it = sortedHeaders.begin(); it != sortedHeaders.end(); ++it) {
        canonicalRequest += it.key() + ":" + it.value() + "\n";
        signedHeadersStr += (signedHeadersStr.isEmpty() ? "" : ";") + it.key();
    }
    canonicalRequest += "\n";  // 空行
    canonicalRequest += signedHeadersStr + "\n";  // 签名头列表
    canonicalRequest += payloadHash.toHex();

    // 步骤2：构造待签名字符串（StringToSign）
    QString stringToSign;
    stringToSign += "AWS4-HMAC-SHA256\n";
    stringToSign += isoTime + "\n";
    stringToSign += date + "/" + region + "/" + service + "/aws4_request\n";
    const QByteArray canonicalHash = QCryptographicHash::hash(canonicalRequest.toUtf8(), QCryptographicHash::Sha256);
    stringToSign += canonicalHash.toHex();

    // 步骤3：生成签名密钥（SigningKey）
    const QByteArray dateKey = hmacSha256(("AWS4" + m_credential.secretKey).toUtf8(), date.toUtf8());
    const QByteArray regionKey = hmacSha256(dateKey, region.toUtf8());
    const QByteArray serviceKey = hmacSha256(regionKey, service.toUtf8());
    const QByteArray signingKey = hmacSha256(serviceKey, "aws4_request");

    // 步骤4：计算签名
    const QByteArray signature = hmacSha256(signingKey, stringToSign.toUtf8());

    // 步骤5：构造Authorization头
    QMap<QString, QString> signedHeaders;
    for (auto it = headers.begin(); it != headers.end(); ++it) {
        signedHeaders[it.key()] = it.value();
    }
    const QString authHeader = QString("AWS4-HMAC-SHA256 Credential=%1/%2/%3/%4/aws4_request, "
                                       "SignedHeaders=%5, Signature=%6")
                                   .arg(m_credential.accessKey)
                                   .arg(date)
                                   .arg(region)
                                   .arg(service)
                                   .arg(signedHeadersStr)
                                   .arg(signature.toHex());
    signedHeaders["Authorization"] = authHeader;
    signedHeaders["x-amz-content-sha256"] = payloadHash.toHex();
    return signedHeaders;
}

// 8. 生成ISO8601格式时间（例：20251110T123456Z）
QString MediaManager::getIso8601Time()
{
    return QDateTime::currentDateTimeUtc().toString("yyyyMMdd'T'HHmmss'Z'");
}

// 10. HMAC-SHA256加密
QByteArray MediaManager::hmacSha256(const QByteArray &key, const QByteArray &data)
{
    QMessageAuthenticationCode mac(QCryptographicHash::Sha256, key);
    mac.addData(data);
    return mac.result();
}

// 组装对象存储 URL（host/port 从 STS endpoint 派生）
QUrl MediaManager::buildMinioUrl(const QString& objectKey, const QUrlQuery& query) const
{
    QUrl url(m_minioBaseUrl + "/" + m_credential.bucket + "/" + objectKey);
    if (!query.isEmpty()) {
        url.setQuery(query);
    }
    return url;
}

QString MediaManager::buildMinioHostHeader() const
{
    return QUrl(m_minioBaseUrl).authority();
}

// 统一设置 x-auth-token 请求头
void MediaManager::applyAuthHeader(QNetworkRequest& request) const
{
    request.setRawHeader("x-auth-token",
                         SettingsManager::instance()->cloudServerSettings()->serverToken()->rawValueString().toUtf8());
}

QUrl MediaManager::serverApiUrl(const QString& path) const
{
    const QString serverIp = SettingsManager::instance()->cloudServerSettings()->serverIp()->rawValueString();
    return QUrl("http://" + serverIp + ":" + QString::number(m_serverPort) + path);
}

QString MediaManager::workspaceId() const
{
    return SettingsManager::instance()->cloudServerSettings()->workSpaceId()->rawValueString();
}

// 获取临时凭证响应处理
void MediaManager::onCredentialReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    const QByteArray responseData = reply->readAll();
    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QNetworkReply::NetworkError netErr = reply->error();
    const QString netErrString = reply->errorString();
    reply->deleteLater();
    qDebug() << "onCredentialReplyFinished:" << responseData;

    if (netErr != QNetworkReply::NoError || httpStatus >= 400) {
        emit uploadFinished(m_fileIndex, false,
            QStringLiteral("获取临时凭证失败（HTTP %1：%2）").arg(httpStatus).arg(netErrString));
        return;
    }

    const QJsonObject jsonObj = QJsonDocument::fromJson(responseData).object();
    if (jsonObj["code"].toInt() != 0) {
        emit uploadFinished(m_fileIndex, false, "获取凭证失败：" + jsonObj["message"].toString());
        return;
    }

    // 解析临时凭证（与服务端 StsCredentialsResponse 字段一致）
    const QJsonObject dataObj = jsonObj["data"].toObject();
    m_credential.bucket  = dataObj["bucket"].toString();
    m_credential.region  = dataObj["region"].toString();
    m_credential.endpoint = dataObj["endpoint"].toString();
    m_credential.objectKeyPrefix = dataObj["object_key_prefix"].toString();

    const QJsonObject credentials = dataObj["credentials"].toObject();
    m_credential.accessKey    = credentials["access_key_id"].toString();
    m_credential.secretKey    = credentials["access_key_secret"].toString();
    m_credential.sessionToken = credentials["security_token"].toString();
    m_credential.expiration   = credentials["expire"].toInt();

    // 对象 key = object_key_prefix + "/" + 文件名（无前缀则直接用文件名）
    if (m_credential.objectKeyPrefix.isEmpty()) {
        m_objectKey = m_fileName;
    } else {
        m_objectKey = m_credential.objectKeyPrefix + "/" + m_fileName;
    }

    // 从 endpoint 解析出 base url（http(s)://host:port），后续 MinIO 请求都用它
    const QUrl endpointUrl(m_credential.endpoint);
    m_minioBaseUrl = endpointUrl.scheme() + "://" + endpointUrl.authority();

    // 验证凭证完整性
    if (m_credential.accessKey.isEmpty() || m_credential.bucket.isEmpty()
            || m_minioBaseUrl.isEmpty()) {
        emit uploadFinished(m_fileIndex, false, "获取凭证失败：凭证信息不完整");
        return;
    }

    qDebug() << "临时凭证获取成功，开始初始化MinIO分块上传，objectKey:" << m_objectKey;
    initMultipartUpload();
}

// MinIO请求响应处理（分块上传/合并）
void MediaManager::onMinioReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    const QByteArray responseData = reply->readAll();
    const QUrlQuery query(reply->request().url());
    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    // 网络层失败（连接拒绝/超时）时无 HTTP 状态码，响应体为空，用 errorString 补充可读信息
    const QString errDetail = statusCode == 0 ? reply->errorString() : QString::fromUtf8(responseData);
    reply->deleteLater();

    // 处理初始化分块上传响应（获取UploadId）
    if (query.hasQueryItem("uploads")) {
        if (statusCode == 200) {
            QDomDocument domDoc;
            domDoc.setContent(responseData);
            const QDomNodeList uploadIds = domDoc.elementsByTagName("UploadId");
            if (!uploadIds.isEmpty()) {
                m_uploadId = uploadIds.at(0).firstChild().nodeValue();
                if (m_uploadId.isEmpty()) {
                    emit uploadFinished(m_fileIndex, false, "初始化分块上传失败：UploadId为空");
                    return;
                }
                qDebug() << "分块上传初始化成功，UploadId：" << m_uploadId;
                uploadNextChunk();
            } else {
                emit uploadFinished(m_fileIndex, false, "初始化分块上传失败：响应格式错误");
            }
        } else {
            emit uploadFinished(m_fileIndex, false,
                QString("初始化分块上传失败，状态码：%1，错误：%2").arg(statusCode).arg(errDetail));
        }
        return;
    }

    // 处理分块上传响应（获取ETag）
    if (query.hasQueryItem("partNumber")) {
        if (statusCode == 200) {
            ChunkInfo& currentChunk = m_chunkList[m_currentChunkIndex];
            currentChunk.eTag = reply->rawHeader("ETag").trimmed();
            currentChunk.isUploaded = true;
            m_totalUploadedBytes += qMin(CHUNK_SIZE, m_fileTotalSize - m_currentChunkIndex * CHUNK_SIZE);

            const int progress = (m_totalUploadedBytes * 100) / m_fileTotalSize;
            emit uploadProgress(m_fileIndex, progress);
            qDebug() << "分块" << currentChunk.partNumber << "上传成功，ETag：" << currentChunk.eTag
                     << "进度：" << progress << "%";

            uploadNextChunk();
        } else {
            emit uploadFinished(m_fileIndex, false,
                QString("分块%1上传失败，状态码：%2，错误：%3")
                    .arg(m_chunkList[m_currentChunkIndex].partNumber).arg(statusCode).arg(errDetail));
        }
        return;
    }

    // 处理合并分块响应
    if (query.hasQueryItem("uploadId")) {
        if (statusCode == 200) {
            qDebug() << "分块合并成功，文件上传到MinIO完成";
            reportUploadResult(m_fileMd5);
        } else {
            emit uploadFinished(m_fileIndex, false,
                QString("分块合并失败，状态码：%1，错误：%2").arg(statusCode).arg(errDetail));
        }
        return;
    }
}

// 上报结果响应处理
void MediaManager::onReportReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    const QByteArray responseData = reply->readAll();
    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QNetworkReply::NetworkError netErr = reply->error();
    const QString netErrString = reply->errorString();
    reply->deleteLater();

    // 网络层失败或 HTTP 4xx/5xx：不能默认 code=0 误报"上传成功"。
    if (netErr != QNetworkReply::NoError || httpStatus >= 400) {
        emit uploadFinished(m_fileIndex, false,
            QStringLiteral("上报结果失败（HTTP %1：%2），文件已传 MinIO 但服务端未记录")
                .arg(httpStatus).arg(netErrString));
    } else {
        const QJsonObject jsonObj = QJsonDocument::fromJson(responseData).object();
        if (jsonObj["code"].toInt() == 0) {
            emit uploadFinished(m_fileIndex, true, "文件上传成功！");
            qDebug() << "上报结果成功，服务端已记录文件信息";
        } else {
            emit uploadFinished(m_fileIndex, false, "上报结果失败：" + jsonObj["message"].toString());
        }
    }

    if (m_file.isOpen()) {
        m_file.close();
    }
}

// 小指纹查询响应处理
void MediaManager::onTinyFingerprintReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        emit tinyFingerprintsChecked(QStringList());
        return;
    }
    const QByteArray responseData = reply->readAll();
    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QNetworkReply::NetworkError netErr = reply->error();
    const QString netErrString = reply->errorString();
    reply->deleteLater();

    // 查询失败一律回一个空结果：调用方（MediaFileModel）用该信号复位"查询中"标志，
    // 这里若直接 return 会让标志永远挂住，之后不再核对服务器文件
    if (netErr != QNetworkReply::NoError || httpStatus >= 400) {
        qWarning() << "查询服务器已有文件失败（HTTP" << httpStatus << "：" << netErrString
                   << "），本次不做标记";
        emit tinyFingerprintsChecked(QStringList());
        return;
    }

    const QJsonObject jsonObj = QJsonDocument::fromJson(responseData).object();
    if (jsonObj["code"].toInt() != 0) {
        emit uploadError("查询小指纹失败：" + jsonObj["message"].toString());
        emit tinyFingerprintsChecked(QStringList());
        return;
    }

    const QJsonArray arr = jsonObj["data"].toObject()["tiny_fingerprints"].toArray();
    QStringList existing;
    for (const QJsonValue& v : arr) {
        existing << v.toString();
    }
    emit tinyFingerprintsChecked(existing);
}

// 分块上传进度
void MediaManager::onChunkUploadProgress(qint64 bytesSent, qint64 bytesTotal)
{
    const int chunkProgress = (bytesSent * 100) / bytesTotal;
    const int totalProgress = ((m_currentChunkIndex * CHUNK_SIZE) + (bytesSent * CHUNK_SIZE / 100)) * 100 / m_fileTotalSize;
    emit uploadProgress(m_fileIndex, qMin(totalProgress, 99));  // 留1%给合并和上报
}

void MediaManager::onMinioError(QNetworkReply::NetworkError error)
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    const QString errorMsg = reply ? reply->errorString() : QString();
    qDebug() << "MinIO网络错误:" << error << errorMsg;
    emit uploadFinished(m_fileIndex, false, QString("MinIO 网络错误: %1 - %2").arg(error).arg(errorMsg));
}
