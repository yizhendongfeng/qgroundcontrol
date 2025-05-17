#include "GCU.h"

#define HEADER0SEND 0XA8
#define HEADER1SEND 0XE5
#define HEADER0RSV  0X8A
#define HEADER1RSV  0X5E



GCU::GCU(VideoSettings * videoSettings) : _videoSettings(videoSettings)
{
#if 0
    //打包测试
    memset(bufferSend, 0, MAXBUFFERLENGTH);
    MessageSend msgSend;
    msgSend.length = 72;
    msgSend.version = 0x02;
    msgSend.roll = 0;
    msgSend.pitch = 100;
    msgSend.yaw = -100;
    msgSend.status = 0x05;
    msgSend.vehicleRoll = -1132;
    msgSend.vehiclePitch = 101;
    msgSend.vehicleYaw = 24000;
    msgSend.vehicleAccNorth = 112;
    msgSend.vehicleAccEast = -112;
    msgSend.vehicleAccUp = 112;
    msgSend.vehicleVelNorth = 2112;
    msgSend.vehicleVelEast = -2112;
    msgSend.vehicleVelUp = 2112;
    msgSend.subFrameRequest = 0x01;
    msgSend.subFrameHeader = 0x01;
    msgSend.vehicleLon = 1709175332;
    msgSend.vehicleLat = 380300822;
    msgSend.vehicleAlt = 41123;
    msgSend.satelliteNum = 19;
    msgSend.gnssMs = 352718000;
    msgSend.gnssWeek = 2278;
    msgSend.relativeAlt = 12120;
    msgSend.command = EMPTY;
    uint8_t commandParam[8];
    uint16_t paramLength = 0;


    uint16_t length = encode(msgSend, bufferSend, commandParam, paramLength);
    QByteArray byteArray(reinterpret_cast<const char*>(bufferSend), msgSend.length);
    qDebug() << "打包测试，长度" << length << "bufferSend: " << byteArray.toHex(' ');

    // 解包测试
    memset(bufferRsv, 0, MAXBUFFERLENGTH);
    uint8_t bufRsv0[MAXBUFFERLENGTH] {
        0x00, 0x8A, 0x5E, 0x49, 0x00, 0x02, 0x12, 0x01, 0x80, 0x0C, 0xFE,
        0xF4, 0x01, 0xDD, 0xFC, 0x20, 0x00, 0x4A, 0x18, 0xFF, 0xFF,
        0xA5, 0x03, 0x47, 0x18, 0xFF, 0xFF, 0x01, 0x00, 0xFE, 0xFF,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x1F, 0x32,
        0x29, 0x00, 0x00, 0x06, 0x17, 0x00, 0x00, 0x24, 0xF2, 0xDF,
        0x65, 0x16, 0xEE, 0xAA, 0x16, 0xA3, 0xA0, 0x00, 0x00, 0x2B,
        0x01, 0x14, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x20,
        // 0x00, 0xEC, 0x85
    };
    uint8_t bufRsv1[MAXBUFFERLENGTH] {
        0x00, 0xEC, 0x85
    };

    MessageRsv msgRsv = {};
    // decode(bufRsv0, 73, msgRsv);
    decode(bufRsv0, 71, msgRsv);
    decode(bufRsv1, 3, msgRsv);
    qDebug() << "msgRsv" << msgRsv.podCode;
#endif
    _port = _videoSettings->podPort()->rawValue().toInt();
    _ip   = _videoSettings->podIp()->rawValue().toString();
    // tcpSocket.connectToHost(_ip, _port);
    connect(&tcpSocket, &QTcpSocket::connected,    this, [&](){ _connected = true;
                                                                emit connectedChanged();
                                                                timerSendEmptyCommand.start(); });
    connect(&tcpSocket, &QTcpSocket::disconnected, this, [&](){ _connected = false;
                                                                emit connectedChanged();
                                                                timerSendEmptyCommand.stop();});
    connect(&tcpSocket, &QTcpSocket::bytesAvailable, this, &GCU::bytesReceivedFromTcp);

    connect(&timerSendEmptyCommand, &QTimer::timeout, this, &GCU::sendEmptyCommand);
    timerSendEmptyCommand.setInterval(200);
    timerSendEmptyCommand.start();// 5Hz频率发送空命令

    connect(&timerResendEmptyCommand, &QTimer::timeout, this, [&]() {timerSendEmptyCommand.start();});
    timerResendEmptyCommand.setSingleShot(true);
    timerResendEmptyCommand.setInterval(3000);
}

