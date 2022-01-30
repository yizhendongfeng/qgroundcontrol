/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include <inttypes.h>
#include <iostream>

#include <QDebug>
#include <QTime>
#include <QApplication>
#include <QSettings>
#include <QStandardPaths>
#include <QtEndian>
#include <QMetaType>
#include <QDir>
#include <QFileInfo>

#include "ShenHangProtocol.h"
#include "UASInterface.h"
#include "UASInterface.h"
#include "UAS.h"
#include "LinkManager.h"
#include "QGCMAVLink.h"
#include "QGC.h"
#include "QGCApplication.h"
#include "QGCLoggingCategory.h"
#include "MultiVehicleManager.h"
#include "SettingsManager.h"


static const uint8_t tableCrc8[256] =
{   ////reversed, 8-bit, polynomial=0x07
    0x00, 0x91, 0xE3, 0x72, 0x07, 0x96, 0xE4, 0x75,
    0x0E, 0x9F, 0xED, 0x7C, 0x09, 0x98, 0xEA, 0x7B,
    0x1C, 0x8D, 0xFF, 0x6E, 0x1B, 0x8A, 0xF8, 0x69,
    0x12, 0x83, 0xF1, 0x60, 0x15, 0x84, 0xF6, 0x67,
    0x38, 0xA9, 0xDB, 0x4A, 0x3F, 0xAE, 0xDC, 0x4D,
    0x36, 0xA7, 0xD5, 0x44, 0x31, 0xA0, 0xD2, 0x43,
    0x24, 0xB5, 0xC7, 0x56, 0x23, 0xB2, 0xC0, 0x51,
    0x2A, 0xBB, 0xC9, 0x58, 0x2D, 0xBC, 0xCE, 0x5F,
    0x70, 0xE1, 0x93, 0x02, 0x77, 0xE6, 0x94, 0x05,
    0x7E, 0xEF, 0x9D, 0x0C, 0x79, 0xE8, 0x9A, 0x0B,
    0x6C, 0xFD, 0x8F, 0x1E, 0x6B, 0xFA, 0x88, 0x19,
    0x62, 0xF3, 0x81, 0x10, 0x65, 0xF4, 0x86, 0x17,
    0x48, 0xD9, 0xAB, 0x3A, 0x4F, 0xDE, 0xAC, 0x3D,
    0x46, 0xD7, 0xA5, 0x34, 0x41, 0xD0, 0xA2, 0x33,
    0x54, 0xC5, 0xB7, 0x26, 0x53, 0xC2, 0xB0, 0x21,
    0x5A, 0xCB, 0xB9, 0x28, 0x5D, 0xCC, 0xBE, 0x2F,
    0xE0, 0x71, 0x03, 0x92, 0xE7, 0x76, 0x04, 0x95,
    0xEE, 0x7F, 0x0D, 0x9C, 0xE9, 0x78, 0x0A, 0x9B,
    0xFC, 0x6D, 0x1F, 0x8E, 0xFB, 0x6A, 0x18, 0x89,
    0xF2, 0x63, 0x11, 0x80, 0xF5, 0x64, 0x16, 0x87,
    0xD8, 0x49, 0x3B, 0xAA, 0xDF, 0x4E, 0x3C, 0xAD,
    0xD6, 0x47, 0x35, 0xA4, 0xD1, 0x40, 0x32, 0xA3,
    0xC4, 0x55, 0x27, 0xB6, 0xC3, 0x52, 0x20, 0xB1,
    0xCA, 0x5B, 0x29, 0xB8, 0xCD, 0x5C, 0x2E, 0xBF,
    0x90, 0x01, 0x73, 0xE2, 0x97, 0x06, 0x74, 0xE5,
    0x9E, 0x0F, 0x7D, 0xEC, 0x99, 0x08, 0x7A, 0xEB,
    0x8C, 0x1D, 0x6F, 0xFE, 0x8B, 0x1A, 0x68, 0xF9,
    0x82, 0x13, 0x61, 0xF0, 0x85, 0x14, 0x66, 0xF7,
    0xA8, 0x39, 0x4B, 0xDA, 0xAF, 0x3E, 0x4C, 0xDD,
    0xA6, 0x37, 0x45, 0xD4, 0xA1, 0x30, 0x42, 0xD3,
    0xB4, 0x25, 0x57, 0xC6, 0xB3, 0x22, 0x50, 0xC1,
    0xBA, 0x2B, 0x59, 0xC8, 0xBD, 0x2C, 0x5E, 0xCF
};


