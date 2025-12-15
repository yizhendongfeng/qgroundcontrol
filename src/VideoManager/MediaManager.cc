#include "MediaManager.h"
#include <QUrl>
#include <QUrlQuery>
#include <QFileInfo>
#include <QDebug>
#include <QJsonArray>
#include <QMessageAuthenticationCode>
#include <QDomDocument>
#include <QDomElement>
#include "SettingsManager.h"
#include "CloudServerSettings.h"

MediaManager::MediaManager(QObject *parent)
    : QObject{parent}
{
    // qDebug() << "输出当前QT支持的openSSL版本: " << QSslSocket::sslLibraryBuildVersionString();
    // qDebug() << "OpenSSL支持情况: " <<QSslSocket::supportsSsl();
    // qDebug() << "OpenSSL运行时SSL库版本: " << QSslSocket::sslLibraryBuildVersionString();

    // QNetworkAccessManager *manager = new QNetworkAccessManager(this);
    // qDebug() << manager->supportedSchemes(); // 如果有https就说明已经配置了openssl
    m_manager = new QNetworkAccessManager(this);
    // 允许自动跟随重定向（包括跨协议HTTP→HTTPS）
    // m_manager->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
    // 连接网络错误信号
    // connect(m_manager, &QNetworkAccessManager::finished, this, [this](QNetworkReply* reply) {
    //     if (reply->error() != QNetworkReply::NoError) {
    //         onNetworkError(reply->error());
    //     }
    //     reply->deleteLater();
    // });
    // qDebug() << "empty hash: " << QCryptographicHash::hash(QByteArray(), QCryptographicHash::Sha256).toHex();

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
    QString url = SERVER_BASE_URL + "/media/api/v1/workspaces/" + WORKSPACE_ID + "/fast-upload";
    QNetworkRequest request((QUrl(url)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json; charset=utf-8"); // 必须添加！
    request.setRawHeader("x-auth-token", SettingsManager::instance()->cloudServerSettings()->getToken().toUtf8());

    m_fileMd5 = calculateFileMD5(localFilePath);
    qDebug() << "m_fileMd5: " << m_fileMd5;
    // 构造请求体（按服务端接口要求传参，需与服务端协商字段）

    QJsonObject reqObj;
    QJsonObject ext;
    ext["drone_model_key"]   = "0-77-0";
    ext["is_original"]       = true;
    ext["payload_model_key"] = "1-66-0";
    ext["tinny_fingerprint"] = m_fileMd5;
    ext["sn"]                = "drone001";
    reqObj["ext"]            = ext;
    reqObj["fingerprint"]    = m_fileMd5;
    reqObj["name"]           = fileInfo.absoluteFilePath();
    reqObj["path"]           = "";

    QByteArray reqData = QJsonDocument(reqObj).toJson();
    QNetworkReply* reply = m_manager->post(request, reqData);
    connect(reply, &QNetworkReply::finished, this, [this](){
        QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
        if (!reply) return;
        QJsonParseError parseError;
        QJsonDocument jsonDocReply = QJsonDocument::fromJson(reply->readAll(), &parseError);
        QJsonObject jsonObjReply = jsonDocReply.object();
        if (jsonObjReply["code"].toInt() == -1) { // 服务器中没有该文件
            // 获取临时凭证
            getTemporaryCredential();
        }
    });

    // 初始化参数
    m_fileName = fileInfo.fileName();
    m_credential.objectName = m_fileName;
    m_fileTotalSize = fileInfo.size();
    m_totalUploadedBytes = 0;
    m_chunkList.clear();

    // 计算分块数量并初始化分块信息
    int chunkCount = (m_fileTotalSize + CHUNK_SIZE - 1) / CHUNK_SIZE;
    for (int i = 0; i < chunkCount; ++i) {
        ChunkInfo chunk;
        chunk.partNumber = i + 1;
        chunk.isUploaded = false;
        m_chunkList.append(chunk);
    }

    qDebug() << "开始上传文件：" << m_fileName;
    qDebug() << "文件大小：" << m_fileTotalSize / 1024 / 1024 << "MB";
    qDebug() << "文件MD5：" << m_fileMd5;
}

// 1. 向服务端获取MinIO临时上传凭证
void MediaManager::getTemporaryCredential()
{
    QString url = SERVER_BASE_URL + "/storage/api/v1/workspaces/" + WORKSPACE_ID + "/sts";
    QNetworkRequest request((QUrl(url)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json; charset=utf-8");
    request.setRawHeader("x-auth-token", SettingsManager::instance()->cloudServerSettings()->getToken().toUtf8());

    QNetworkReply* reply = m_manager->post(request, QByteArray());
    connect(reply, &QNetworkReply::finished, this, &MediaManager::onCredentialReplyFinished);
}

// 2. 计算文件MD5指纹
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

// 3. 初始化MinIO分块上传（获取UploadId）
void MediaManager::initMultipartUpload()
{
    QUrlQuery query;
    query.addQueryItem("uploads", "");
    QString url = MINIO_ENDPOINT + "/" + m_credential.bucket + "/" + m_credential.objectName;
    QUrl requestUrl(url);
    requestUrl.setQuery(query);
    QNetworkRequest request(requestUrl);
    qDebug() << "requestUrl.path(): " << requestUrl.path() << requestUrl.query();

    // 构造请求头
    QMap<QString, QString> headers;
    headers["Host"] = QUrl(MINIO_ENDPOINT).authority();
    headers["X-Amz-Security-Token"] = m_credential.sessionToken.trimmed();
    QString amzDate = getIso8601Time();
    headers["X-Amz-Date"] = amzDate;
    headers["Content-Type"] = "application/octet-stream"; // 显式设置，避免Qt默认填充
    headers["Content-Length"] = QString::number(0); //
    qDebug() << "初始化分块上传参数:";
    qDebug() << "URL:" << requestUrl.toString();
    qDebug() << "Bucket:" << m_credential.bucket;
    qDebug() << "Object:" << m_credential.objectName;
    qDebug() << "X-Amz-Date:" << amzDate;
    // 添加S3 V4签名头
    QMap<QString, QString> signedHeaders = s3V4Sign("POST", requestUrl.path(), QByteArray(), headers, requestUrl.query());
    for (auto it = signedHeaders.begin(); it != signedHeaders.end(); ++it) {
        request.setRawHeader(it.key().toUtf8(), it.value().toUtf8());
    }

    // 构造请求参数（初始化分块上传）
    qDebug() << "query.toString():" << query.toString() << request.url();
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
    qint64 chunkOffset = m_currentChunkIndex * CHUNK_SIZE;
    qint64 chunkSize = qMin(CHUNK_SIZE, m_fileTotalSize - chunkOffset);

    // 打开文件并定位到分块起始位置
    if (!m_file.isOpen()) {
        m_file.setFileName(m_localFilePath);
        if (!m_file.open(QIODevice::ReadOnly)) {
            emit uploadError("文件打开失败：" + m_file.errorString());
            return;
        }
    }
    m_file.seek(chunkOffset);
    QByteArray chunkData = m_file.read(chunkSize);

    // 构造MinIO分块上传URL
    QString url = MINIO_ENDPOINT + "/" + m_credential.bucket + "/" + m_credential.objectName;
    QUrlQuery query;
    query.addQueryItem("partNumber", QString::number(currentChunk.partNumber));
    query.addQueryItem("uploadId", m_uploadId);
    QUrl requestUrl(url);
    requestUrl.setQuery(query);
    qDebug() << "uploadNextChunk() requestUrl: " << requestUrl;
    QNetworkRequest request(requestUrl);
    QMap<QString, QString> headers;
    headers["Host"] = QUrl(MINIO_ENDPOINT).authority();
    headers["Content-Type"] = "application/octet-stream"; // 显式设置，避免Qt默认填充
    headers["Content-Length"] = QString::number(chunkData.size());
    headers["X-Amz-Security-Token"] = m_credential.sessionToken.trimmed();
    headers["X-Amz-Date"] = getIso8601Time();
    qDebug() << "chunkData: " ;

    // 添加S3 V4签名头
    QMap<QString, QString> signedHeaders = s3V4Sign("PUT", requestUrl.path(), chunkData, headers, requestUrl.query());
    for (auto it = signedHeaders.begin(); it != signedHeaders.end(); ++it) {
        request.setRawHeader(it.key().toUtf8(), it.value().toUtf8());
    }

    // 发送分块上传请求
    m_currentMinioReply = m_manager->put(request, chunkData);
    connect(m_currentMinioReply, &QNetworkReply::finished, this, &MediaManager::onMinioReplyFinished);
    // connect(m_currentMinioReply, &QNetworkReply::uploadProgress, this, &MediaManager::onChunkUploadProgress);
}

// 5. 完成MinIO分块上传（合并分块）
void MediaManager::completeMultipartUpload()
{
    QString url = MINIO_ENDPOINT + "/" + m_credential.bucket + "/" + m_credential.objectName;
    QUrlQuery query;
    query.addQueryItem("uploadId", m_uploadId);
    // url += "?" + query.toString();

    // 构造合并分块的请求体（JSON格式，包含所有分块的partNumber和ETag）
    // 构造合并分块的请求体（XML格式，这是S3标准格式）
    QString xmlBody = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    xmlBody += "<CompleteMultipartUpload>\n";

    for (const ChunkInfo& chunk : m_chunkList) {
        // 确保ETag格式正确，通常需要引号
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

    // QJsonObject completeObj;
    // QJsonArray partsArray;
    // for (const ChunkInfo& chunk : m_chunkList) {
    //     QJsonObject partObj;
    //     partObj["PartNumber"] = chunk.partNumber;
    //     partObj["ETag"] = QString(chunk.eTag);
    //     partsArray.append(partObj);
    // }
    // completeObj["Parts"] = partsArray;
    QByteArray requestBody = xmlBody.toUtf8();;

    QUrl requestUrl(url);
    requestUrl.setQuery(query);
    QNetworkRequest request(requestUrl);
    QMap<QString, QString> headers;
    headers["Host"] = QUrl(MINIO_ENDPOINT).host();
    headers["Content-Type"] = "application/json";
    headers["Content-Length"] = QString::number(requestBody.size());
    headers["X-Amz-Security-Token"] = m_credential.sessionToken.trimmed();
    headers["X-Amz-Date"] = getIso8601Time();

    // 添加S3 V4签名头
    QMap<QString, QString> signedHeaders = s3V4Sign("POST", requestUrl.path(), requestBody, headers, requestUrl.query());
    for (auto it = signedHeaders.begin(); it != signedHeaders.end(); ++it) {
        request.setRawHeader(it.key().toUtf8(), it.value().toUtf8());
    }

    m_currentMinioReply = m_manager->post(request, requestBody);
    connect(m_currentMinioReply, &QNetworkReply::finished, this, &MediaManager::onMinioReplyFinished);
    connect(m_currentMinioReply, &QNetworkReply::errorOccurred, this, &MediaManager::onMinioError);
}

// 6. 向服务端上报上传结果
void MediaManager::reportUploadResult(const QString &fileMd5)
{
    QString url = SERVER_BASE_URL + "/media/api/v1/workspaces/" + WORKSPACE_ID + "/upload-callback";
    QUrl requestUrl((QUrl(url)));
    QNetworkRequest request(requestUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("x-auth-token", SettingsManager::instance()->cloudServerSettings()->getToken().toUtf8());
    QUrlQuery query;
    query.addQueryItem("workspaceId", WORKSPACE_ID);
    // query.addQueryItem("x-auth-token", QString(token));
    requestUrl.setQuery(query);

    // 构造上报参数（按服务端接口要求传参）
    QJsonObject reqObj;
    reqObj["result"] = 0;
    QJsonObject jsonObjExt;
    jsonObjExt["file_group_id"] = 0;
    jsonObjExt["drone_model_key"] = "0-77-0";
    jsonObjExt["is_original"] = true;
    jsonObjExt["payload_model_key"] = "0-77-0";
    jsonObjExt["tinny_fingerprint"] = m_fileMd5;
    jsonObjExt["sn"] = "dgcs001";

    reqObj["ext"] = jsonObjExt;

    QJsonObject jsonObjMeta;
    jsonObjMeta["absolute_altitude"] = 0;
    jsonObjMeta["created_time"] = "2023-11-15T14:30:45Z";
    jsonObjMeta["gimbal_yaw_degree"] = 0;
    jsonObjMeta["relative_altitude"] = 0;
    QJsonObject jsonObjPos;
    jsonObjPos["lat"] = 22.5799555;
    jsonObjPos["lng"] = 113.9827345;
    jsonObjMeta["shoot_position"] = jsonObjPos;
    reqObj["metadata"] = jsonObjMeta;
    reqObj["fingerprint"] = m_fileMd5;
    reqObj["name"] = m_credential.objectName;
    reqObj["fingerprint"] = fileMd5;  // 文件MD5指纹（服务端校验用）

    reqObj["object_key"] = m_credential.bucket + "/" + m_fileName;  // MinIO中文件路径
    reqObj["path"] = m_credential.bucket;
    reqObj["sub_file_type"] = 0;

    QByteArray reqData = QJsonDocument(reqObj).toJson();
    QNetworkReply* reply = m_manager->post(request, reqData);
    connect(reply, &QNetworkReply::finished, this, &MediaManager::onReportReplyFinished);
}

// 签名函数应该正确处理以下内容：
// 1. HTTP方法 (POST)
// 2. 对象路径
// 3. 查询参数 ("uploads")
// 4. 所有头部（包括Host, X-Amz-Date, X-Amz-Security-Token等）
// 5. 请求体哈希（对于初始化分块上传应该是空字符串的SHA256）
// 7. S3 V4签名实现（核心）
QMap<QString, QString> MediaManager::s3V4Sign(const QString &method, const QString &objectName,
    const QByteArray &requestBody, const QMap<QString, QString> &headers, const QString &queryParams)
{
    // QDateTime now = QDateTime::currentDateTimeUtc();
    // QString date = now.toString("yyyyMMdd");
    // QString isoTime = now.toString("yyyyMMdd'T'HHmmss'Z'");
    QString date = headers["X-Amz-Date"].left(8);
    QString isoTime = headers["X-Amz-Date"];
    QString region = m_credential.region.isEmpty() ? "us-east-1" : m_credential.region;
    QString service = "s3";

    // 步骤1：构造规范请求（CanonicalRequest）
    QString canonicalRequest;
    canonicalRequest += method + "\n";  // HTTP方法（POST/PUT）
    canonicalRequest += QUrl::toPercentEncoding(objectName, "/_.!~*'()") + "\n";  // 编码后的文件路径
    canonicalRequest += queryParams + "\n";  // 查询参数（无则留空）
    // canonicalRequest += QUrl::toPercentEncoding(queryParams, "/_.!~*'()")  + "\n";  // 查询参数（无则留空）

    // 请求体哈希（SHA256）
    // （保留之前的payloadHash、stringToSign、签名计算逻辑）
    QByteArray payloadHash = QCryptographicHash::hash(requestBody, QCryptographicHash::Sha256);
    // headers["x-amz-content-sha256"] = payloadHash;
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
    qDebug() << "***canonicalRequest:" << canonicalRequest;
    // 步骤2：构造待签名字符串（StringToSign）
    QString stringToSign;
    stringToSign += "AWS4-HMAC-SHA256\n";
    stringToSign += isoTime + "\n";
    stringToSign += date + "/" + region + "/" + service + "/aws4_request\n";
    QByteArray canonicalHash = QCryptographicHash::hash(canonicalRequest.toUtf8(), QCryptographicHash::Sha256);
    stringToSign += canonicalHash.toHex();
    // qDebug() << "\\n***stringToSign:" << stringToSign;
    // 步骤3：生成签名密钥（SigningKey）
    QByteArray dateKey = hmacSha256(("AWS4" + m_credential.secretKey).toUtf8(), date.toUtf8());
    QByteArray regionKey = hmacSha256(dateKey, region.toUtf8());
    QByteArray serviceKey = hmacSha256(regionKey, service.toUtf8());
    QByteArray signingKey = hmacSha256(serviceKey, "aws4_request");

    // 步骤4：计算签名
    QByteArray signature = hmacSha256(signingKey, stringToSign.toUtf8());
    // 打印验证，格式必须和下面一致（示例）
    // qDebug() << "StringToSign：" << stringToSign;
    // 正确格式示例：
    // AWS4-HMAC-SHA256
    // 20251112T100000Z
    // 20251112/us-east-1/s3/aws4_request
    // e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
    // 步骤5：构造Authorization头
    QMap<QString, QString> signedHeaders;
    for (auto it = headers.begin(); it != headers.end(); ++it) {
        signedHeaders[it.key()] = it.value();
    }
    QString authHeader = QString("AWS4-HMAC-SHA256 Credential=%1/%2/%3/%4/aws4_request, "
                                 "SignedHeaders=%5, Signature=%6")
                             .arg(m_credential.accessKey)
                             .arg(date)
                             .arg(region)
                             .arg(service)
                             .arg(signedHeadersStr)
                             .arg(signature.toHex());
    signedHeaders["Authorization"] = authHeader;
    signedHeaders["x-amz-content-sha256"] = payloadHash.toHex();
    qDebug() << "signedHeaders" << signedHeaders;
    return signedHeaders;
}

// 8. 生成ISO8601格式时间（例：20251110T123456Z）
QString MediaManager::getIso8601Time()
{
    return QDateTime::currentDateTimeUtc().toString("yyyyMMdd'T'HHmmss'Z'");
}

// 9. 生成YYYYMMDD格式日期
QString MediaManager::getDateStamp()
{
    return QDateTime::currentDateTimeUtc().toString("yyyyMMdd");
}

// 10. HMAC-SHA256加密
QByteArray MediaManager::hmacSha256(const QByteArray &key, const QByteArray &data)
{
    // QMessageAuthenticationCode 专门用于HMAC计算
    QMessageAuthenticationCode mac(QCryptographicHash::Sha256, key);
    mac.addData(data);
    return mac.result();
}

// 获取临时凭证响应处理
void MediaManager::onCredentialReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    QByteArray responseData = reply->readAll();
    qDebug() << "onCredentialReplyFinished: " << responseData;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
    if (!jsonDoc.isObject()) {
        emit uploadError("获取凭证失败：响应格式错误");
        return;
    }

    QJsonObject jsonObj = jsonDoc.object();
    if (jsonObj["code"].toInt() != 0) {
        emit uploadError("获取凭证失败：" + jsonObj["message"].toString());
        return;
    }

    // 解析临时凭证（需与服务端返回字段一致）
    QJsonObject dataObj = jsonObj["data"].toObject();
    m_credential.bucket = dataObj["bucket"].toString() + "/medias";
    m_credential.region = dataObj["region"].toString();
    m_endpoint = dataObj["endpoint"].toString();

    QJsonObject jsonObjCredentials = dataObj["credentials"].toObject();
    m_credential.accessKey = jsonObjCredentials["access_key_id"].toString();
    m_credential.secretKey = jsonObjCredentials["access_key_secret"].toString();
    m_credential.sessionToken = jsonObjCredentials["security_token"].toString();
    m_credential.expiration = jsonObjCredentials["expire"].toInt();
    // m_credential.objectName = "media";

    qDebug() << "accessKey:" << m_credential.accessKey;
    qDebug() << "secretKey:" << m_credential.secretKey;
    qDebug() << "sessionToken:" << m_credential.sessionToken;

    // 验证凭证完整性
    if (m_credential.accessKey.isEmpty() || m_credential.bucket.isEmpty() || m_endpoint.isEmpty()) {
        emit uploadError("获取凭证失败：凭证信息不完整");
        return;
    }

    qDebug() << "临时凭证获取成功，开始初始化MinIO分块上传";
    // 第二步：初始化MinIO分块上传
    initMultipartUpload();
}

// MinIO请求响应处理（分块上传/合并）
void MediaManager::onMinioReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    QByteArray responseData = reply->readAll();
    qDebug() << "*********** onMinioReplyFinished() responseData: " << QString(responseData) ;
    qDebug() << "reply->request().url(): " << reply->request().url();


    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    // 处理初始化分块上传响应（获取UploadId）
    if (reply->request().url().query().contains("uploads")) {
        if (statusCode == 200) {
            QDomDocument domDoc;
            domDoc.setContent(responseData);
            QDomNodeList uploadIds = domDoc.elementsByTagName("UploadId");
            if (!uploadIds.isEmpty()) {
                QDomNode uploadId = uploadIds.at(0);
                m_uploadId = uploadId.firstChild().nodeValue();
                if (m_uploadId.isEmpty()) {
                    emit uploadError("初始化分块上传失败：UploadId为空");
                    return;
                }
                qDebug() << "分块上传初始化成功，UploadId：" << m_uploadId;
                // 开始上传第一个分块
                uploadNextChunk();
            } else {
                qDebug() << "收到的xml响应中没有UploadI";
                emit uploadError("初始化分块上传失败：响应格式错误");
            }
        } else {
            emit uploadError(QString("初始化分块上传失败，状态码：%1，错误：%2").arg(statusCode).arg(QString(responseData)));
        }
        return;
    }

    // 处理分块上传响应（获取ETag）
    if (reply->request().url().query().contains("partNumber")) {
        if (statusCode == 200) {
            ChunkInfo& currentChunk = m_chunkList[m_currentChunkIndex];
            currentChunk.eTag = reply->rawHeader("ETag").trimmed();  // MinIO返回的ETag
            currentChunk.isUploaded = true;
            m_totalUploadedBytes += qMin(CHUNK_SIZE, m_fileTotalSize - m_currentChunkIndex * CHUNK_SIZE);

            // 计算总进度
            int progress = (m_totalUploadedBytes * 100) / m_fileTotalSize;
            emit uploadProgress(m_fileIndex , progress);
            qDebug() << "分块" << currentChunk.partNumber << "上传成功，ETag：" << currentChunk.eTag << "进度：" << progress << "%";

            // 上传下一个分块
            uploadNextChunk();
        } else {
            emit uploadError(QString("分块%1上传失败，状态码：%2，错误：%3").arg(m_chunkList[m_currentChunkIndex].partNumber).arg(statusCode).arg(QString(responseData)));
        }
        return;
    }

    // 处理合并分块响应
    if (reply->request().url().query().contains("uploadId") && !reply->request().url().query().contains("partNumber")) {
        if (statusCode == 200) {
            qDebug() << "分块合并成功，文件上传到MinIO完成";
            // 第三步：向服务端上报结果
            reportUploadResult(m_fileMd5);
        } else {
            emit uploadError(QString("分块合并失败，状态码：%1，错误：%2").arg(statusCode).arg(QString(responseData)));
        }
        return;
    }
}

// 上报结果响应处理
void MediaManager::onReportReplyFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    QByteArray responseData = reply->readAll();
    QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
    if (!jsonDoc.isObject()) {
        emit uploadFinished(m_fileIndex, false, "上报结果失败：响应格式错误");
        return;
    }

    QJsonObject jsonObj = jsonDoc.object();
    if (jsonObj["code"].toInt() == 0) {
        emit uploadFinished(m_fileIndex, true, "文件上传成功！");
        qDebug() << "上报结果成功，服务端已记录文件信息";
    } else {
        emit uploadFinished(m_fileIndex, false, "上报结果失败：" + jsonObj["message"].toString());
    }

    // 关闭文件
    if (m_file.isOpen()) {
        m_file.close();
    }
}

// 网络错误处理
void MediaManager::onNetworkError(QNetworkReply::NetworkError error)
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    QString errorMsg = QString("网络错误：%1，详情：%2").arg(error).arg(reply->errorString());
    emit uploadError(errorMsg);
    qDebug() << errorMsg;

    // 关闭文件
    if (m_file.isOpen()) {
        m_file.close();
    }
}

// 分块上传进度
void MediaManager::onChunkUploadProgress(qint64 bytesSent, qint64 bytesTotal)
{
    // 计算当前分块的实时进度，叠加到总进度
    int chunkProgress = (bytesSent * 100) / bytesTotal;
    int totalProgress = ((m_currentChunkIndex * CHUNK_SIZE) + (bytesSent * CHUNK_SIZE / 100)) * 100 / m_fileTotalSize;
    emit uploadProgress(m_fileIndex,  qMin(totalProgress, 99));  // 留1%给合并和上报
}

void MediaManager::onMinioError(QNetworkReply::NetworkError error)
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    qDebug() << "MinIO网络错误:" << error << reply->errorString();
    emit uploadError(QString("网络错误: %1 - %2").arg(error).arg(reply->errorString()));
}
