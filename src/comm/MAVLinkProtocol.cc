/****************************************************************************
 *
 *   (c) 2009-2016 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/


/**
 * @file
 *   @brief Implementation of class MAVLinkProtocol
 *   @author Lorenz Meier <mail@qgroundcontrol.org>
 */

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

#include "MAVLinkProtocol.h"
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

Q_DECLARE_METATYPE(mavlink_message_t)

QGC_LOGGING_CATEGORY(MAVLinkProtocolLog, "MAVLinkProtocolLog")

const char* MAVLinkProtocol::_tempLogFileTemplate = "FlightDataXXXXXX"; ///< Template for temporary log file
const char* MAVLinkProtocol::_logFileExtension = "mavlink";             ///< Extension for log files

/**
 * The default constructor will create a new MAVLink object sending heartbeats at
 * the MAVLINK_HEARTBEAT_DEFAULT_RATE to all connected links.
 */
MAVLinkProtocol::MAVLinkProtocol(QGCApplication* app, QGCToolbox* toolbox)
    : QGCTool(app, toolbox)
    , m_enable_version_check(true)
    , _message({})
    , _status({})
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
    memset(&_status,            0, sizeof(_status));
    memset(&_message,           0, sizeof(_message));
}

MAVLinkProtocol::~MAVLinkProtocol()
{
    storeSettings();
    _closeLogFile();
}

void MAVLinkProtocol::setVersion(unsigned version)
{
    QList<LinkInterface*> links = _linkMgr->links();

    for (int i = 0; i < links.length(); i++) {
        mavlink_status_t* mavlinkStatus = mavlink_get_channel_status(links[i]->mavlinkChannel());

        // Set flags for version
        if (version < 200) {
            mavlinkStatus->flags |= MAVLINK_STATUS_FLAG_OUT_MAVLINK1;
        } else {
            mavlinkStatus->flags &= ~MAVLINK_STATUS_FLAG_OUT_MAVLINK1;
        }
    }

    _current_version = version;
}

void MAVLinkProtocol::setToolbox(QGCToolbox *toolbox)
{
   QGCTool::setToolbox(toolbox);

   _linkMgr =               _toolbox->linkManager();
   _multiVehicleManager =   _toolbox->multiVehicleManager();

   qRegisterMetaType<mavlink_message_t>("mavlink_message_t");

   loadSettings();

   // All the *Counter variables are not initialized here, as they should be initialized
   // on a per-link basis before those links are used. @see resetMetadataForLink().

   connect(this, &MAVLinkProtocol::protocolStatusMessage,   _app, &QGCApplication::criticalMessageBoxOnMainThread);
   connect(this, &MAVLinkProtocol::saveTelemetryLog,        _app, &QGCApplication::saveTelemetryLogOnMainThread);
   connect(this, &MAVLinkProtocol::checkTelemetrySavePath,  _app, &QGCApplication::checkTelemetrySavePathOnMainThread);

   connect(_multiVehicleManager, &MultiVehicleManager::vehicleAdded, this, &MAVLinkProtocol::_vehicleCountChanged);
   connect(_multiVehicleManager, &MultiVehicleManager::vehicleRemoved, this, &MAVLinkProtocol::_vehicleCountChanged);

   emit versionCheckChanged(m_enable_version_check);
}

void MAVLinkProtocol::loadSettings()
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

void MAVLinkProtocol::storeSettings()
{
    // Store settings
    QSettings settings;
    settings.beginGroup("QGC_MAVLINK_PROTOCOL");
    settings.setValue("VERSION_CHECK_ENABLED", m_enable_version_check);
    settings.setValue("GCS_SYSTEM_ID", systemId);
    // Parameter interface settings
}

void MAVLinkProtocol::resetMetadataForLink(LinkInterface *link)
{
    int channel = link->mavlinkChannel();
    totalReceiveCounter[channel] = 0;
    totalLossCounter[channel]    = 0;
    runningLossPercent[channel]  = 0.0f;
    for(int i = 0; i < 256; i++) {
        firstMessage[channel][i] =  1;
    }
    link->setDecodedFirstMavlinkPacket(false);
}