Q_DECLARE_METATYPE(mavlink_message_t)

QGC_LOGGING_CATEGORY(ShenHangProtocolLog, "ShenHangProtocolLog")

const char* ShenHangProtocol::_tempLogFileTemplate   = "FlightDataXXXXXX";   ///< Template for temporary log file
const char* ShenHangProtocol::_logFileExtension      = "mavlink";            ///< Extension for log files

/**
 * The default constructor will create a new MAVLink object sending heartbeats at
 * the MAVLINK_HEARTBEAT_DEFAULT_RATE to all connected links.
 */
ShenHangProtocol::ShenHangProtocol(QGCApplication* app, QGCToolbox* toolbox)
    : QGCTool(app, toolbox)
    , m_enable_version_check(true)
    , _shenHangProtocolMessage({})
    , _shenHangProtocolStatus(nullptr)
    , versionMismatchIgnore(false)
    , systemId(255)
    , _current_version(100)
    , _radio_version_mismatch_count(0)
    , _logSuspendError(false)
    , _logSuspendReplay(false)
    , _vehicleWasArmed(false)
    , _tempLogFile(QString("%2.%3").arg(_tempLogFileTemplate).arg(_logFileExtension))
    , _linkMgr(nullptr)
    , _multiVehicleManager(nullptr)
{
    memset(totalReceiveCounter, 0, sizeof(totalReceiveCounter));
    memset(totalLossCounter,    0, sizeof(totalLossCounter));
    memset(runningLossPercent,  0, sizeof(runningLossPercent));
    memset(firstMessage,        1, sizeof(firstMessage));
    memset(&_shenHangProtocolStatus,  0, sizeof(_shenHangProtocolStatus));
    memset(&_shenHangProtocolMessage,  0, sizeof(_shenHangProtocolMessage));
}

ShenHangProtocol::~ShenHangProtocol()
{
    storeSettings();
    _closeLogFile();
}

uint8_t ShenHangProtocol::CaculateCrc8(uint8_t buf[], uint16_t len)
{
    uint8_t fcs = 0xFF;
    uint16_t i;
    for (i = 0; i < len; i++)
    {
        fcs = tableCrc8[fcs ^ buf[i]];
    }
    return (0xFF - fcs);
}

