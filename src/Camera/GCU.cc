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
    _autoConnect = _videoSettings->autoConnectToPod()->rawValue().toBool();
    connect(&tcpSocket, &QTcpSocket::connected,    this, [&](){ _connected = true;
                                                                emit connectedChanged();
                                                                timerSendEmptyCommand.start();
                                                                timerConnectToGcu->stop();
    });
    connect(&tcpSocket, &QTcpSocket::disconnected, this, [&](){ _connected = false;
                                                                emit connectedChanged();
                                                                timerSendEmptyCommand.stop();
                                                                timerConnectToGcu->start();
    });
    connect(&tcpSocket, &QTcpSocket::readyRead, this, &GCU::bytesReceivedFromTcp);


    connect(&timerSendEmptyCommand, &QTimer::timeout, this, &GCU::sendEmptyCommand);
    timerSendEmptyCommand.setInterval(30);
    // timerSendEmptyCommand.start();// 10Hz频率发送空命令

    connect(&timerResendEmptyCommand, &QTimer::timeout, this, [&]() {timerSendEmptyCommand.start();qDebug() << "after 1s to start timer";});
    timerResendEmptyCommand.setSingleShot(true);
    timerResendEmptyCommand.setInterval(1000);

    timerConnectToGcu = new QTimer(this);
    timerConnectToGcu->setInterval(1000);
    timerConnectToGcu->setSingleShot(true);
    connect(timerConnectToGcu, &QTimer::timeout, this, [&](){
        if (_autoConnect) {
            connectToGcu(true);
        }
    });
    connect(&tcpSocket, &QTcpSocket::errorOccurred, this, [&](QAbstractSocket::SocketError socketError){
        qDebug() << "connectToGcu error: " << socketError;
        timerConnectToGcu->start();
    });
    timerConnectToGcu->start();
}

void GCU::connectToGcu(bool connect)
{
    _port = _videoSettings->podPort()->rawValue().toInt();
    _ip   = _videoSettings->podIp()->rawValue().toString();
    qDebug() << "connectToGcu ip:" << _ip << "port: " << _port << "connect:" << connect;
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
        qDebug() << "startRecording gcu tcp not connected";
        return;
    }

    GcuMessageSend msgSend = {};
    msgSend.length = 73;
    msgSend.version = 0x02;
    msgSend.command = RECORD;  // 录制与停止录制交替
    uint8_t commandParam[1] = {0x01};
    uint16_t paramLength = 1;
    sendMsg(msgSend, commandParam, paramLength);
}

void GCU::takePhoto()
{
    if (!_connected) {
        qDebug() << "takePhoto gcu tcp not connected";
        return;
    }
    qDebug() << "takePhoto()";
    GcuMessageSend msgSend = {};
    msgSend.length = 73;
    msgSend.version = 0x02;
    msgSend.command = TAKEPHOTO;  //
    uint8_t commandParam[1] = {0x01};
    uint16_t paramLength = 1;
    sendMsg(msgSend, commandParam, paramLength);
}

