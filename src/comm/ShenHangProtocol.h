/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include <QObject>
#include <QMutex>
#include <QString>
#include <QTimer>
#include <QFile>
#include <QMap>
#include <QByteArray>
#include <QLoggingCategory>

#include "LinkInterface.h"
#include "QGCMAVLink.h"
#include "QGC.h"
#include "QGCTemporaryFile.h"
#include "QGCToolbox.h"

#include "ShenHangVehicleData.h"

class LinkManager;
class MultiVehicleManager;
class QGCApplication;


#define MAX_PAYLOAD_LEN 64
#define MIN_PROTOCOL_LEN 10
#define HEAD0 0X4D
#define HEAD1 0X45
# define SHENHANG_COMM_NUM_BUFFERS 16

struct ShenHangProtocolStatus {
    uint8_t msgReceived;               ///< Number of received messages
    uint8_t bufferOverrun;             ///< Number of buffer overruns
    uint8_t parseError;                ///< Number of parse errors
    uint8_t currentReceivedSequence;   ///< Sequence number of last packet received
    uint8_t currentSentSequence;       ///< Sequence number of last packet sent
    uint16_t packetReceivedSuccessCount;///< Received packets
    uint16_t packetReceivedDropCount;   ///< Number of packet drops
    uint8_t bufferReceived[256];        /// 接收数据缓冲区
    int32_t receivedBufferLength;
};


struct ShenHangProtocolMessage {
    uint8_t     tyMsg0;
    uint8_t     tyMsg1;
    uint16_t    idSource;
    uint16_t    idTarget;
    uint8_t     payload[MAX_PAYLOAD_LEN];
    uint8_t     ctr;
};

//不同编号数据包负载长度<id,length>
const QMap<uint8_t, uint16_t> payloadLength{{1, 64}, {2, 50}, {3, 48}, {4, 18}, {5, 40},
                                 {128, 20}, {129, 20}, {130, 20}, {131, 20}, {132, 20}, {133, 20}, {134, 20},
                                 {192, 20}, {193, 20}, {194, 20}, {195, 20}, {196, 20}, {197, 20}, {198, 20}};
static uint8_t counter = 0;
Q_DECLARE_LOGGING_CATEGORY(ShenHangProtocolLog)
/**
 * @brief MAVLink micro air vehicle protocol reference implementation.
 *
 * MAVLink is a generic communication protocol for micro air vehicles.
 * for more information, please see the official website: https://mavlink.io
 **/
class ShenHangProtocol : public QGCTool
{
    Q_OBJECT
public:
    ShenHangProtocol(QGCApplication* app, QGCToolbox* toolbox);
    ~ShenHangProtocol();
    static  ShenHangProtocolStatus* ShenHangProtocolGetChannelStatus(uint8_t chan)
    {
        static ShenHangProtocolStatus shenHangProtocolStatusBuffer[SHENHANG_COMM_NUM_BUFFERS];
        return &shenHangProtocolStatusBuffer[chan];
    }

    uint8_t CaculateCrc8(uint8_t buf[], uint16_t len);
    /**
     * @brief Decode 从接收的数据中解析消息
     * @param bufferRsv
     * @param length
     * @param msg
     * @param lengthLeft 缓冲区剩余字节数
     * @return
     */
    void Decode(LinkInterface* link, uint8_t channel, QByteArray bytesReceived, ShenHangProtocolMessage& msg, ShenHangProtocolStatus* shenHangProtocolStatus);

    /**
     * @brief Encode 数据打包
     * @param msg
     * @param buf 打包后缓冲区地址
     * @return 打包后的数据长度
     */
    uint16_t Encode(ShenHangProtocolMessage msg, uint8_t* buf);
    void HandleMessage(LinkInterface* link, uint8_t channel);
    /** @brief Get the human-friendly name of this protocol */
    QString getName();
    /** @brief Get the system id of this application */
    int getSystemId();
    /** @brief Get the component id of this application */
    int getComponentId();
    
    /** @brief Get protocol version check state */
    bool versionCheckEnabled() const {
        return m_enable_version_check;
    }
    /** @brief Get the protocol version */
    int getVersion() {
        return MAVLINK_VERSION;
    }
    /** @brief Get the currently configured protocol version */
    unsigned getCurrentVersion() {
        return _current_version;
    }
    /**
     * Reset the counters for all metadata for this link.
     */
    virtual void resetMetadataForLink(LinkInterface *link);
    