void ShenHangProtocol::Decode(LinkInterface* link, uint8_t channel, QByteArray bytesReceived, ShenHangProtocolMessage& msg, ShenHangProtocolStatus* shenHangProtocolStatus)
{
    if (bytesReceived.length() <= 0)
        return;
    shenHangProtocolStatus = ShenHangProtocolGetChannelStatus(channel);
    memcpy(shenHangProtocolStatus->bufferReceived + shenHangProtocolStatus->receivedBufferLength, bytesReceived.data(), static_cast<size_t>(bytesReceived.length()));
    shenHangProtocolStatus->receivedBufferLength += bytesReceived.length();
    if (shenHangProtocolStatus->receivedBufferLength < MIN_PROTOCOL_LEN) {
        return;
    }

    uint16_t i = 0;
    bool findHeader = false;
    while (i < shenHangProtocolStatus->receivedBufferLength) {
        if (!findHeader && (i + 1 <= shenHangProtocolStatus->receivedBufferLength)
                && shenHangProtocolStatus->bufferReceived[i] == HEAD0
                && shenHangProtocolStatus->bufferReceived[i + 1] == HEAD1) {     // 找到帧头
            findHeader = true;
        }

        if (!findHeader) {
            if (i == shenHangProtocolStatus->receivedBufferLength - 1
                    && shenHangProtocolStatus->bufferReceived[i] == HEAD0) { // 如果最后一个字节是帧头第一个字节，保存在缓冲区中
                break;
            }
            i++;
            continue;
        } else if (i + 2 <= shenHangProtocolStatus->receivedBufferLength) {/// 2.找到了帧头，读取第三个字节（消息类型）
            msg.tyMsg0 = shenHangProtocolStatus->bufferReceived[i + 2];
            if (i + MIN_PROTOCOL_LEN + payloadLength.value(msg.tyMsg0) <= shenHangProtocolStatus->receivedBufferLength) {// 足够一包数据
                findHeader = false; // 不论校验通过还是失败都要重新找帧头
                msg.tyMsg1 = shenHangProtocolStatus->bufferReceived[i + 3];
                memcpy(&msg.idSource, shenHangProtocolStatus->bufferReceived + i + 4, 2);
                memcpy(&msg.idTarget, shenHangProtocolStatus->bufferReceived + i + 6, 2);
                memcpy(msg.payload, shenHangProtocolStatus->bufferReceived + i + 8, payloadLength.value(msg.tyMsg0));
                msg.ctr = shenHangProtocolStatus->bufferReceived[i + 8 + payloadLength.value(msg.tyMsg0)];
                uint8_t CaculatedCrc = CaculateCrc8(shenHangProtocolStatus->bufferReceived + i, MIN_PROTOCOL_LEN - 1 + payloadLength.value(msg.tyMsg0));
                if (CaculatedCrc == shenHangProtocolStatus->bufferReceived[i + 9 + payloadLength.value(msg.tyMsg0)]) {
                    HandleMessage(link, channel);
                    i += MIN_PROTOCOL_LEN + payloadLength.value(msg.tyMsg0);
                } else {
                    i++;
                }
            } else {
                break;
            }
        }
    }
    // 不足一包数据
    memcpy(shenHangProtocolStatus->bufferReceived, shenHangProtocolStatus->bufferReceived + i,
           static_cast<size_t>(shenHangProtocolStatus->receivedBufferLength - i));
    shenHangProtocolStatus->receivedBufferLength -= i;
}

uint16_t ShenHangProtocol::Encode(ShenHangProtocolMessage msg, uint8_t* buf)
{
    buf[0] = HEAD0;
    buf[1] = HEAD1;
    buf[2] = msg.tyMsg0;
    buf[3] = msg.tyMsg1;
    memcpy(buf + 4, &msg.idSource, 2);
    memcpy(buf + 6, &msg.idTarget, 2);
    memcpy(buf + 8, &msg.payload, payloadLength.value(msg.tyMsg0));
    buf[8 + payloadLength.value(msg.tyMsg0)] = counter++;
    buf[9 + payloadLength.value(msg.tyMsg0)] = CaculateCrc8(buf, 9 + payloadLength.value(msg.tyMsg0));
//    qDebug() << QByteArray((char*)(buf) , 10 + payloadLength.value(msg.tyMsg0)).toHex(' ');
    return 10 + payloadLength.value(msg.tyMsg0);
}

