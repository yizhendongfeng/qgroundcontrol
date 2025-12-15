#ifndef MEDIAMANAGER_H
#define MEDIAMANAGER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCryptographicHash>
#include <QTimer>

// 配置参数
const QString SERVER_BASE_URL = "http://192.168.31.208:6789";
const QString MINIO_ENDPOINT  = "http://192.168.31.208:9000";
const QString WORKSPACE_ID    = "e3dea0f5-37f2-4d79-ae58-490af3228069";
const qint64  CHUNK_SIZE      = 10 * 1024 * 1024;


// 服务器返回的临时凭证
struct TempCredential {
    QString endpoint;
    QString accessKey;
    QString secretKey;
    QString sessionToken;
    QString bucket;       // minio文件捅名
    QString objectName;   // minio中文件存储路径
    QString region;       // minio区域，默认不校验区域
    qint64  expiration;   // 凭证过期时间（秒）
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


signals:
    // 上传进度信号（总进度0-100）
    void uploadProgress(const int index, const int progress);
    // 上传结果信号（成功/失败，消息）
    void uploadFinished(const int index, const bool success, const QString& message);
    // 错误信号
    void uploadError(const QString& errorMsg);

private slots:
    // 网络请求响应槽函数
    void onCredentialReplyFinished();    // 获取临时凭证响应
    void onMinioReplyFinished();         // MinIO请求响应（分块上传/合并）
    void onReportReplyFinished();         // 上报结果响应
    // 网络错误槽函数
    void onNetworkError(QNetworkReply::NetworkError error);
    // 分块上传进度槽函数
    void onChunkUploadProgress(qint64 bytesSent, qint64 bytesTotal);
    void onMinioError(QNetworkReply::NetworkError error);
private:
    // 1. 向服务端获取MinIO临时上传凭证
    void getTemporaryCredential();
    // 2. 计算文件MD5指纹（用于服务端校验）
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
    // 9. 生成YYYYMMDD格式日期（S3签名需要）
    QString getDateStamp();
    // 10. HMAC-SHA256加密（S3签名核心）
    QByteArray hmacSha256(const QByteArray& key, const QByteArray& data);

private:
    QNetworkAccessManager* m_manager;    // 网络请求管理器
    QString m_localFilePath;             // 本地文件路径
    QString m_fileName;                  // 文件名
    int     m_fileIndex = -1;            // 文件在MediaModel中的索引，当文件上传成功，需要将此索引发送到MediaModel中来设置已上传成功标识
    qint64 m_fileTotalSize;              // 文件总大小
    QString m_fileMd5;                   // 文件MD5指纹
    TempCredential m_credential;         // 临时凭证
    QString m_endpoint;
    QFile m_file;                        // 待上传文件
    QString m_uploadId;                  // MinIO分块上传ID（初始化分块时获取）
    QList<ChunkInfo> m_chunkList;        // 分块信息列表
    int m_currentChunkIndex;             // 当前正在上传的分块索引
    int m_totalUploadedBytes;            // 已上传总字节数
    QNetworkReply* m_currentMinioReply;  // 当前MinIO请求响应对象

    QByteArray token{
        "eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzI1NiJ9.eyJ3b3Jrc3BhY2VfaWQiOiJlM2RlYTBmNS0zN2YyLTRkNzktYWU1OC00OTBhZjMyMjgwNjkiLCJzdWIiOiJDbG91ZEFwaVNhbXBsZSIsInVzZXJfdHlwZSI6IjIiLCJuYmYiOjE3NjQ4OTQxNzIsImxvZyI6IkxvZ2dlcltjb20uZGppLnNhbXBsZS5jb21tb24ubW9kZWwuQ3VzdG9tQ2xhaW1dIiwiaXNzIjoiREpJIiwiaWQiOiJiZTdjNmMzZC1hZmU5LTRiZTQtYjllYi1jNTUwNjZjMDkxNGUiLCJleHAiOjE3NjQ5ODA1NzIsImlhdCI6MTc2NDg5NDE3MiwidXNlcm5hbWUiOiJwaWxvdCJ9.sZbwy8cLkyga2wS4F_0QQIPEd-zqUwvbkeEv4DNdW3o"
    };

};

#endif // MEDIAMANAGER_H