/**
 * This method parses all incoming bytes and constructs a MAVLink packet.
 * It can handle multiple links in parallel, as each link has it's own buffer/
 * parsing state machine.
 * @param link The interface to read from
 * @see LinkInterface
 **/

void MAVLinkProtocol::receiveBytes(LinkInterface* link, QByteArray b)
{
    // Since receiveBytes signals cross threads we can end up with signals in the queue
    // that come through after the link is disconnected. For these we just drop the data
    // since the link is closed.
    if (!_linkMgr->containsLink(link)) {
        return;
    }
    uint8_t mavlinkChannel = link->mavlinkChannel();

    static int  nonmavlinkCount = 0;
    static bool checkedUserNonMavlink = false;
    static bool warnedUserNonMavlink  = false;

#if 0
    /******************************     增加无人船通信解析程序    --张纪敏 *********************************/
    //思路：把接收到的数据通过分别通过无人船通信解析与无人机通信解析，无人船通信解析成功后打包成无人机mavlink消息发送出去
    //需要的mavlink消息包括：心跳包、位置包、姿态包
//    if(b.size() > (BinaryBufferLength - usvBuffer.length))
//        usvBuffer.length = 0;
    size_t len = b.size() > (BinaryBufferLength - usvBuffer.length) ? (BinaryBufferLength - usvBuffer.length) : b.size();
    memcpy(usvBuffer.data + usvBuffer.length, b.data(), len);
    usvBuffer.length += len;
    qDebug() << "b.size:" << b.size() << "BinaryBufferLength:" << BinaryBufferLength  << "usvBuffer.length:" << usvBuffer.length << (BinaryBufferLength - usvBuffer.length) << link->getLinkConfiguration()->name();
    BinaryPacket binaryPacket = Protocol::BinaryBufferDecode(&usvBuffer);
    static int usvMsgCount = 1;
    if(binaryPacket.id == 19)//
    {
        mavlink_message_t usvMavlinkMsg;
        usvMavlinkMsg.sysid = 11;
        usvMavlinkMsg.compid = 1;
//        mavlink_status_t usvStatus;
        //一秒钟发送一次心跳包
        if((usvMsgCount++)%5 == 0)
        {
            mavlink_heartbeat_t usvHeartbeat;
            usvHeartbeat.autopilot = MAV_AUTOPILOT_PX4;
            usvHeartbeat.type = MAV_TYPE_GROUND_ROVER;
            usvHeartbeat.base_mode |= MAV_MODE_FLAG_DECODE_POSITION_MANUAL;
            usvHeartbeat.base_mode |= MAV_MODE_FLAG_DECODE_POSITION_SAFETY;
            usvHeartbeat.mavlink_version = 2;
            usvMsgCount = 1;
            emit vehicleHeartbeatInfo(link, usvMavlinkMsg.sysid, usvMavlinkMsg.compid, usvHeartbeat.autopilot, usvHeartbeat.type);
        }
        //发送位置、姿态
        mavlink_global_position_int_t usvGlobalPosInt = {};
        double latitude, longtitude, altitude, timeStamp;
        float vx, vy, roll, pitch, yaw;
        memcpy(&timeStamp, binaryPacket.data, 8);
        memcpy(&latitude, binaryPacket.data + 9, 8);
        memcpy(&longtitude, binaryPacket.data + 17, 8);
        memcpy(&altitude, binaryPacket.data + 25, 8);
        memcpy(&vx, binaryPacket.data + 45, 4);
        memcpy(&vy, binaryPacket.data + 49, 4);
        usvGlobalPosInt.time_boot_ms = static_cast<uint32_t>(timeStamp * 1e3);
        usvGlobalPosInt.lat = static_cast<int>(latitude * 180 / M_PI * 1e7) + usvMsgCount * 1000000;
        usvGlobalPosInt.lon = static_cast<int>(longtitude * 180 / M_PI * 1e7);
        usvGlobalPosInt.alt = static_cast<int>(altitude * 1000);
        usvGlobalPosInt.relative_alt = usvGlobalPosInt.alt;
        usvGlobalPosInt.vx = static_cast<short>(vx * 100);
        usvGlobalPosInt.vy = static_cast<short>(vy * 100);

        usvMavlinkMsg.msgid = MAVLINK_MSG_ID_GLOBAL_POSITION_INT;
        mavlink_msg_global_position_int_encode(usvMavlinkMsg.sysid, usvMavlinkMsg.compid, &usvMavlinkMsg,
                                               static_cast<mavlink_global_position_int_t*>(&usvGlobalPosInt));
        emit messageReceived(link, usvMavlinkMsg);

        mavlink_gps_raw_int_t gps_raw_int = {};
        usvMavlinkMsg.msgid = MAVLINK_MSG_ID_GPS_RAW_INT;
        gps_raw_int.fix_type = GPS_FIX_TYPE_3D_FIX;
        gps_raw_int.lat = usvGlobalPosInt.lat;
        gps_raw_int.lon = usvGlobalPosInt.lon;
        gps_raw_int.satellites_visible = 13;
        gps_raw_int.eph = 30;
        gps_raw_int.epv = 50;
        gps_raw_int.hdg_acc = 120;
        gps_raw_int.cog = 18000;
        mavlink_msg_gps_raw_int_encode(usvMavlinkMsg.sysid, usvMavlinkMsg.compid, &usvMavlinkMsg,
                                       static_cast<mavlink_gps_raw_int_t*>(&gps_raw_int));
        emit messageReceived(link, usvMavlinkMsg);

        mavlink_attitude_t usvAttitude = {};
        memcpy(&roll, binaryPacket.data + 33, 4);
        memcpy(&pitch, binaryPacket.data + 37, 4);
        memcpy(&yaw, binaryPacket.data + 41, 4);
        usvAttitude.time_boot_ms = static_cast<uint32_t>(timeStamp * 1e3);
        usvAttitude.roll = roll;
        usvAttitude.pitch = pitch;
        usvAttitude.yaw = yaw;
        usvMavlinkMsg.msgid = MAVLINK_MSG_ID_ATTITUDE;
        mavlink_msg_attitude_encode(usvMavlinkMsg.sysid, usvMavlinkMsg.compid, &usvMavlinkMsg,
                                    static_cast<const mavlink_attitude_t*>(&usvAttitude));
        emit messageReceived(link, usvMavlinkMsg);
        qDebug() << "usv msgid:" << binaryPacket.id << "latitude:" << latitude;
        delete[] binaryPacket.data;
//        return;
    }
    /**********************************   代码修改结束   --张纪敏  ****************************************/
#endif
    for (int position = 0; position < b.size(); position++) {
        if (mavlink_parse_char(mavlinkChannel, static_cast<uint8_t>(b[position]), &_message, &_status)) {
            // Got a valid message
            if (!link->decodedFirstMavlinkPacket()) {
                link->setDecodedFirstMavlinkPacket(true);
                mavlink_status_t* mavlinkStatus = mavlink_get_channel_status(mavlinkChannel);
                if (!(mavlinkStatus->flags & MAVLINK_STATUS_FLAG_IN_MAVLINK1) && (mavlinkStatus->flags & MAVLINK_STATUS_FLAG_OUT_MAVLINK1)) {
                    qDebug() << "Switching outbound to mavlink 2.0 due to incoming mavlink 2.0 packet:" << mavlinkStatus << mavlinkChannel << mavlinkStatus->flags;
                    mavlinkStatus->flags &= ~MAVLINK_STATUS_FLAG_OUT_MAVLINK1;
                    // Set all links to v2
                    setVersion(200);
                }
            }

            //-----------------------------------------------------------------
            // MAVLink Status
            uint8_t lastSeq = lastIndex[_message.sysid][_message.compid];
            uint8_t expectedSeq = lastSeq + 1;
            // Increase receive counter
            totalReceiveCounter[mavlinkChannel]++;
            // Determine what the next expected sequence number is, accounting for
            // never having seen a message for this system/component pair.
            if(firstMessage[_message.sysid][_message.compid]) {
                firstMessage[_message.sysid][_message.compid] = 0;
                lastSeq     = _message.seq;
                expectedSeq = _message.seq;
            }
            // And if we didn't encounter that sequence number, record the error
            //int foo = 0;
            if (_message.seq != expectedSeq)
            {
                //foo = 1;
                int lostMessages = 0;
                //-- Account for overflow during packet loss
                if(_message.seq < expectedSeq) {
                    lostMessages = (_message.seq + 255) - expectedSeq;
                } else {
                    lostMessages = _message.seq - expectedSeq;
                }
                // Log how many were lost
                totalLossCounter[mavlinkChannel] += static_cast<uint64_t>(lostMessages);
            }

            // And update the last sequence number for this system/component pair
            lastIndex[_message.sysid][_message.compid] = _message.seq;;
            // Calculate new loss ratio
            uint64_t totalSent = totalReceiveCounter[mavlinkChannel] + totalLossCounter[mavlinkChannel];
            float receiveLossPercent = static_cast<float>(static_cast<double>(totalLossCounter[mavlinkChannel]) / static_cast<double>(totalSent));
            receiveLossPercent *= 100.0f;
            receiveLossPercent = (receiveLossPercent * 0.5f) + (runningLossPercent[mavlinkChannel] * 0.5f);
            runningLossPercent[mavlinkChannel] = receiveLossPercent;

            //qDebug() << foo << _message.seq << expectedSeq << lastSeq << totalLossCounter[mavlinkChannel] << totalReceiveCounter[mavlinkChannel] << totalSentCounter[mavlinkChannel] << "(" << _message.sysid << _message.compid << ")";

            //-----------------------------------------------------------------
            // Log data
            if (!_logSuspendError && !_logSuspendReplay && _tempLogFile.isOpen()) {
                uint8_t buf[MAVLINK_MAX_PACKET_LEN+sizeof(quint64)];

                // Write the uint64 time in microseconds in big endian format before the message.
                // This timestamp is saved in UTC time. We are only saving in ms precision because
                // getting more than this isn't possible with Qt without a ton of extra code.
                quint64 time = static_cast<quint64>(QDateTime::currentMSecsSinceEpoch() * 1000);
                qToBigEndian(time, buf);

                // Then write the message to the buffer
                int len = mavlink_msg_to_send_buffer(buf + sizeof(quint64), &_message);

                // Determine how many bytes were written by adding the timestamp size to the message size
                len += sizeof(quint64);

                // Now write this timestamp/message pair to the log.
                QByteArray b(reinterpret_cast<const char*>(buf), len);
                if(_tempLogFile.write(b) != len)
                {
                    // If there's an error logging data, raise an alert and stop logging.
                    emit protocolStatusMessage(tr("MAVLink Protocol"), tr("MAVLink Logging failed. Could not write to file %1, logging disabled.").arg(_tempLogFile.fileName()));
                    _stopLogging();
                    _logSuspendError = true;
                }

                // Check for the vehicle arming going by. This is used to trigger log save.
                if (!_vehicleWasArmed && _message.msgid == MAVLINK_MSG_ID_HEARTBEAT) {
                    mavlink_heartbeat_t state;
                    mavlink_msg_heartbeat_decode(&_message, &state);
                    if (state.base_mode & MAV_MODE_FLAG_DECODE_POSITION_SAFETY) {
                        _vehicleWasArmed = true;
                    }
                }
            }

            if (_message.msgid == MAVLINK_MSG_ID_HEARTBEAT) {
                _startLogging();
                mavlink_heartbeat_t heartbeat;
                mavlink_msg_heartbeat_decode(&_message, &heartbeat);
                emit vehicleHeartbeatInfo(link, _message.sysid, _message.compid, heartbeat.autopilot, heartbeat.type);
            }

            if (_message.msgid == MAVLINK_MSG_ID_HIGH_LATENCY2) {
                _startLogging();
                mavlink_high_latency2_t highLatency2;
                mavlink_msg_high_latency2_decode(&_message, &highLatency2);
                emit vehicleHeartbeatInfo(link, _message.sysid, _message.compid, highLatency2.autopilot, highLatency2.type);
            }
            ///收到位置信息后广播至其他飞机  zjm
            if(_message.msgid == MAVLINK_MSG_ID_GLOBAL_POSITION_INT)
            {
                uint8_t formationBuffer[MAVLINK_MAX_PACKET_LEN];
                QList<SharedLinkInterfacePointer> sharedLinks = _linkMgr->getSharedLinks();
                int len = mavlink_msg_to_send_buffer(formationBuffer, &_message);
                for(int i = 0; i < sharedLinks.count(); i++)
                {
//                    if(sharedLinks.at(i).data() != link /*&&  sharedLinks.at(i).data()->isConnected()*/)//->active())
                    {
                        sharedLinks.at(i)->writeBytesSafe((const char*)formationBuffer, len);
                        qDebug() << "*** broadcast formation data***" << "id:" << _message.sysid << "seq:" << _message.seq << i << sharedLinks.count();
                    }
                }
            }///
            // Detect if we are talking to an old radio not supporting v2
            mavlink_status_t* mavlinkStatus = mavlink_get_channel_status(mavlinkChannel);
            if (_message.msgid == MAVLINK_MSG_ID_RADIO_STATUS && _radio_version_mismatch_count != -1) {
                if ((mavlinkStatus->flags & MAVLINK_STATUS_FLAG_IN_MAVLINK1)
                && !(mavlinkStatus->flags & MAVLINK_STATUS_FLAG_OUT_MAVLINK1)) {
                    _radio_version_mismatch_count++;
                }
            }

            if (_radio_version_mismatch_count == 5) {
                // Warn the user if the radio continues to send v1 while the link uses v2
                emit protocolStatusMessage(tr("MAVLink Protocol"), tr("Detected radio still using MAVLink v1.0 on a link with MAVLink v2.0 enabled. Please upgrade the radio firmware."));
                // Set to flag warning already shown
                _radio_version_mismatch_count = -1;
                // Flick link back to v1
                qDebug() << "Switching outbound to mavlink 1.0 due to incoming mavlink 1.0 packet:" << mavlinkStatus << mavlinkChannel << mavlinkStatus->flags;
                mavlinkStatus->flags |= MAVLINK_STATUS_FLAG_OUT_MAVLINK1;
            }

            // Update MAVLink status on every 32th packet
            if ((totalReceiveCounter[mavlinkChannel] & 0x1F) == 0) {
                emit mavlinkMessageStatus(_message.sysid, totalSent, totalReceiveCounter[mavlinkChannel], totalLossCounter[mavlinkChannel], receiveLossPercent);
            }

            // The packet is emitted as a whole, as it is only 255 - 261 bytes short
            // kind of inefficient, but no issue for a groundstation pc.
            // It buys as reentrancy for the whole code over all threads
            emit messageReceived(link, _message);
            // Reset message parsing
            memset(&_status,  0, sizeof(_status));
            memset(&_message, 0, sizeof(_message));
//            return;
        }
        else if (!link->decodedFirstMavlinkPacket()) {
            // No formed message yet
            nonmavlinkCount++;
            if (nonmavlinkCount > 1000 && !warnedUserNonMavlink) {
                // 1000 bytes with no mavlink message. Are we connected to a mavlink capable device?
                if (!checkedUserNonMavlink) {
                    link->requestReset();
                    checkedUserNonMavlink = true;
                } else {
                    warnedUserNonMavlink = true;
                    // Disconnect the link since it's some other device and
                    // QGC clinging on to it and feeding it data might have unintended
                    // side effects (e.g. if its a modem)
                    qDebug() << "disconnected link" << link->getName() << "as it contained no MAVLink data";
                    QMetaObject::invokeMethod(_linkMgr, "disconnectLink", Q_ARG( LinkInterface*, link ) );
                    return;
                }
            }
        }
    }

}