void ShenHangProtocol::HandleMessage(LinkInterface* link, uint8_t channel)
{
    //-----------------------------------------------------------------
    // MAVLink Status
    uint8_t lastSeq = lastIndex[_shenHangProtocolMessage.idSource];
    uint8_t expectedSeq = lastSeq + 1;
    // Increase receive counter
    totalReceiveCounter[channel]++;
    // Determine what the next expected sequence number is, accounting for
    // never having seen a message for this system/component pair.
    if(firstMessage[_shenHangProtocolMessage.idSource]) {
        firstMessage[_shenHangProtocolMessage.idSource] = 0;
        lastSeq     = _shenHangProtocolMessage.ctr;
        expectedSeq = _shenHangProtocolMessage.ctr;
    }
    // And if we didn't encounter that sequence number, record the error
    //int foo = 0;
    if (_shenHangProtocolMessage.ctr != expectedSeq)
    {
        //foo = 1;
        int lostMessages = 0;
        //-- Account for overflow during packet loss
        if(_shenHangProtocolMessage.ctr < expectedSeq) {
            lostMessages = (_shenHangProtocolMessage.ctr + 255) - expectedSeq;
        } else {
            lostMessages = _shenHangProtocolMessage.ctr - expectedSeq;
        }
        // Log how many were lost
        totalLossCounter[channel] += static_cast<uint64_t>(lostMessages);
    }

    // And update the last sequence number for this system/component pair
    lastIndex[_shenHangProtocolMessage.idSource] = _shenHangProtocolMessage.ctr;
    // Calculate new loss ratio
    uint64_t totalSent = totalReceiveCounter[channel] + totalLossCounter[channel];
    float receiveLossPercent = static_cast<float>(static_cast<double>(totalLossCounter[channel]) / static_cast<double>(totalSent));
    receiveLossPercent *= 100.0f;
    receiveLossPercent = (receiveLossPercent * 0.5f) + (runningLossPercent[channel] * 0.5f);
    runningLossPercent[channel] = receiveLossPercent;
    // Update MAVLink status on every 32th packet
    if ((totalReceiveCounter[channel] & 0x1F) == 0) {
        emit mavlinkMessageStatus(_shenHangProtocolMessage.idSource, totalSent, totalReceiveCounter[channel], totalLossCounter[channel], receiveLossPercent);
    }
    if (_shenHangProtocolMessage.tyMsg0 == GENERAL_STATUS)  // 获取无人机类型
    {
        GeneralStatus generalStatus;
        memcpy(&generalStatus, _shenHangProtocolMessage.payload, sizeof(generalStatus));
        emit shenHangVehicleTypeInfo(link, _shenHangProtocolMessage.idSource, MAV_COMP_ID_AUTOPILOT1, MAV_AUTOPILOT_SHEN_HANG, generalStatus.tyObj);
    }
    emit shenHangMessageReceived(link, _shenHangProtocolMessage);
}

void ShenHangProtocol::setVersion(unsigned version)
{
    QList<SharedLinkInterfacePtr> sharedLinks = _linkMgr->links();

    for (int i = 0; i < sharedLinks.length(); i++) {
        mavlink_status_t* mavlinkStatus = mavlink_get_channel_status(sharedLinks[i].get()->mavlinkChannel());

        // Set flags for version
        if (version < 200) {
            mavlinkStatus->flags |= MAVLINK_STATUS_FLAG_OUT_MAVLINK1;
        } else {
            mavlinkStatus->flags &= ~MAVLINK_STATUS_FLAG_OUT_MAVLINK1;
        }
    }

    _current_version = version;
}

void ShenHangProtocol::setToolbox(QGCToolbox *toolbox)
{
   QGCTool::setToolbox(toolbox);

   _linkMgr =               _toolbox->linkManager();
   _multiVehicleManager =   _toolbox->multiVehicleManager();

   qRegisterMetaType<mavlink_message_t>("mavlink_message_t");

   loadSettings();

   // All the *Counter variables are not initialized here, as they should be initialized
   // on a per-link basis before those links are used. @see resetMetadataForLink().

   connect(this, &ShenHangProtocol::protocolStatusMessage,   _app, &QGCApplication::criticalMessageBoxOnMainThread);
   connect(this, &ShenHangProtocol::saveTelemetryLog,        _app, &QGCApplication::saveTelemetryLogOnMainThread);
   connect(this, &ShenHangProtocol::checkTelemetrySavePath,  _app, &QGCApplication::checkTelemetrySavePathOnMainThread);

   connect(_multiVehicleManager, &MultiVehicleManager::vehicleAdded, this, &ShenHangProtocol::_vehicleCountChanged);
   connect(_multiVehicleManager, &MultiVehicleManager::vehicleRemoved, this, &ShenHangProtocol::_vehicleCountChanged);

   emit versionCheckChanged(m_enable_version_check);
}