void GCU::connectToGcu(bool connect)
{
    qDebug() << "connectToGcu ip:" << _ip << "port: " << _port;

    if (_ip.isEmpty() || _port <= 0)
        return;
    if (connect) {
        tcpSocket.connectToHost(_ip, _port);
    } else {
        tcpSocket.disconnectFromHost();
    }
}

void GCU::startRecording(bool start)
{
    if (!_connected) {
        return;
    } else {
        qDebug() << "gcu tcp not connected";
    }
    timerSendEmptyCommand.stop();
    timerResendEmptyCommand.start();
    MessageSend msgSend = {};
    msgSend.length = 73;
    msgSend.version = 0x02;
    msgSend.command = RECORD;  // 录制与停止录制交替
    uint8_t commandParam[1] = {0x01};
    uint16_t paramLength = 1;
    uint16_t length = encode(msgSend, bufferSend, commandParam, paramLength);
    tcpSocket.write((char*)bufferSend, (qint64)length);
}

void GCU::takePhoto()
{
    if (!_connected) {
        return;
    } else {
        qDebug() << "gcu tcp not connected";
    }
    timerSendEmptyCommand.stop();
    timerResendEmptyCommand.start();
    MessageSend msgSend = {};
    msgSend.length = 73;
    msgSend.version = 0x02;
    msgSend.command = TAKEPHOTO;  //
    uint8_t commandParam[1] = {0x01};
    uint16_t paramLength = 1;
    uint16_t length = encode(msgSend, bufferSend, commandParam, paramLength);
    tcpSocket.write((char*)bufferSend, (qint64)length);
}

uint16_t GCU::encode(MessageSend msg, uint8_t *buf, uint8_t *commandParam, uint8_t commandLength)
{
    buf[0] = HEADER0SEND;
    buf[1] = HEADER1SEND;
    memcpy(buf + 2, &msg.length, 2);
    buf[4] = 0x02;
    memcpy(buf + 5, &msg.roll, 2);
    memcpy(buf + 7, &msg.pitch, 2);
    memcpy(buf + 9, &msg.yaw, 2);
    buf[11] = msg.status;
    memcpy(buf + 12, &msg.vehicleRoll, 2);
    memcpy(buf + 14, &msg.vehiclePitch, 2);
    memcpy(buf + 16, &msg.vehicleYaw, 2);
    memcpy(buf + 18, &msg.vehicleAccNorth, 2);
    memcpy(buf + 20, &msg.vehicleAccEast, 2);
    memcpy(buf + 22, &msg.vehicleAccUp, 2);
    memcpy(buf + 24, &msg.vehicleVelNorth, 2);
    memcpy(buf + 26, &msg.vehicleVelEast, 2);
    memcpy(buf + 28, &msg.vehicleVelUp, 2);
    buf[30] = msg.command == EMPTY ? 0x01 : msg.subFrameRequest;
    buf[37] = msg.command == EMPTY ? 0x00 : 0x01; // 副帧帧头
    memcpy(buf + 38, &msg.vehicleLon, 4);
    memcpy(buf + 42, &msg.vehicleLat, 4);
    memcpy(buf + 46, &msg.vehicleAlt, 4);
    buf[50] = msg.satelliteNum;
    memcpy(buf + 51, &msg.gnssMs, 4);
    memcpy(buf + 55, &msg.gnssWeek, 2);
    memcpy(buf + 57, &msg.relativeAlt, 4);
    buf[69] = msg.command;
    memcpy(buf + 70, commandParam, commandLength);
    uint16_t crc = CaculateCrc16(buf, msg.length - 2);
    buf[70 + commandLength] = (crc & 0xff00) >> 8;
    buf[71 + commandLength] = crc & 0xff;
    _lastCommandSent = msg.command;
    return msg.length;
}