void MAVLinkProtocol::receiveUSVBytes(LinkInterface *link, QByteArray b)
{
#if 1
    /******************************     增加无人船通信解析程序    --张纪敏 *********************************/
    //思路：把接收到的数据通过分别通过无人船通信解析与无人机通信解析，无人船通信解析成功后打包成无人机mavlink消息发送出去
    //需要的mavlink消息包括：心跳包、位置包、姿态包
//    if(b.size() > (BinaryBufferLength - usvBuffer.length))
//        usvBuffer.length = 0;
    size_t len = b.size() > (BinaryBufferLength - usvBuffer.length) ? (BinaryBufferLength - usvBuffer.length) : b.size();
    memcpy(usvBuffer.data + usvBuffer.length, b.data(), len);
    usvBuffer.length += len;
//    qDebug() << "b.size:" << b.size() << "BinaryBufferLength:" << BinaryBufferLength  << "usvBuffer.length:" << usvBuffer.length
//             << (BinaryBufferLength - usvBuffer.length) << link->getLinkConfiguration()->name()
//             << link->getLinkConfiguration()->name().contains("USV");
    BinaryPacket binaryPacket = Protocol::BinaryBufferDecode(&usvBuffer);
    static int usvMsgCount = 1;
    if(binaryPacket.id == 19)//
    {
        mavlink_message_t usvMavlinkMsg;
        usvMavlinkMsg.sysid = 11;
        usvMavlinkMsg.compid = 1;
//        mavlink_status_t usvStatus;
        mavlink_heartbeat_t usvHeartbeat;
        usvHeartbeat.autopilot = MAV_AUTOPILOT_PX4;
        usvHeartbeat.type = MAV_TYPE_GROUND_ROVER;
        usvHeartbeat.base_mode |= MAV_MODE_FLAG_DECODE_POSITION_MANUAL;
        usvHeartbeat.base_mode |= MAV_MODE_FLAG_DECODE_POSITION_SAFETY;
        usvHeartbeat.mavlink_version = 2;
        //一秒钟发送一次心跳包
        if((usvMsgCount++)%5 == 0)
        {

            usvMsgCount = 1;
            emit vehicleHeartbeatInfo(link, usvMavlinkMsg.sysid, usvMavlinkMsg.compid, usvHeartbeat.autopilot, usvHeartbeat.type);
        }
        //发送位置、姿态
        mavlink_global_position_int_t usvGlobalPosInt = {};
        double latitude, longtitude, altitude, timeStamp;
        float vx, vy, roll, pitch, yaw;
        memcpy(&timeStamp, binaryPacket.data, 8);
        memcpy(&latitude, binaryPacket.data + 9, 8);
        memcpy(&longtitude, binaryPacket.data + 17, 8);
        memcpy(&altitude, binaryPacket.data + 25, 8);
        memcpy(&vx, binaryPacket.data + 45, 4);
        memcpy(&vy, binaryPacket.data + 49, 4);
        usvGlobalPosInt.time_boot_ms = static_cast<uint32_t>(timeStamp * 1e3);
        usvGlobalPosInt.lat = static_cast<int>(latitude * 180 / M_PI * 1e7);// + usvMsgCount * 1000000
        usvGlobalPosInt.lon = static_cast<int>(longtitude * 180 / M_PI * 1e7);
        usvGlobalPosInt.alt = static_cast<int>(altitude * 1000);
        usvGlobalPosInt.relative_alt = usvGlobalPosInt.alt;
        usvGlobalPosInt.vx = static_cast<short>(vx * 100);
        usvGlobalPosInt.vy = static_cast<short>(vy * 100);

        usvMavlinkMsg.msgid = MAVLINK_MSG_ID_GLOBAL_POSITION_INT;
        mavlink_msg_global_position_int_encode(usvMavlinkMsg.sysid, usvMavlinkMsg.compid, &usvMavlinkMsg,
                                               static_cast<mavlink_global_position_int_t*>(&usvGlobalPosInt));
        emit messageReceived(link, usvMavlinkMsg);

        mavlink_gps_raw_int_t gps_raw_int = {};
        usvMavlinkMsg.msgid = MAVLINK_MSG_ID_GPS_RAW_INT;
        gps_raw_int.fix_type = GPS_FIX_TYPE_3D_FIX;
        gps_raw_int.lat = usvGlobalPosInt.lat;
        gps_raw_int.lon = usvGlobalPosInt.lon;
        gps_raw_int.satellites_visible = 13;
        gps_raw_int.eph = 30;
        gps_raw_int.epv = 50;
        gps_raw_int.hdg_acc = 120;
        gps_raw_int.cog = 18000;
        mavlink_msg_gps_raw_int_encode(usvMavlinkMsg.sysid, usvMavlinkMsg.compid, &usvMavlinkMsg,
                                       static_cast<mavlink_gps_raw_int_t*>(&gps_raw_int));
        emit messageReceived(link, usvMavlinkMsg);

        mavlink_attitude_t usvAttitude = {};
        memcpy(&roll, binaryPacket.data + 33, 4);
        memcpy(&pitch, binaryPacket.data + 37, 4);
        memcpy(&yaw, binaryPacket.data + 41, 4);
        usvAttitude.time_boot_ms = static_cast<uint32_t>(timeStamp * 1e3);
        usvAttitude.roll = roll;
        usvAttitude.pitch = pitch;
        usvAttitude.yaw = yaw;
        usvMavlinkMsg.msgid = MAVLINK_MSG_ID_ATTITUDE;
        mavlink_msg_attitude_encode(usvMavlinkMsg.sysid, usvMavlinkMsg.compid, &usvMavlinkMsg,
                                    static_cast<const mavlink_attitude_t*>(&usvAttitude));
        emit messageReceived(link, usvMavlinkMsg);
        qDebug() << "usv msgid:" << binaryPacket.id << "latitude:" << latitude;
        if(binaryPacket.data != nullptr)
            delete[] binaryPacket.data;
//        return;
    }
    /**********************************   代码修改结束   --张纪敏  ****************************************/
#endif
}