void ShenHangProtocol::loadSettings()
{
    // Load defaults from settings
    QSettings settings;
    settings.beginGroup("QGC_MAVLINK_PROTOCOL");
    enableVersionCheck(settings.value("VERSION_CHECK_ENABLED", m_enable_version_check).toBool());

    // Only set system id if it was valid
    int temp = settings.value("GCS_SYSTEM_ID", systemId).toInt();
    if (temp > 0 && temp < 256)
    {
        systemId = temp;
    }
}

void ShenHangProtocol::storeSettings()
{
    // Store settings
    QSettings settings;
    settings.beginGroup("QGC_MAVLINK_PROTOCOL");
    settings.setValue("VERSION_CHECK_ENABLED", m_enable_version_check);
    settings.setValue("GCS_SYSTEM_ID", systemId);
    // Parameter interface settings
}

void ShenHangProtocol::resetMetadataForLink(LinkInterface *link)
{
    int channel = link->mavlinkChannel();
    totalReceiveCounter[channel] = 0;
    totalLossCounter[channel]    = 0;
    runningLossPercent[channel]  = 0.0f;
    for(int i = 0; i < 256; i++) {
        firstMessage[channel] =  1;
    }
    link->setDecodedFirstMavlinkPacket(false);
}

/**
 * This method parses all outcoming bytes and log a MAVLink packet.
 * @param link The interface to read from
 * @see LinkInterface
 **/

void ShenHangProtocol::logSentBytes(LinkInterface* link, QByteArray b){

    uint8_t bytes_time[sizeof(quint64)];

    Q_UNUSED(link);
    if (!_logSuspendError && !_logSuspendReplay && _tempLogFile.isOpen()) {

        quint64 time = static_cast<quint64>(QDateTime::currentMSecsSinceEpoch() * 1000);

        qToBigEndian(time,bytes_time);

        b.insert(0,QByteArray((const char*)bytes_time,sizeof(bytes_time)));

        int len = b.count();

        if(_tempLogFile.write(b) != len)
        {
            // If there's an error logging data, raise an alert and stop logging.
            emit protocolStatusMessage(tr("MAVLink Protocol"), tr("MAVLink Logging failed. Could not write to file %1, logging disabled.").arg(_tempLogFile.fileName()));
            _stopLogging();
            _logSuspendError = true;
        }
    }

}

/**
 * This method parses all incoming bytes and constructs a MAVLink packet.
 * It can handle multiple links in parallel, as each link has it's own buffer/
 * parsing state machine.
 * @param link The interface to read from
 * @see LinkInterface
 **/

