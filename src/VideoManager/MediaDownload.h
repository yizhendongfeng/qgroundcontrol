/**
 *  从吊舱中下载视频和图片
 *  步骤：
 *  1.创建Media finder，2.开始查找，3.查找下一个，4.关闭，5.销毁finder，6.下载
 */
#ifndef MEDIADOWNLOAD_H
#define MEDIADOWNLOAD_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QAbstractListModel>
#include <QFile>
#include <QElapsedTimer>

class MediaDownload : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles {
        FilePathRole = Qt::UserRole + 1,
        FileTypeRole,
        StartTimeRole,
        DurationRole, // 视频时长
        SizeRole,
        SizeStrRole, //单位不同B,KB,MB,GB
        FileSelectedRole,
        DownloadProgressRole
    };
    struct PodFileInfo {
        QString filePath;
        QString fileType;
        QString startTime;
        float duration;
        int   fileSize;
        QString fileSizeStr;
        bool  selected;
        float downloadProgress = 0; // 0：未下载，1：已下载
    };

    explicit MediaDownload(QObject* parent = nullptr);


    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    QHash<int, QByteArray> roleNames() const override;


    Q_INVOKABLE void refreshMediaInPod(const QString startDateTime, const QString endDateTime);
    Q_INVOKABLE void startDownloadFiles();
    void downloadFiles();

    /**
     * @brief getPodInfo 主要获取存储信息
     */
    void getPodInfo();

    /**
     * @brief startFindFile 查找吊舱中的文件
     */
    void startFindFile();

    /**
     * @brief findNextFile 查询文件
     */
    void findNextFile();

    void addPodFileInfo(QString& rawText);

    void closeFileFinder();

    void destroyFileFinder();


    QString generateLocalFileName(const QString str);
    QString formatSize(qint64 bytes);
    QString formatSpeed(qint64 bytes);
// signals:
private slots:
    void onReplyFinished(QNetworkReply* reply);
    void onAuthenticationRequired(QNetworkReply *reply, QAuthenticator *authenticator);
private:
    QString urlStr;
    QNetworkAccessManager* _netManager;
    QStringList filesInPod;
    QString findObjectStr; // 用于吊舱查找的对象编号
    QList<PodFileInfo> _podFileInfos;
    int FINDNEXTFILECOUNT = 100;
    int _downloadFileIndex = 0; // 当前需要下载的文件索引
    QFile _downloadFile;
    int _fileSize; // 当前下载文件总大小
    int _recievedSize = 0; // 已接收的当前文件大小
    QString _mediaRootFolder;
    QElapsedTimer _timerDownload;
    int _readThreshold = 1024 * 1024;
    QString startFindTime = "2020-1-1 00:00:00";  // 开始查找日期
    QString endFindTime;    // 结束查找日期
};

#endif  // MEDIADOWNLOAD_H