/**
 * @return The name of this protocol
 **/
QString MAVLinkProtocol::getName()
{
    return QString(tr("MAVLink protocol"));
}

/** @return System id of this application */
int MAVLinkProtocol::getSystemId()
{
    return systemId;
}

void MAVLinkProtocol::setSystemId(int id)
{
    systemId = id;
}

/** @return Component id of this application */
int MAVLinkProtocol::getComponentId()
{
    return 0;
}

void MAVLinkProtocol::enableVersionCheck(bool enabled)
{
    m_enable_version_check = enabled;
    emit versionCheckChanged(enabled);
}

void MAVLinkProtocol::_vehicleCountChanged(void)
{
    int count = _multiVehicleManager->vehicles()->count();
    if (count == 0) {
        // Last vehicle is gone, close out logging
        _stopLogging();
        _radio_version_mismatch_count = 0;
    }
}

/// @brief Closes the log file if it is open
bool MAVLinkProtocol::_closeLogFile(void)
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

void MAVLinkProtocol::_startLogging(void)
{
    //-- Are we supposed to write logs?
    if (qgcApp()->runningUnitTests()) {
        return;
    }
#ifdef __mobile__
    //-- Mobile build don't write to /tmp unless told to do so
    if (!_app->toolbox()->settingsManager()->appSettings()->telemetrySave()->rawValue().toBool()) {
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

            qDebug() << "Temp log" << _tempLogFile.fileName();
            emit checkTelemetrySavePath();

            _logSuspendError = false;
        }
    }
}