void ShenHangProtocol::receiveBytes(LinkInterface* link, QByteArray b)
{
    // Since receiveBytes signals cross threads we can end up with signals in the queue
    // that come through after the link is disconnected. For these we just drop the data
    // since the link is closed.
    SharedLinkInterfacePtr linkPtr = _linkMgr->sharedLinkInterfacePointerForLink(link, true);
    if (!linkPtr) {
        qCDebug(ShenHangProtocolLog) << "receiveBytes: link gone!" << b.size() << " bytes arrived too late";
        return;
    }

    uint8_t shenHangProtocollinkChannel = link->shenHangProtocolChannel();
    qCDebug(ShenHangProtocolLog) << QString("receiveBytes from link channel:%1, size:%2").arg(shenHangProtocollinkChannel).arg(b.size());
    while (b.size() > 0) {
        _shenHangProtocolStatus = ShenHangProtocolGetChannelStatus(shenHangProtocollinkChannel);
        int32_t lengthLeft = sizeof (_shenHangProtocolStatus->bufferReceived) - _shenHangProtocolStatus->receivedBufferLength;
        int32_t length = lengthLeft < b.size() ? lengthLeft : b.size();
        QByteArray bytes = b.left(length);
        b.remove(0, length);
        Decode(link, shenHangProtocollinkChannel, bytes, _shenHangProtocolMessage, _shenHangProtocolStatus);
        {
#if 0
            //-----------------------------------------------------------------
            // MAVLink Status
            uint8_t lastSeq = lastIndex[_shenHangProtocolMessage.idSource];
            uint8_t expectedSeq = lastSeq + 1;
            // Increase receive counter
            totalReceiveCounter[shenHangProtocollinkChannel]++;
            // Determine what the next expected sequence number is, accounting for
            // never having seen a message for this system/component pair.
            if(firstMessage[_shenHangProtocolMessage.idSource]) {
                firstMessage[_shenHangProtocolMessage.idSource] = 0;
                lastSeq     = _shenHangProtocolMessage.ctr;
                expectedSeq = _shenHangProtocolMessage.ctr;
            }
            // And if we didn't encounter that sequence number, record the error
            //int foo = 0;
            if (_shenHangProtocolMessage.ctr != expectedSeq)
            {
                //foo = 1;
                int lostMessages = 0;
                //-- Account for overflow during packet loss
                if(_shenHangProtocolMessage.ctr < expectedSeq) {
                    lostMessages = (_shenHangProtocolMessage.ctr + 255) - expectedSeq;
                } else {
                    lostMessages = _shenHangProtocolMessage.ctr - expectedSeq;
                }
                // Log how many were lost
                totalLossCounter[shenHangProtocollinkChannel] += static_cast<uint64_t>(lostMessages);
            }

            // And update the last sequence number for this system/component pair
            lastIndex[_shenHangProtocolMessage.idSource] = _shenHangProtocolMessage.ctr;
            // Calculate new loss ratio
            uint64_t totalSent = totalReceiveCounter[shenHangProtocollinkChannel] + totalLossCounter[shenHangProtocollinkChannel];
            float receiveLossPercent = static_cast<float>(static_cast<double>(totalLossCounter[shenHangProtocollinkChannel]) / static_cast<double>(totalSent));
            receiveLossPercent *= 100.0f;
            receiveLossPercent = (receiveLossPercent * 0.5f) + (runningLossPercent[shenHangProtocollinkChannel] * 0.5f);
            runningLossPercent[shenHangProtocollinkChannel] = receiveLossPercent;
            // Update MAVLink status on every 32th packet
            if ((totalReceiveCounter[shenHangProtocollinkChannel] & 0x1F) == 0) {
                emit mavlinkMessageStatus(_shenHangProtocolMessage.idSource, totalSent, totalReceiveCounter[shenHangProtocollinkChannel], totalLossCounter[shenHangProtocollinkChannel], receiveLossPercent);
            }
            if (_shenHangProtocolMessage.tyMsg0 == GENERAL_STATUS)  // 获取无人机类型
            {
                GeneralStatus generalStatus;
                memcpy(&generalStatus, _shenHangProtocolMessage.payload, sizeof(generalStatus));
                emit shenHangVehicleTypeInfo(link, _shenHangProtocolMessage.idSource, MAV_COMP_ID_AUTOPILOT1, MAV_AUTOPILOT_SHEN_HANG, generalStatus.tyObj);
            }
            emit shenHangMessageReceived(link, _shenHangProtocolMessage);
#endif
        }
    }
}

/**
 * @return The name of this protocol
 **/
QString ShenHangProtocol::getName()
{
    return tr("ShenHang protocol");
}

/** @return System id of this application */
int ShenHangProtocol::getSystemId()
{
    return systemId;
}

void ShenHangProtocol::setSystemId(int id)
{
    systemId = id;
}

/** @return Component id of this application */
int ShenHangProtocol::getComponentId()
{
    return MAV_COMP_ID_MISSIONPLANNER;
}

void ShenHangProtocol::enableVersionCheck(bool enabled)
{
    m_enable_version_check = enabled;
    emit versionCheckChanged(enabled);
}