    /// Suspend/Restart logging during replay.
    void suspendLogForReplay(bool suspend);

    /// Set protocol version
    void setVersion(unsigned version);

    // Override from QGCTool
    virtual void setToolbox(QGCToolbox *toolbox);

public slots:
    /** @brief Receive bytes from a communication interface */
    void receiveBytes(LinkInterface* link, QByteArray b);

    /** @brief Log bytes sent from a communication interface */
    void logSentBytes(LinkInterface* link, QByteArray b);
    
    /** @brief Set the system id of this application */
    void setSystemId(int id);

    /** @brief Enable / disable version check */
    void enableVersionCheck(bool enabled);

    /** @brief Load protocol settings */
    void loadSettings();
    /** @brief Store protocol settings */
    void storeSettings();
    
    /// @brief Deletes any log files which are in the temp directory
    static void deleteTempLogFiles(void);
    
    /// Checks for lost log files
    void checkForLostLogFiles(void);

protected:
    bool        m_enable_version_check;                         ///< Enable checking of version match of MAV and QGC
    uint8_t     lastIndex[256];                            ///< Store the last received sequence ID for each system
    uint8_t     firstMessage[256];                         ///< First message flag
    uint64_t    totalReceiveCounter[MAVLINK_COMM_NUM_BUFFERS];  ///< The total number of successfully received messages
    uint64_t    totalLossCounter[MAVLINK_COMM_NUM_BUFFERS];     ///< Total messages lost during transmission.
    float       runningLossPercent[MAVLINK_COMM_NUM_BUFFERS];   ///< Loss rate

    ShenHangProtocolMessage _shenHangProtocolMessage;
    ShenHangProtocolStatus* _shenHangProtocolStatus;

    bool        versionMismatchIgnore;
    int         systemId;
    unsigned    _current_version;
    int         _radio_version_mismatch_count;

signals:
    /// Heartbeat received on link
    void shenHangVehicleTypeInfo(LinkInterface* link, int vehicleId, int componentId, int vehicleFirmwareType, int vehicleType);

    /** @brief Message received and directly copied via signal */
    void messageReceived(LinkInterface* link, mavlink_message_t message);
    /** @brief Message received and directly copied via signal */
    void shenHangMessageReceived(LinkInterface* link, ShenHangProtocolMessage message);
    /** @brief Emitted if version check is enabled / disabled */
    void versionCheckChanged(bool enabled);
    /** @brief Emitted if a message from the protocol should reach the user */
    void protocolStatusMessage(const QString& title, const QString& message);
    /** @brief Emitted if a new system ID was set */
    void systemIdChanged(int systemId);

    void mavlinkMessageStatus(int uasId, uint64_t totalSent, uint64_t totalReceived, uint64_t totalLoss, float lossPercent);
    void shenHangMessageStatus(int uasId, uint64_t totalSent, uint64_t totalReceived, uint64_t totalLoss, float lossPercent);

    /**
     * @brief Emitted if a new radio status packet received
     *
     * @param rxerrors receive errors
     * @param fixed count of error corrected packets
     * @param rssi local signal strength in dBm
     * @param remrssi remote signal strength in dBm
     * @param txbuf how full the tx buffer is as a percentage
     * @param noise background noise level
     * @param remnoise remote background noise level
     */
    void radioStatusChanged(LinkInterface* link, unsigned rxerrors, unsigned fixed, int rssi, int remrssi,
    unsigned txbuf, unsigned noise, unsigned remnoise);
    
    /// Emitted when a temporary telemetry log file is ready for saving
    void saveTelemetryLog(QString tempLogfile);

    /// Emitted when a telemetry log is started to save.
    void checkTelemetrySavePath(void);

private slots:
    void _vehicleCountChanged(void);
    
private:
    bool _closeLogFile(void);
    void _startLogging(void);
    void _stopLogging(void);

    bool _logSuspendError;      ///< true: Logging suspended due to error
    bool _logSuspendReplay;     ///< true: Logging suspended due to replay
    bool _vehicleWasArmed;      ///< true: Vehicle was armed during log sequence

    QGCTemporaryFile    _tempLogFile;            ///< File to log to
    static const char*  _tempLogFileTemplate;    ///< Template for temporary log file
    static const char*  _logFileExtension;       ///< Extension for log files

    LinkManager*            _linkMgr;
    MultiVehicleManager*    _multiVehicleManager;
};