bool GCU::decode(uint8_t *buf, uint16_t length, MessageRsv &msg)
{
    uint16_t packageLength = 0;
    bool     findHeader = false;
    bool     success    = false;
    memcpy(bufferRsv + bytesCountInBuffer, buf, length);
    bytesCountInBuffer += length;
    uint16_t i = 0;
    if (bytesCountInBuffer < 72) { // 不足一包（最低72字节）
        return false;
    }
    while (i < bytesCountInBuffer) {
        if (!findHeader) {
            if (bufferRsv[i] == HEADER0RSV) { // 查找帧头
                if (i + 1 < bytesCountInBuffer) {
                    if(bufferRsv[i + 1] == HEADER1RSV) {
                        findHeader = true;
                        continue;
                    }
                } else {
                    break;
                }
            }
            i++;
        } else {
            if (i + 3 < bytesCountInBuffer) {
                memcpy(&packageLength, bufferRsv + i + 2, 2);
                if (i + packageLength - 1 < bytesCountInBuffer) {
                    uint16_t crc = CaculateCrc16(bufferRsv + i, packageLength - 2);
                    uint16_t crcHigh = bufferRsv[i + packageLength - 2];
                    uint16_t crcInPackage = (crcHigh << 8) | bufferRsv[i + packageLength - 1];
                    if(crc == crcInPackage) {
                        msg.version = bufferRsv[i + 4];
                        msg.mode = bufferRsv[i + 5];
                        msg.status = (bufferRsv[i + 6] << 8) | bufferRsv[i + 7];
                        memcpy(&msg.missHorizentol, bufferRsv + i + 8, 2);
                        memcpy(&msg.missVerticle, bufferRsv + i + 10, 2);
                        memcpy(&msg.offsetAngleX, bufferRsv + i + 12, 2);
                        memcpy(&msg.offsetAngleY, bufferRsv + i + 14, 2);
                        memcpy(&msg.offsetAngleZ, bufferRsv + i + 16, 2);
                        memcpy(&msg.absRoll, bufferRsv + i + 18, 2);
                        memcpy(&msg.absPitch, bufferRsv + i + 20, 2);
                        memcpy(&msg.absYaw, bufferRsv + i + 22, 2);
                        memcpy(&msg.absXRate, bufferRsv + i + 24, 2);
                        memcpy(&msg.absYRate, bufferRsv + i + 26, 2);
                        memcpy(&msg.absZRate, bufferRsv + i + 28, 2);
                        msg.subFrameHeader = bufferRsv[i + 37];
                        msg.hardwareVersion = bufferRsv[i + 38];
                        msg.firmwareVersion = bufferRsv[i + 39];
                        msg.podCode = bufferRsv[i + 40];
                        memcpy(&msg.errorCode, bufferRsv + i + 41, 2);
                        memcpy(&msg.targetDist, bufferRsv + i + 43, 4);
                        memcpy(&msg.targetLon, bufferRsv + i + 47, 4);
                        memcpy(&msg.targetLat, bufferRsv + i + 51, 4);
                        memcpy(&msg.targetAlt, bufferRsv + i + 55, 4);
                        memcpy(&msg.camera1Zoom, bufferRsv + i + 59, 2);
                        memcpy(&msg.camera2Zoom, bufferRsv + i + 61, 2);
                        msg.thermalStatus = bufferRsv[i + 63];
                        memcpy(&msg.cameraStatus, bufferRsv + i + 64, 2);
                        msg.timeZone = bufferRsv[i + 66];
                        msg.command = bufferRsv[i + 69];
                        if (msg.command != 0x00) {// 非空命令
                            msg.commandStatus = bufferRsv[70];
                        }
                        i += packageLength;
                        findHeader = false;
                        success  = true;
                        // 收到的应答为上次命令的应答且不为空命令，则需要发送一条空命令。否则多次重发相同的命令会被忽略
                        if (msg.command == _lastCommandSent && _lastCommandSent != EMPTY) {
                            timerSendEmptyCommand.start();
                        }
                        if (abs(msg.camera1Zoom * 0.1 - _zoom) < 0.001f ) {
                            _zoom = msg.camera1Zoom * 0.1;
                            emit zoomChanged();
                        }

                        continue;
                    } else { //crc校验不通过继续找帧头
                        i += 2;
                        findHeader = false;
                        success = false;
                        continue;
                    }
                } else { // 数据不够一帧
                    break;
                }
            }
        }
    }
    if (i < bytesCountInBuffer) {
        memcpy(bufferRsv, bufferRsv + i, bytesCountInBuffer - i);
    }
    bytesCountInBuffer -= i;
    return success;
}

