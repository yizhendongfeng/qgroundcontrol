/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include <atomic>

#include <QtCore/QLoggingCategory>
#include <QtCore/QMutex>
#include <QtCore/QQueue>
#include <QtCore/QThread>
#include <QtCore/QTimer>
#include <QtCore/QWaitCondition>

#include <glib.h>
#include <gst/gstelement.h>
#include <gst/gstpad.h>

#include "VideoReceiver.h"

Q_DECLARE_LOGGING_CATEGORY(GstVideoReceiverLog)

typedef std::function<void()> Task;

/*===========================================================================*/

class GstVideoWorker : public QThread
{
    Q_OBJECT

public:
    explicit GstVideoWorker(QObject *parent = nullptr);
    ~GstVideoWorker();
    bool needDispatch() const;
    void dispatch(Task task);
    void shutdown();

private:
    void run() final;

    QWaitCondition _taskQueueUpdate;
    QMutex _taskQueueSync;
    QQueue<Task> _taskQueue;
    bool _shutdown = false;
};

/*===========================================================================*/

typedef struct _GstElement GstElement;

class GstVideoReceiver : public VideoReceiver
{
    Q_OBJECT
public:
    explicit GstVideoReceiver(QObject *parent = nullptr);
    ~GstVideoReceiver();


    void startStreaming(const QString &streamUrl, StreamType streamType) override;
    void setLiveClarity(const LIVE_CLARITY liveClarity) override;
    void stopStreaming() override;
    void setBitrate(int biterate = 1000); // kbps
    void setResolution(QSize res = QSize(1080, 720)); // 720p

public slots:
    void start(uint32_t timeout) override;
    void stop() override;
    void startDecoding(void *sink) override;
    void stopDecoding() override;
    void startRecording(const QString &videoFile, FILE_FORMAT format) override;
    void stopRecording() override;
    void takeScreenshot(const QString &imageFile) override;


private slots:
    void _watchdog();
    void _handleEOS();

private:
    GstElement *_makeSource(const QString &input);
    GstElement *_makeDecoder(GstCaps *caps = nullptr, GstElement *videoSink = nullptr);
    GstElement *_makeFileSink(const QString &videoFile, FILE_FORMAT format);
    //创建推流元素链 _makeStreamSink（核心）,根据 RTSP/RTMP 类型构建不同的编码 + 传输链：
    GstElement *_makeStreamSink(const QString &streamUrl, StreamType streamType);
    void _onNewSourcePad(GstPad *pad);
    void _onNewDecoderPad(GstPad *pad);
    bool _addDecoder(GstElement *src);
    bool _addVideoSink(GstPad *pad);
    void _noteTeeFrame();
    void _noteVideoSinkFrame();
    void _noteEndOfStream();
    /// -Unlink the branch from the src pad
    /// -Send an EOS event at the beginning of that branch
    bool _unlinkBranch(GstElement *from);
    void _shutdownDecodingBranch();
    void _shutdownRecordingBranch();
    void _shutdownStreamingBranch();

    bool _needDispatch();
    void _dispatchSignal(Task emitter);

    /// 停止管道并排队延迟重启（指数退避）。`reason` 用于日志定位重连风暴；
    /// 当 autoReconnect() 关闭时仅做干净停止、不重试。
    void _scheduleReconnect(const char *reason);

    /// 返回 _pipeline 的强引用（调用方需 gst_object_unref），管道已拆除时返回 nullptr。
    /// 总线同步消息回调运行在流线程，与工作线程上的 stop() 并发，
    /// 直接解引用 _pipeline 会与 gst_clear_object(&_pipeline) 产生竞态。
    GstElement *_acquirePipelineRef() const;

    static gboolean _onBusMessage(GstBus *bus, GstMessage *message, gpointer user_data);
    static void _onNewPad(GstElement *element, GstPad *pad, gpointer data);
    static void _wrapWithGhostPad(GstElement *element, GstPad *pad, gpointer data);
    static void _linkPad(GstElement *element, GstPad *pad, gpointer data);
    static gboolean _padProbe(GstElement *element, GstPad *pad, gpointer user_data);
    static gboolean _filterParserCaps(GstElement *bin, GstPad *pad, GstElement *element, GstQuery *query, gpointer data);
    static GstPadProbeReturn _teeProbe(GstPad *pad, GstPadProbeInfo *info, gpointer user_data);
    static GstPadProbeReturn _videoSinkProbe(GstPad *pad, GstPadProbeInfo *info, gpointer user_data);
    static GstPadProbeReturn _eosProbe(GstPad *pad, GstPadProbeInfo *info, gpointer user_data);
    static GstPadProbeReturn _keyframeWatch(GstPad *pad, GstPadProbeInfo *info, gpointer user_data);

    GstElement *_decoder = nullptr;
    GstElement *_decoderValve = nullptr;
    GstElement *_fileSink = nullptr;
    GstElement *_pipeline = nullptr;
    GstElement *_recorderValve = nullptr;
    GstElement *_source = nullptr;
    GstElement *_tee = nullptr;
    GstElement *_videoSink = nullptr;

    // 推流分支核心元素
    GstElement *_streamerValve = nullptr;
    GstElement *_streamerSink = nullptr;

    // 推流状态
    bool _streamingOut = false;  // 区分现有 _streaming（接收流）
    QString _streamUrl;          // 推流目标URL
    StreamType _streamType;      // 推流类型（RTSP/RTMP）

    GstVideoWorker *_worker = nullptr;
    mutable QMutex _pipelineMutex;  // 串行化 _pipeline 变更（worker）与 _onBusMessage 读取（流线程）
    std::atomic<int> _reconnectAttempts = 0;     ///< 流线程（_noteTeeFrame）与 GUI 线程（重连 lambda）写入；原子。
    std::atomic<quint64> _reconnectEpoch = 0;    ///< 每次 stop() 递增——挂起的 singleShot lambda 触发前比对，取代显式 pending 标志。
    std::atomic<quint64> _sourceFrameCount = 0;  ///< tee 探针帧计数（流线程），驱动源端心跳日志。
    gulong _teeProbeId = 0;
    gulong _videoSinkProbeId = 0;

    static constexpr const char *_kFileMux[FILE_FORMAT_MAX + 1] = {
        "matroskamux",
        "qtmux",
        "mp4mux"
    };


};