void ShenHangProtocol::_vehicleCountChanged(void)
{
    int count = _multiVehicleManager->vehicles()->count();
    if (count == 0) {
        // Last vehicle is gone, close out logging
        _stopLogging();
        _radio_version_mismatch_count = 0;
    }
}

/// @brief Closes the log file if it is open
bool ShenHangProtocol::_closeLogFile(void)
{
    if (_tempLogFile.isOpen()) {
        if (_tempLogFile.size() == 0) {
            // Don't save zero byte files
            _tempLogFile.remove();
            return false;
        } else {
            _tempLogFile.flush();
            _tempLogFile.close();
            return true;
        }
    }
    return false;
}

void ShenHangProtocol::_startLogging(void)
{
    //-- Are we supposed to write logs?
    if (qgcApp()->runningUnitTests()) {
        return;
    }
    AppSettings* appSettings = _app->toolbox()->settingsManager()->appSettings();
    if(appSettings->disableAllPersistence()->rawValue().toBool()) {
        return;
    }
#ifdef __mobile__
    //-- Mobile build don't write to /tmp unless told to do so
    if (!appSettings->telemetrySave()->rawValue().toBool()) {
        return;
    }
#endif
    //-- Log is always written to a temp file. If later the user decides they want
    //   it, it's all there for them.
    if (!_tempLogFile.isOpen()) {
        if (!_logSuspendReplay) {
            if (!_tempLogFile.open()) {
                emit protocolStatusMessage(tr("MAVLink Protocol"), tr("Opening Flight Data file for writing failed. "
                                                                      "Unable to write to %1. Please choose a different file location.").arg(_tempLogFile.fileName()));
                _closeLogFile();
                _logSuspendError = true;
                return;
            }

            qCDebug(MAVLinkProtocolLog) << "Temp log" << _tempLogFile.fileName();
            emit checkTelemetrySavePath();

            _logSuspendError = false;
        }
    }
}

void ShenHangProtocol::_stopLogging(void)
{
    if (_tempLogFile.isOpen()) {
        if (_closeLogFile()) {
            if ((_vehicleWasArmed || _app->toolbox()->settingsManager()->appSettings()->telemetrySaveNotArmed()->rawValue().toBool()) &&
                _app->toolbox()->settingsManager()->appSettings()->telemetrySave()->rawValue().toBool() &&
                !_app->toolbox()->settingsManager()->appSettings()->disableAllPersistence()->rawValue().toBool()) {
                emit saveTelemetryLog(_tempLogFile.fileName());
            } else {
                QFile::remove(_tempLogFile.fileName());
            }
        }
    }
    _vehicleWasArmed = false;
}

/// @brief Checks the temp directory for log files which may have been left there.
///         This could happen if QGC crashes without the temp log file being saved.
///         Give the user an option to save these orphaned files.
void ShenHangProtocol::checkForLostLogFiles(void)
{
    QDir tempDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation));

    QString filter(QString("*.%1").arg(_logFileExtension));
    QFileInfoList fileInfoList = tempDir.entryInfoList(QStringList(filter), QDir::Files);
    //qDebug() << "Orphaned log file count" << fileInfoList.count();

    for(const QFileInfo& fileInfo: fileInfoList) {
        //qDebug() << "Orphaned log file" << fileInfo.filePath();
        if (fileInfo.size() == 0) {
            // Delete all zero length files
            QFile::remove(fileInfo.filePath());
            continue;
        }
        emit saveTelemetryLog(fileInfo.filePath());
    }
}

void ShenHangProtocol::suspendLogForReplay(bool suspend)
{
    _logSuspendReplay = suspend;
}

void ShenHangProtocol::deleteTempLogFiles(void)
{
    QDir tempDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation));

    QString filter(QString("*.%1").arg(_logFileExtension));
    QFileInfoList fileInfoList = tempDir.entryInfoList(QStringList(filter), QDir::Files);

    for (const QFileInfo& fileInfo: fileInfoList) {
        QFile::remove(fileInfo.filePath());
    }
}