uint16_t GCU::CaculateCrc16(uint8_t *ptr, uint8_t len)
{
    uint16_t crc;
    uint8_t da;
    uint16_t crc_ta[16]={
        0x0000,0x1021,0x2042,0x3063,0x4084,0x50a5,0x60c6,0x70e7,
        0x8108,0x9129,0xa14a,0xb16b,0xc18c,0xd1ad,0xe1ce,0xf1ef,
    };
    crc=0;
    while(len--!=0) {
        da = crc>>12;
        crc <<= 4;
        crc ^= crc_ta[da^(*ptr>>4)];
        da = crc >> 12;
        crc <<= 4;
        crc ^= crc_ta[da ^ (*ptr&0x0F)];
        ptr++;
    }
    return(crc);
}

void GCU::setVisualLight(const bool visualLight)
{
    if (!_connected) {
        return;
    } else {
        qDebug() << "gcu tcp not connected";
    }
    timerSendEmptyCommand.stop();
    timerResendEmptyCommand.start();
    _visualLight = visualLight;
    MessageSend msgSend = {};
    msgSend.length = 74;
    msgSend.version = 0x02;
    msgSend.command = NIGHTVISION;
    uint8_t commandParam[2] = {0x01, static_cast<uint8_t>(visualLight ? 0x00 : 0x01)};
    uint16_t paramLength = 2;
    uint16_t length = encode(msgSend, bufferSend, commandParam, paramLength);
    tcpSocket.write((char*)bufferSend, (qint64)length);
}

void GCU::adjustPitch(const double pitch)
{
    if (!_connected) {
        return;
    } else {
        qDebug() << "gcu tcp not connected";
    }
    timerSendEmptyCommand.stop();
    timerResendEmptyCommand.start();
    MessageSend msgSend = {};
    msgSend.length = 72;
    msgSend.version = 0x02;
    msgSend.command = YAWFOLLOW; // 指向跟随，无参数
    msgSend.pitch   = pitch * _zoom * 10;

    msgSend.status = 0x04; //控制量有效
    qDebug() << "adjustPitch:" << pitch;
    uint8_t *commandParam = nullptr;
    uint16_t length = encode(msgSend, bufferSend, commandParam, 0);
    tcpSocket.write((char*)bufferSend, (qint64)length);;
}

void GCU::adjustYaw(const double yaw)
{
    if (!_connected) {
        return;
    } else {
        qDebug() << "gcu tcp not connected";
    }
    timerSendEmptyCommand.stop();
    timerResendEmptyCommand.start();
    MessageSend msgSend = {};
    msgSend.length = 72;
    msgSend.version = 0x02;
    msgSend.command = YAWFOLLOW; // 指向跟随，无参数
    msgSend.yaw   = yaw * 10 * _zoom * 10; // 10度/秒

    msgSend.status = 0x04; //控制量有效
    qDebug() << "adjustYaw:" << yaw;
    uint8_t *commandParam = nullptr;
    uint16_t length = encode(msgSend, bufferSend, commandParam, 0);
    tcpSocket.write((char*)bufferSend, (qint64)length);;
}