int GCU::sendMsg(GcuMessageSend msg, uint8_t *commandParam, uint8_t commandLength)
{
    bufferSend[0] = HEADER0SEND;
    bufferSend[1] = HEADER1SEND;
    memcpy(bufferSend + 2, &msg.length, 2);
    bufferSend[4] = 0x02;
    memcpy(bufferSend + 5, &msg.roll, 2);
    memcpy(bufferSend + 7, &msg.pitch, 2);
    memcpy(bufferSend + 9, &msg.yaw, 2);
    bufferSend[11] = msg.status | (_imuValid ? 1 : 0);
    memcpy(bufferSend + 12, &msgFromVehicle.vehicleRoll, 2);
    memcpy(bufferSend + 14, &msgFromVehicle.vehiclePitch, 2);
    memcpy(bufferSend + 16, &msgFromVehicle.vehicleYaw, 2);
    memcpy(bufferSend + 18, &msgFromVehicle.vehicleAccNorth, 2);
    memcpy(bufferSend + 20, &msgFromVehicle.vehicleAccEast, 2);
    memcpy(bufferSend + 22, &msgFromVehicle.vehicleAccUp, 2);
    memcpy(bufferSend + 24, &msgFromVehicle.vehicleVelNorth, 2);
    memcpy(bufferSend + 26, &msgFromVehicle.vehicleVelEast, 2);
    memcpy(bufferSend + 28, &msgFromVehicle.vehicleVelUp, 2);
    bufferSend[30] = 0x01;  //msg.command == EMPTY ? 0x01 : msg.subFrameRequest;// 填写需要返回的副帧帧头
    bufferSend[37] = 0x01;  //msg.command == EMPTY ? 0x00 : 0x01; // 副帧帧头
    memcpy(bufferSend + 38, &msgFromVehicle.vehicleLon, 4);
    memcpy(bufferSend + 42, &msgFromVehicle.vehicleLat, 4);
    memcpy(bufferSend + 46, &msgFromVehicle.vehicleAlt, 4);
    bufferSend[50] = msgFromVehicle.satelliteNum;
    memcpy(bufferSend + 51, &msgFromVehicle.gnssMs, 4);
    memcpy(bufferSend + 55, &msgFromVehicle.gnssWeek, 2);
    memcpy(bufferSend + 57, &msgFromVehicle.relativeAlt, 4);
    bufferSend[69] = msg.command;
    memcpy(bufferSend + 70, commandParam, commandLength);
    uint16_t crc = CaculateCrc16(bufferSend, msg.length - 2);
    bufferSend[70 + commandLength] = (crc & 0xff00) >> 8;
    bufferSend[71 + commandLength] = crc & 0xff;
    if (msg.command != EMPTY) { // 非空命令则暂停空命令的发送，优先发送命令，接收到命令的执行状态后再启动定时器
        timerSendEmptyCommand.stop();
        timerResendEmptyCommand.start();
    }
    //保存命令信息
    _lastCommandSent = msg.command;
    bytesCountInSendBuffer = msg.length;
    // 发送
    if (tcpSocket.isValid() && _connected) {
        int length = tcpSocket.write((char*)bufferSend, bytesCountInSendBuffer);
        qDebug() << "send command:" << msg.command << "send buffer length:" << length << ",bytes:" << QByteArray((char*)bufferSend, length).toHex();
        return length;
    }
    else {
        timerSendEmptyCommand.stop();
        timerResendEmptyCommand.stop();
        qDebug() << "吊舱tcpsocket不可用，或者未连接";
        return -1;
    }
}