void MAVLinkProtocol::_stopLogging(void)
{
    if (_tempLogFile.isOpen()) {
        if (_closeLogFile()) {
            if ((_vehicleWasArmed || _app->toolbox()->settingsManager()->appSettings()->telemetrySaveNotArmed()->rawValue().toBool()) &&
                _app->toolbox()->settingsManager()->appSettings()->telemetrySave()->rawValue().toBool()) {
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
void MAVLinkProtocol::checkForLostLogFiles(void)
{
    QDir tempDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation));

    QString filter(QString("*.%1").arg(_logFileExtension));
    QFileInfoList fileInfoList = tempDir.entryInfoList(QStringList(filter), QDir::Files);
    //qDebug() << "Orphaned log file count" << fileInfoList.count();

    for(const QFileInfo fileInfo: fileInfoList) {
        //qDebug() << "Orphaned log file" << fileInfo.filePath();
        if (fileInfo.size() == 0) {
            // Delete all zero length files
            QFile::remove(fileInfo.filePath());
            continue;
        }
        emit saveTelemetryLog(fileInfo.filePath());
    }
}

void MAVLinkProtocol::suspendLogForReplay(bool suspend)
{
    _logSuspendReplay = suspend;
}

void MAVLinkProtocol::deleteTempLogFiles(void)
{
    QDir tempDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation));

    QString filter(QString("*.%1").arg(_logFileExtension));
    QFileInfoList fileInfoList = tempDir.entryInfoList(QStringList(filter), QDir::Files);

    for(const QFileInfo fileInfo: fileInfoList) {
        QFile::remove(fileInfo.filePath());
    }
}