void GCU::setDirection(CameraDirection direction, const double pitch)
{
    if (!_connected) {
        return;
    } else {
        qDebug() << "gcu tcp not connected";
    }
    timerSendEmptyCommand.stop();
    timerResendEmptyCommand.start();
    MessageSend msgSend = {};
    msgSend.length = 72;
    msgSend.version = 0x02;
    msgSend.command = ANGLECONTROL; // 角度控制
    msgSend.status  = 0x04;         // 控制角有效
    msgSend.roll = 0;
    msgSend.pitch = pitch * 100;
    switch (direction) {
    case LOOKFORWARD:
        msgSend.yaw = 0;
        break;
    case LOOKBACKWARD:
        msgSend.yaw = 180 * 100;
        break;
    case LOOKLEFT:
        msgSend.yaw = 90 * 100;
        break;
    case LOOKRIGHT:
        msgSend.yaw = -90 * 100;
        break;
    default:
        break;
    }
    msgSend.status = 0x04; //控制量有效
    uint8_t *commandParam = nullptr;

    uint16_t length = encode(msgSend, bufferSend, commandParam, 0);
    tcpSocket.write((char*)bufferSend, (qint64)length);;
}

void GCU::setZoom(const double zoom)
{
    if (!_connected) {
        return;
    } else {
        qDebug() << "gcu tcp not connected";
    }
    timerSendEmptyCommand.stop();
    timerResendEmptyCommand.start();
    MessageSend msgSend = {};
    msgSend.length = 72;
    msgSend.version = 0x02;
    if (zoom > 0) {
        msgSend.command = ZOOMIN; // 放大，无参数
    } else if (zoom < 0) {
        msgSend.command = ZOOMOUT; // 缩小
    } else if (zoom == 0) {
        msgSend.command = ZOOMSTOP; // 停止缩放
    }
    qDebug() << "setZoom:" << zoom;
    uint8_t *commandParam = nullptr;

    uint16_t length = encode(msgSend, bufferSend, commandParam, 0);
    tcpSocket.write((char*)bufferSend, (qint64)length);;
}

void GCU::setIp(const QString ip)
{
    _ip = ip;
    emit ipChanged();
}

void GCU::setPort(const int port)
{
    _port = port;
}

void GCU::sendEmptyCommand()
{
    MessageSend msgSend = {};
    msgSend.length = 72;
    msgSend.version = 2;
    msgSend.command = EMPTY; // 空命令
    uint8_t *commandParam = nullptr;
    uint16_t length = encode(msgSend, bufferSend, commandParam, 0);
    if (_connected) {
        tcpSocket.write(reinterpret_cast<char*>(bufferSend), static_cast<qint64>(length));
        // qDebug() << "empty command:" << QByteArray((char*)bufferSend, length).split(' ');
    }
}

void GCU::_mavlinkMessageReceived(const mavlink_message_t &message)
{

}

void GCU::bytesReceivedFromTcp()
{
    QByteArray bytes = tcpSocket.readAll();
    MessageRsv msgRsv;
    bool decoded = decode(reinterpret_cast<uint8_t*>(bytes.data()), bytes.length(), msgRsv);
    if (!decoded)
        return;
    _pitch = msgRsv.offsetAngleX * 0.01f;
    _yaw   = msgRsv.offsetAngleZ * 0.01f;
    _zoom  = msgRsv.camera1Zoom;
    emit pitchChanged();
    emit yawChanged();
    emit zoomChanged();
    if (_cameraName != _podCodeNames[msgRsv.podCode]) {
        _cameraName = _podCodeNames[msgRsv.podCode];
        emit cameraNameChanged();
    }

}
