#ifndef MEDIAMANAGER_H
#define MEDIAMANAGER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCryptographicHash>
#include <QUrl>
#include <QUrlQuery>
#include <QMap>
#include <QList>
#include <QStringList>

// 配置参数
const qint64  CHUNK_SIZE            = 10 * 1024 * 1024;   // MinIO 分块大小（10MB）
const qint64  TINY_FINGERPRINT_BYTES = 10 * 1024;         // 小指纹取样字节数（文件前 10KB）


// 服务器返回的临时凭证
struct TempCredential {
    QString endpoint;       // 对象存储 endpoint（如 http://192.168.144.110:9000）
    QString accessKey;
    QString secretKey;
    QString sessionToken;
    QString bucket;         // minio 桶名
    QString objectKeyPrefix; // 对象存储路径前缀（STS 返回的 object_key_prefix，如 "wayline"）
    QString region;         // minio 区域
    qint64  expiration;     // 凭证过期时间（秒）
};

// 分块上传状态结构体
struct ChunkInfo {
    int partNumber;          // 分块编号（从1开始）
    QByteArray eTag;         // 分块上传后MinIO返回的ETag（用于合并分块）
    bool isUploaded;         // 是否上传成功
};


class MediaManager : public QObject
{
    Q_OBJECT
public:
    explicit MediaManager(QObject *parent = nullptr);

    // 启动完整上传流程（入口方法）
    void startUpload(const QString& localFilePath, const int index);

    // 批量查询已存在的小指纹（obtain-exited-tiny-fingerprint）
    void checkTinyFingerprints(const QStringList& tinyFingerprints);

    // 计算文件小指纹（前 10KB MD5 + "_" + 拍摄时间 y_M_d_H_m_s，无前导零）
    // 纯函数，供 MediaFileModel 批量核对"服务器上已有文件"时复用
    static QString calculateTinyFingerprint(const QString& filePath);

    // 分组上传完成回调（group-upload-callback）
    void reportGroupUploadResult(const QString& fileGroupId, int fileCount, int fileUploadedCount);

signals:
    // 上传进度信号（总进度0-100）
    void uploadProgress(const int index, const int progress);
    // 上传结果信号（成功/失败，消息）
    void uploadFinished(const int index, const bool success, const QString& message);
    // 错误信号
    void uploadError(const QString& errorMsg);
    // 小指纹查询结果（返回已存在于服务器的小指纹列表）
    void tinyFingerprintsChecked(const QStringList& existingFingerprints);

private slots:
    // 网络请求响应槽函数
    void onCredentialReplyFinished();    // 获取临时凭证响应
    void onMinioReplyFinished();         // MinIO请求响应（分块上传/合并）
    void onReportReplyFinished();        // 上报结果响应
    void onTinyFingerprintReplyFinished(); // 小指纹查询响应
    // 分块上传进度槽函数
    void onChunkUploadProgress(qint64 bytesSent, qint64 bytesTotal);
    void onMinioError(QNetworkReply::NetworkError error);
private:
    // 1. 向服务端获取MinIO临时上传凭证
    void getTemporaryCredential();
    // 2. 计算文件MD5指纹（完整指纹，用于服务端秒传/去重）
    QString calculateFileMD5(const QString& filePath);
    // 3. 初始化MinIO分块上传（获取UploadId）
    void initMultipartUpload();
    // 4. 上传单个分块
    void uploadNextChunk();
    // 5. 完成MinIO分块上传（合并分块）
    void completeMultipartUpload();
    // 6. 向服务端上报上传结果
    void reportUploadResult(const QString& fileMd5);
    // 7. S3 V4签名（MinIO兼容S3，必须签名请求）
    QMap<QString, QString> s3V4Sign(const QString& method, const QString& objectName,
                                    const QByteArray& requestBody, const QMap<QString, QString>& headers,
                                    const QString &queryParams = "");
    // 8. 生成ISO8601格式时间（S3签名需要）
    QString getIso8601Time();
    // 10. HMAC-SHA256加密（S3签名核心）
    QByteArray hmacSha256(const QByteArray& key, const QByteArray& data);

private:
    // 组装对象存储请求（URL 与 Host 头都从 STS endpoint 派生，不再写死端口）
    QUrl    buildMinioUrl(const QString& objectKey, const QUrlQuery& query = QUrlQuery()) const;
    QString buildMinioHostHeader() const;
    // 统一的 x-auth-token 请求头
    void applyAuthHeader(QNetworkRequest& request) const;
    // 组装云后台 API URL（http://serverIp:6789 + path）
    QUrl serverApiUrl(const QString& path) const;
    QString workspaceId() const;

private:
    QNetworkAccessManager* m_manager;    // 网络请求管理器
    QString m_localFilePath;             // 本地文件路径
    QString m_fileName;                  // 文件名
    int     m_fileIndex = -1;            // 文件在MediaModel中的索引，当文件上传成功，需要将此索引发送到MediaModel中来设置已上传成功标识
    qint64 m_fileTotalSize;              // 文件总大小
    QString m_fileMd5;                   // 文件MD5指纹（完整）
    QString m_tinyFingerprint;           // 文件小指纹（前10KB MD5 + 时间戳）
    TempCredential m_credential;         // 临时凭证
    QString m_minioBaseUrl;              // 对象存储 base url（含协议/host/port，从 endpoint 解析）
    QString m_objectKey;                 // 对象存储中的完整 key（prefix + "/" + 文件名）
    QFile m_file;                        // 待上传文件
    QString m_uploadId;                  // MinIO分块上传ID（初始化分块时获取）
    QList<ChunkInfo> m_chunkList;        // 分块信息列表
    int m_currentChunkIndex;             // 当前正在上传的分块索引
    int m_totalUploadedBytes;            // 已上传总字节数
    QNetworkReply* m_currentMinioReply;  // 当前MinIO请求响应对象

    int m_serverPort = 6789;             // DJI 云后台 API 端口（media/storage）
};

#endif // MEDIAMANAGER_H