bool GCU::decode(uint8_t *buf, uint16_t length, MessageRsv &msg)
{
    uint16_t packageLength = 0;
    bool     findHeader = false;
    bool     success    = false;
    memcpy(bufferRsv + bytesCountInRsvBuffer, buf, length);
    bytesCountInRsvBuffer += length;
    uint16_t i = 0;
    if (bytesCountInRsvBuffer < 72) { // 不足一包（最低72字节）
        return false;
    }
    while (i < bytesCountInRsvBuffer) {
        if (!findHeader) {
            if (bufferRsv[i] == HEADER0RSV) { // 查找帧头
                if (i + 1 < bytesCountInRsvBuffer) {
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
            if (i + 3 < bytesCountInRsvBuffer) {
                memcpy(&packageLength, bufferRsv + i + 2, 2);
                if (i + packageLength - 1 < bytesCountInRsvBuffer) {
                    uint16_t crc = CaculateCrc16(bufferRsv + i, packageLength - 2);
                    uint16_t crcHigh = bufferRsv[i + packageLength - 2];
                    uint16_t crcInPackage = (crcHigh << 8) | bufferRsv[i + packageLength - 1];
                    if(crc == crcInPackage) {
                        msg.version = bufferRsv[i + 4];
                        msg.mode = bufferRsv[i + 5];
                        memcpy(&msg.status, bufferRsv + i + 6, 2);
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
                            msg.commandStatus = bufferRsv[i + 70];
                        }
                        i += packageLength;
                        findHeader = false;
                        success  = true;
                        // 收到的应答为上次命令的应答且不为空命令，则需要发送一条空命令。否则多次重发相同的命令会被忽略
                        if (msg.command == _lastCommandSent && _lastCommandSent != EMPTY) {
                            timerSendEmptyCommand.start();
                        }
                        // if (abs(msg.camera1Zoom * 0.1 - _zoom) > 0.001f ) {
                        //     _zoom = msg.camera1Zoom * 0.1;
                        //     emit zoomChanged();
                        // }

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
    if (i < bytesCountInRsvBuffer) {
        memcpy(bufferRsv, bufferRsv + i, bytesCountInRsvBuffer - i);
    }
    bytesCountInRsvBuffer -= i;
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
        qDebug() << "setVisualLight gcu tcp not connected";
        return;
    }

    _visualLight = visualLight;
    GcuMessageSend msgSend = {};
    msgSend.length = 74;
    msgSend.version = 0x02;
    msgSend.command = NIGHTVISION;
    uint8_t commandParam[2] = {0x01, static_cast<uint8_t>(visualLight ? 0x00 : 0x01)};
    uint16_t paramLength = 2;
    sendMsg(msgSend, commandParam, paramLength);
}

void GCU::setOsd(const bool open)
{
    GcuMessageSend msgSend = {};
    msgSend.length = 73;
    msgSend.version = 0x02;
    msgSend.command = OSD;
    uint8_t commandParam[1] = {static_cast<uint8_t>(open ? 0x01 : 0x00)};
    uint16_t paramLength = 1;
    sendMsg(msgSend, commandParam, paramLength);
}

void GCU::setPodHeadMode(const int mode)
{
    qDebug() << "setPodHeadMode()" << QString::number(mode, 16);
    if (mode == TRACK) {
        podTrack(true, 4500, 4500, 5500, 5500);
        return;
    }
    GcuMessageSend msgSend = {};
    msgSend.length = 72;
    msgSend.version = 0x02;
    // _podMode = static_cast<CameraCommand>(mode);
    msgSend.command = static_cast<CameraCommand>(mode);
    uint8_t *commandParam = nullptr;
    uint16_t paramLength = 0;

    sendMsg(msgSend, commandParam, paramLength);

}

void GCU::rotatePod(const int axis, const float rate)
{
    switch (axis) {
    case 0:
        _yawRateTarget = rate * 10;// * 0.1 / qMax(_zoom, 1.0);
        break;
    case 1:
        _pitchRateTarget = rate * 10;// * 0.1 / qMax(_zoom, 1.0);
        break;
    default:
        break;
    }
}

void GCU::setPodAngle(const int axis, const float angle)
{
    qDebug() << "setPodAngle:" << axis << angle;

    switch (axis) {
    case 0:
        _yawAngleTarget = angle * 100;
        break;
    case 1:
        _pitchAngleTarget = angle * 100;
        break;
    case 2:
        _rollAngleTarget = angle * 100;
        break;
    default:
        break;
    }
}

void GCU::setZoom(const float value)
{
    int zoomValue = value * -10;
    GcuMessageSend msgSend = {};
    msgSend.length = 75;
    msgSend.version = 2;
    msgSend.command = SETZOOM; // 空命令
    uint8_t commandParam[3];
    commandParam[0] = 1;
    memcpy(commandParam + 1, &zoomValue, 2);
    sendMsg(msgSend, commandParam, 3);
}

void GCU::moveToPoint(const int x, const int y)
{
    if (_podMode == TRACK) {
        podTrack(false, 0, 0, 0, 0);
    }
    GcuMessageSend msgSend = {};
    msgSend.length = 77;
    msgSend.version = 0x02;
    msgSend.command = POINTANDMOVE;
    msgSend.status = 0;
    uint8_t commandParam[5];
    commandParam[0] = 0x01;
    uint16_t pointX = x;
    uint16_t pointY = y;
    memcpy(commandParam + 1, &pointX, 2);
    memcpy(commandParam + 3, &pointY, 2);
    uint16_t paramLength = 5;
    sendMsg(msgSend, commandParam, paramLength);
}

void GCU::calibratePod()
{
    GcuMessageSend msgSend = {};
    msgSend.length = 72;
    msgSend.version = 0x02;
    msgSend.command = CALIBRATE;
    msgSend.status = 0;
    uint8_t* commandParam = nullptr;
    uint16_t paramLength = 0;
    sendMsg(msgSend, commandParam, paramLength);
}

void GCU::turnOnFillLight(int brightness)
{
    GcuMessageSend msgSend = {};
    msgSend.length = 73;
    msgSend.version = 0x02;
    msgSend.command = FILLINLIGHT;
    msgSend.status = 0;
    uint8_t commandParam[1];
    commandParam[0] = brightness;
    uint16_t paramLength = 1;
    sendMsg(msgSend, commandParam, paramLength);
}

void GCU::podCentering()
{
    GcuMessageSend msgSend = {};
    msgSend.length = 72;
    msgSend.version = 0x02;
    msgSend.command = CENTER;
    msgSend.status = 0;
    uint8_t *commandParam = nullptr;
    uint16_t paramLength = 0;
    sendMsg(msgSend, commandParam, paramLength);
}

void GCU::podTrack(bool start, const int startX, const int startY, const int endX, const int endY)
{
    qDebug() << "podTrack()" << "start:" << start << "x0:" << startX << "y0:" << startY << "x1:" << endX << "y1:" << endY;
    GcuMessageSend msgSend = {};
    const uint16_t paramLength = 10;
    msgSend.length = 72 + paramLength;
    msgSend.version = 0x02;
    msgSend.command = TRACK;
    msgSend.status = 0;
    uint8_t commandParam[paramLength];
    commandParam[0] = 0x01; // 主相机
    commandParam[1] = start ? 0x01 : 0x00;
    memcpy(commandParam + 2, &startX, 2);
    memcpy(commandParam + 4, &startY, 2);
    memcpy(commandParam + 6, &endX, 2);
    memcpy(commandParam + 8, &endY, 2);
    sendMsg(msgSend, commandParam, paramLength);
}

void GCU::adjustPitch(const double pitch)
{
    if (!_connected) {
        qDebug() << "adjustPitch gcu tcp not connected";
        return;
    }
    GcuMessageSend msgSend = {};
    msgSend.length = 72;
    msgSend.version = 0x02;
    msgSend.command = YAWFOLLOW; // 指向跟随，无参数
    msgSend.pitch   = pitch * _zoom * 10;

    msgSend.status = 0x04; //控制量有效
    qDebug() << "adjustPitch:" << pitch;
    uint8_t *commandParam = nullptr;
    sendMsg(msgSend, commandParam, 0);
}

void GCU::adjustYaw(const double yaw)
{
    if (!_connected) {
        qDebug() << "adjustYaw gcu tcp not connected";
        return;
    }

    GcuMessageSend msgSend = {};
    msgSend.length = 72;
    msgSend.version = 0x02;
    msgSend.command = YAWFOLLOW; // 指向跟随，无参数
    msgSend.yaw   = yaw * 10 * _zoom * 10; // 10度/秒

    msgSend.status = 0x04; //控制量有效
    qDebug() << "adjustYaw:" << yaw;
    uint8_t *commandParam = nullptr;
    sendMsg(msgSend, commandParam, 0);
}

void GCU::setDirection(CameraDirection direction, const double pitch)
{
    if (!_connected) {
        qDebug() << "setDirection gcu tcp not connected";
        return;
    }

    GcuMessageSend msgSend = {};
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
        msgSend.yaw = 90 * 100 * (_upRight ? 1 : -1);
        break;
    case LOOKRIGHT:
        msgSend.yaw = -90 * 100 * (_upRight ? 1 : -1);
        break;
    default:
        break;
    }
    msgSend.status = 0x04; //控制量有效
    uint8_t *commandParam = nullptr;
    sendMsg(msgSend, commandParam, 0);
}

void GCU::setCameraCommand(CameraCommand command)
{

}

void GCU::continuousZoom(const double zoom)
{
    if (!_connected) {
        qDebug() << "setZoom gcu tcp not connected";
        return;
    }

    GcuMessageSend msgSend = {};
    msgSend.length = 73;
    msgSend.version = 0x02;
    if (zoom > 0) {
        msgSend.command = ZOOMIN; // 放大，无参数
    } else if (zoom < 0) {
        msgSend.command = ZOOMOUT; // 缩小
    } else if (zoom == 0) {
        msgSend.command = ZOOMSTOP; // 停止缩放
    }
    qDebug() << "setZoom:" << zoom << ", command:" << msgSend.command;
    uint8_t commandParam[1] = {0x01}; // 相机编号，默认1为主相机，可能有红外相机
    sendMsg(msgSend, commandParam, 1);
}

void GCU::setIp(const QString ip)
{
    if (_ip != ip) {
        _ip = ip;
        emit ipChanged();
    }
}

void GCU::setPort(const int port)
{
    if (_port != port) {
        _port = port;
        emit portChanged();
    }
}

void GCU::setPodMode(const int mode)
{
    // if (_podMode != (CameraCommand)mode) {
        _podMode = (CameraCommand)mode;
        emit podModeChanged();
    // }
}

void GCU::setFillLight(const bool turnOn)
{
    if (_fillLight != turnOn) {
        _fillLight = turnOn;
        emit fillLightChanged();
    }
}

void GCU::setNightVision(const bool nightVision)
{
    if (_nightVision != nightVision) {
        _nightVision = nightVision;
        emit nightVisionChanged();
    }
}

void GCU::sendEmptyCommand()
{
    GcuMessageSend msgSend = {};
    msgSend.length = 72;
    msgSend.version = 2;
    msgSend.command = EMPTY; // 空命令
    if (_podMode == YAWFOLLOW || _podMode == YAWLOCK) {
        msgSend.yaw    = _yawRateTarget;
        msgSend.pitch  = _pitchRateTarget * (_upRight ? 1 : -1);
        msgSend.status = 1 << 2 | (_imuValid ? 0 : 1); // TODO: 此处要添加惯导数据是否有效的设置，Bit0，Bit2为控制量是否有效，无效时角速度为0
        qDebug() << "###### _yawRateTarget: " << _yawRateTarget << ",_pitchRateTarget:" << _pitchRateTarget;
    } else if (_podMode == ANGLECONTROL || _podMode == EULERCONTROL ||  _podMode == FPV) {
        msgSend.yaw   = _yawAngleTarget;
        msgSend.pitch = _pitchAngleTarget;
        msgSend.roll  = _rollAngleTarget;
        msgSend.status = 1 << 2 | (_imuValid ? 0 : 1);
        qDebug() << "###### _yawAngleTarget: " << _yawAngleTarget << ",_pitchAngleTarget:" << _pitchAngleTarget << ",_rollAngleTarget:" << _rollAngleTarget;
    }
    uint8_t *commandParam = nullptr;
    sendMsg(msgSend, commandParam, 0);
}

void GCU::receiveVehicleMessage(const GcuMessageSend &msg)
{
    msgFromVehicle = msg;
    if (msg.vehicleRoll != 0)
        _imuValid = true;
    // qDebug() << "receiveVehicleMessage" << msg.roll << msg.yaw;
}

// void GCU::_mavlinkMessageReceived(const mavlink_message_t &message)
// {

// }

void GCU::bytesReceivedFromTcp()
{
    QByteArray bytes = tcpSocket.readAll();
    MessageRsv msgRsv;
    bool decoded = decode(reinterpret_cast<uint8_t*>(bytes.data()), bytes.length(), msgRsv);
    if (!decoded)
        return;
    // qDebug() << "bytes:" << bytes.toHex() << "command:" << QString::number(msgRsv.command, 16) << "_lastCommandSent:" << QString::number(_lastCommandSent, 16);
    if (msgRsv.command != _lastCommandSent) { // 接收的不是发送的命令，需重新发送，连续多条重复命令只会执行一次
        int length = tcpSocket.write((char*)bufferSend, bytesCountInSendBuffer);
        // qDebug() << "*****command miss, resend length:"<<length << ",bytes:" << QByteArray((char*)bufferSend, bytesCountInSendBuffer).toHex();
        _needResendLastCommand = true;
    } else if(msgRsv.command != EMPTY && msgRsv.commandStatus != 0){    // 接收的是发送的命令，但执行失败，需重新发送，连续多条重复命令只会执行一次
        // tcpSocket.write((char*)bufferSend, bytesCountInSendBuffer);
        qDebug() << "command status wrong, command:" << QString::number(msgRsv.command, 16) << ",msgRsv.commandStatus:" << msgRsv.commandStatus << ",_podMode:" << QString::number(_podMode, 16);
        _needResendLastCommand = true;
        emit podModeChanged(); // 命令执行失败，刷新下界面，返回之前的模式
    } else if (msgRsv.command != EMPTY && msgRsv.commandStatus == 0){ // 命令状态为成功，重发空命令
        _needResendLastCommand = false;
        timerSendEmptyCommand.start();
    }

    if (msgRsv.command == CALIBRATE) {
        emit calibrateStateChanged(msgRsv.commandStatus);
    }
    // if (_podMode != static_cast<CameraCommand>(msgRsv.mode)) {
    //     _podMode = static_cast<CameraCommand>(msgRsv.mode);
    //     emit podModeChanged();
    // }
    setPodMode(msgRsv.mode);
    _podStatus = msgRsv.status;
    _upRight = (_podStatus >> 12) & 0x01;
    setFillLight((_podStatus >> 10) & 0x01);
    setNightVision((_podStatus >> 9) & 0x01);
#if 0
    float pitch = msgRsv.absPitch * 0.01f;
    if (_pitch != pitch) {
        _pitch = pitch;
        emit pitchChanged();
    }
    float yaw   = msgRsv.absYaw * 0.01f;
    if (_yaw != yaw) {
        _yaw = yaw;
        emit yawChanged();
    }
    float zoom  = msgRsv.camera1Zoom * 0.1f;
    if (_zoom != zoom) {
        _zoom = zoom;
        emit zoomChanged();
    }
#endif
    _roll = msgRsv.absRoll * 0.01f;
    emit rollChanged();
    _pitch = msgRsv.absPitch * 0.01f;
    emit pitchChanged();
    _yaw   = msgRsv.absYaw * 0.01f;
    while (_yaw > 180) {
        _yaw -= 360;
    }
    emit yawChanged();
    _zoom  = msgRsv.camera1Zoom * 0.1f;
    emit zoomChanged();
    qDebug() << "pitch:" << _pitch << ",offsetAngleY:" << msgRsv.offsetAngleY * 0.01 << ", yaw:" << _yaw << ",offsetAngleZ:" << msgRsv.offsetAngleZ * 0.01 << ", zoom:" << _zoom << ", _podMode:" << QString::number(_podMode, 16) << ", podCode:" << msgRsv.podCode << _podCodeNames[msgRsv.podCode] << "podStatus:" << QString::number(_podStatus, 2) << "nightVision:" << _nightVision << "fillLight:" << _fillLight;
    if (_cameraName != _podCodeNames[msgRsv.podCode]) {
        _cameraName = _podCodeNames[msgRsv.podCode];
        emit cameraNameChanged();
    }
    // if (msgRsv.command == EMPTY && _lastCommandSentNonEmpty != EMPTY) {
    //     _lastCommandSentNonEmpty = EMPTY;
    //     tcpSocket.write(reinterpret_cast<char*>(bufferSend), bytesCountInBufferLast);
    //     qDebug() << "rensend last command:" << _lastCommandSentNonEmpty;
    // }
}
