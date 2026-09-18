#include "InyyoA102Pro.h"

InyyoA102Pro::InyyoA102Pro(VideoSettings * videoSettings)
    : _videoSettings(videoSettings)
{
#if 0
    // uint8_t unpackData[29] = {0xEE,0x01,0x1D,0xB8,0x0B,0x32,0x00,0x3E,0xFE,0xE8,0x03,0x05,0x01,0x87,0x2D,
    //                           0xB9,0x16,0x87,0xDB,0x05,0x44,0x7B,0x00,0x00,0x00,0x00,0x00,0x01,0xD8};
    // UnpackData(unpackData, 29);

    StopRoate();
    PitchRotate(true,  0xff);
    PitchRotate(false, 0xff);
    YawRotate(true, 0xff);
    YawRotate(false, 0xff);
    ZoomChange(false, 0x04);
    ZoomChange(true, 0x04);
    ZoomStop();
    MoveToPoint(0x40,0x40);
    SetPitchAngle(10);
    SetYawAngle(100);
    StartTrackToPoint(0x40,0x40);
    StopTrackToPoint();
    TargetDetect(true);
    TargetDetect(false);
    AuxiliaryTracking(true);
    AuxiliaryTracking(false);
    TakePhoto();
    Recording(true);
    Recording(false);
    FollowPlatform(true);
    FollowPlatform(false);
    LookDown();
    LookForward();
    PictureInPictureSwitch(0x02);
    ThermalModeSwitch(0x01);
    ThermalZoom(0x01);
    Stabilization(true);
    Stabilization(false);
    Osd(true);
    Osd(false);
    ManualFocus(true);
    ManualFocus(false);
    ChangeFocus(true);
    ChangeFocus(false);
    StopFocus();
    RequestSbusChannel();
    RequestPodTime();
    RequestPodVersion();
    SetReturnFrequency(0x02);
    ResetStreamIp();
    RequestStreamIp();
    PodPower(0x00);
    PodPower(0x01);
    PodPower(0x02);
    PodPower(0x03);
#endif
    _port = _videoSettings->podPort()->rawValue().toInt();
    _ip   = _videoSettings->podIp()->rawValue().toString();
    _autoConnect = _videoSettings->autoConnectToPod()->rawValue().toBool();

    _timerConnectToPod = new QTimer(this);
    _timerConnectToPod->setInterval(1000);
    _timerConnectToPod->setSingleShot(true);
    connect(_timerConnectToPod, &QTimer::timeout, this, [&](){
        if (_autoConnect) {
            connectToPod(true);
        }
    });

    connect(&_tcpSocket, &QTcpSocket::connected,    this, [&](){ _connected = true;
        emit connectedChanged();
        _timerConnectToPod->stop();
    });
    connect(&_tcpSocket, &QTcpSocket::disconnected, this, [&](){ _connected = false;
        emit connectedChanged();
        _timerConnectToPod->start();
    });
    connect(&_tcpSocket, &QTcpSocket::readyRead, this, &InyyoA102Pro::bytesReceivedFromTcp);
    _timerConnectToPod->start();
}

void InyyoA102Pro::connectToPod(bool connect)
{
    _port = _videoSettings->podPort()->rawValue().toInt();
    _ip   = _videoSettings->podIp()->rawValue().toString();
    qDebug() << "connectToPod ip:" << _ip << "port: " << _port << "connect:" << connect;
    if (_ip.isEmpty() || _port <= 0)
        return;
    if (connect) {
        _tcpSocket.connectToHost(_ip, _port);
    } else {
        _tcpSocket.disconnectFromHost();
    }
}

void InyyoA102Pro::stopRoate()
{
    uint8_t data[7];
    int length = 0;
    packData(0x00, 0x00, 0x00, 0x00, data, length);
    qDebug() << "StopMove: " << QByteArray((const char*)data, length).toHex(' ');
    sendToPod(data, length);
}

void InyyoA102Pro::pitchRotate(bool up, uint8_t speed)
{
    uint8_t data[7];
    int length = 0;
    uint8_t command2 = up ? 0x08 : 0x10;
    packData(0x00, command2, 0x00, speed, data, length);
    sendToPod(data, length);
    qDebug() << "PitchUp: " << QByteArray((const char*)data, length).toHex(' ');
}


void InyyoA102Pro::yawRotate(bool left, uint8_t speed)
{
    uint8_t data[7];
    int length = 0;
    uint8_t command2 = left ? 0x04 : 0x02;
    packData(0x00, command2, speed, 0x00, data, length);
    sendToPod(data, length);
    qDebug() << "YawLeft: " << QByteArray((const char*)data, length).toHex(' ');
}

void InyyoA102Pro::zoomChange(bool zoomIn, uint8_t speed)
{
    uint8_t data[7];
    int length = 0;
    uint8_t command2 = zoomIn ? 0x40 : 0x20;
    packData(0x00, command2, speed, 0x00, data, length);
    sendToPod(data, length);
    qDebug() << "ZoomOut: " << QByteArray((const char*)data, length).toHex(' ');
}

void InyyoA102Pro::zoomStop()
{
    uint8_t data[7];
    int length = 0;
    packData(0x00, 0x60, 0x00, 0x00, data, length);
    sendToPod(data, length);
    qDebug() << "ZoomStop: " << QByteArray((const char*)data, length).toHex(' ');
}

void InyyoA102Pro::moveToPoint(uint8_t x, uint8_t y)
{
    uint8_t data[7];
    int length = 0;
    packData(0x10, 0x00, x, y, data, length);
    sendToPod(data, length);
    qDebug() << "ZoomStop: " << QByteArray((const char*)data, length).toHex(' ');
}

void InyyoA102Pro::setPitchAngle(float angle)
{
    uint8_t data[7];
    int length = 0;
    if (angle > 30) {
        angle = 30;
    } else if (angle < -120) {
        angle = -120;
    }
    int16_t angleTemp = angle * 50;
    packData(0x10, 0x01, (angleTemp >> 8) & 0xff, angleTemp & 0xff, data, length);
    sendToPod(data, length);
    qDebug() << "SetPitchAngle: " << QByteArray((const char*)data, length).toHex(' ');
}

void InyyoA102Pro::setYawAngle(float angle)
{
    uint8_t data[7];
    int length = 0;
    int16_t angleTemp = angle * 50;
    packData(0x10, 0x02, (angleTemp >> 8) & 0xff, angleTemp & 0xff, data, length);
    sendToPod(data, length);
    qDebug() << "SetYawAngle: " << QByteArray((const char*)data, length).toHex(' ');
}

void InyyoA102Pro::startTrackToPoint(uint8_t x, uint8_t y)
{
    uint8_t data[7];
    int length = 0;
    packData(0x11, 0x00, x, y, data, length);
    sendToPod(data, length);
    qDebug() << "StartTrackToPoint: " << QByteArray((const char*)data, length).toHex(' ');
}

void InyyoA102Pro::stopTrackToPoint()
{
    uint8_t data[7];
    int length = 0;
    packData(0x11, 0x01, 0x00, 0x00, data, length);
    sendToPod(data, length);
    qDebug() << "StopTrackToPoint: " << QByteArray((const char*)data, length).toHex(' ');
}

void InyyoA102Pro::targetDetect(bool on)
{
    uint8_t data[7];
    int length = 0;
    uint8_t command2 = on ? 0x02 : 0x03;
    packData(0x11, command2, 0x00, 0x00, data, length);
    sendToPod(data, length);
    qDebug() << "StartTargetDetect: " << QByteArray((const char*)data, length).toHex(' ');
}

void InyyoA102Pro::auxiliaryTracking(bool on)
{
    uint8_t data[7];
    int length = 0;
    uint8_t command2 = on ? 0x04 : 0x05;
    packData(0x11, command2, 0x00, 0x00, data, length);
    sendToPod(data, length);
    qDebug() << "StartAuxiliaryTracking: " << QByteArray((const char*)data, length).toHex(' ');
}

void InyyoA102Pro::takePhoto()
{
    uint8_t data[7];
    int length = 0;
    packData(0x12, 0x00, 0x00, 0x00, data, length);
    sendToPod(data, length);
    qDebug() << "TakePhoto: " << QByteArray((const char*)data, length).toHex(' ');
}

void InyyoA102Pro::recording(bool on)
{
    uint8_t data[7];
    int length = 0;
    uint8_t command2 = on ? 0x01 : 0x02;
    packData(0x12, command2, 0x00, 0x00, data, length);
    sendToPod(data, length);
    qDebug() << "StartRecording: " << QByteArray((const char*)data, length).toHex(' ');
}

void InyyoA102Pro::followPlatform(bool on)
{
    uint8_t data[7];
    int length = 0;
    uint8_t command2 = on ? 0x00 : 0x01;
    packData(0x13, command2, 0x00, 0x00, data, length);
    sendToPod(data, length);
    qDebug() << "StartFollowPlatform: " << QByteArray((const char*)data, length).toHex(' ');
}

void InyyoA102Pro::lookDown()
{
    uint8_t data[7];
    int length = 0;
    packData(0x13, 0x02, 0x00, 0x00, data, length);
    sendToPod(data, length);
    qDebug() << "LookDown: " << QByteArray((const char*)data, length).toHex(' ');
}

void InyyoA102Pro::lookForward()
{
    uint8_t data[7];
    int length = 0;
    packData(0x13, 0x03, 0x00, 0x00, data, length);
    sendToPod(data, length);
    qDebug() << "LookForward: " << QByteArray((const char*)data, length).toHex(' ');
}

void InyyoA102Pro::pictureInPictureSwitch(uint8_t mode)
{
    //数据码1为画面模式，0x00:可见光嵌入热成像，0x01:热成像嵌入可见光，0x02:可见光，0x03:热成像
    uint8_t data[7];
    int length = 0;
    packData(0x14, 0x00, mode, 0x00, data, length);
    sendToPod(data, length);
    qDebug() << "PictureInPictureSwitch: " << QByteArray((const char*)data, length).toHex(' ');
}

void InyyoA102Pro::thermalModeSwitch(uint8_t mode)
{
    //数据码1为黑热、白热、彩色切换模式，0x00:白热，0x01:黑热，0x02:彩色
    uint8_t data[7];
    int length = 0;
    packData(0x15, 0x00, mode, 0x00, data, length);
    sendToPod(data, length);
    qDebug() << "ThermalModeSwitch: " << QByteArray((const char*)data, length).toHex(' ');
}

void InyyoA102Pro::thermalZoom(uint8_t zoom)
{
    //数据码1为电子变倍倍数(1-8倍)，0x00:1倍，0x01:2倍，0x02:3倍，0x03:4倍
    uint8_t data[7];
    int length = 0;
    packData(0x17, 0x00, zoom, 0x00, data, length);
    sendToPod(data, length);
    qDebug() << "ThermalZoom: " << QByteArray((const char*)data, length).toHex(' ');
}

void InyyoA102Pro::stabilization(bool on)
{
    uint8_t data[7];
    int length = 0;
    uint8_t command2 = on ? 0x00 : 0x01;
    packData(0x18, command2, 0x00, 0x00, data, length);
    sendToPod(data, length);
    qDebug() << "Stabilization: " << QByteArray((const char*)data, length).toHex(' ');
}

void InyyoA102Pro::osd(bool on)
{
    uint8_t data[7];
    int length = 0;
    uint8_t command2 = on ? 0x00 : 0x01;
    packData(0x19, command2, 0x00, 0x00, data, length);
    sendToPod(data, length);
    qDebug() << "Osd: " << QByteArray((const char*)data, length).toHex(' ');
}

void InyyoA102Pro::manualFocus(bool on)
{
    uint8_t data[7];
    int length = 0;
    uint8_t command2 = on ? 0x00 : 0x01;
    packData(0x1d, command2, 0x00, 0x00, data, length);
    sendToPod(data, length);
    qDebug() << "ManualFocus: " << QByteArray((const char*)data, length).toHex(' ');
}

void InyyoA102Pro::changeFocus(bool near)
{
    uint8_t data[7];
    int length = 0;
    uint8_t command2 = near ? 0x02 : 0x03;
    packData(0x1d, command2, 0x00, 0x00, data, length);
    sendToPod(data, length);
    qDebug() << "ChangeFocus: " << QByteArray((const char*)data, length).toHex(' ');
}

void InyyoA102Pro::stopFocus()
{
    uint8_t data[7];
    int length = 0;
    packData(0x1d, 0x04, 0x00, 0x00, data, length);
    sendToPod(data, length);
    qDebug() << "StopFocus: " << QByteArray((const char*)data, length).toHex(' ');
}

void InyyoA102Pro::requestSbusChannel()
{
    uint8_t data[7];
    int length = 0;
    packData(0x1e, 0x00, 0x00, 0x00, data, length);
    sendToPod(data, length);
    qDebug() << "RequestSbusChannel: " << QByteArray((const char*)data, length).toHex(' ');
}

void InyyoA102Pro::requestPodTime()
{
    uint8_t data[7];
    int length = 0;
    packData(0x1e, 0x01, 0x00, 0x00, data, length);
    sendToPod(data, length);
    qDebug() << "RequestPodTime: " << QByteArray((const char*)data, length).toHex(' ');
}

void InyyoA102Pro::requestPodVersion()
{
    uint8_t data[7];
    int length = 0;
    packData(0x1e, 0x02, 0x00, 0x00, data, length);
    sendToPod(data, length);
    qDebug() << "RequestPodVersion: " << QByteArray((const char*)data, length).toHex(' ');
}

void InyyoA102Pro::setReturnFrequency(uint8_t frequency)
{
    // 数据码1为回传频率，支持1-10Hz，0x01:1Hz,0x02:2Hz,…,0x0A:10Hz
    uint8_t data[7];
    int length = 0;
    packData(0x20, 0x00, frequency, 0x00, data, length);
    sendToPod(data, length);
    qDebug() << "SetReturnFrequency: " << QByteArray((const char*)data, length).toHex(' ');
}

void InyyoA102Pro::resetStreamIp()
{
    uint8_t data[7];
    int length = 0;
    packData(0x21, 0x00, 0x00, 0x00, data, length);
    sendToPod(data, length);
    qDebug() << "ResetStreamIp: " << QByteArray((const char*)data, length).toHex(' ');
}

void InyyoA102Pro::requestStreamIp()
{
    uint8_t data[7];
    int length = 0;
    packData(0x21, 0x01, 0x00, 0x00, data, length);
    sendToPod(data, length);
    qDebug() << "RequestStreamIp: " << QByteArray((const char*)data, length).toHex(' ');
}

void InyyoA102Pro::podPower(uint8_t operation)
{
    // 0x00:云台开机，0x01:关机，0x02:云台重启，0x03:机芯重启
    uint8_t data[7];
    int length = 0;
    packData(0x22, operation, 0x00, 0x00, data, length);
    sendToPod(data, length);
    qDebug() << "PodPower: " << QByteArray((const char*)data, length).toHex(' ');
}


void InyyoA102Pro::packData(uint8_t command1, uint8_t command2, uint8_t data1, uint8_t data2, uint8_t * packedData, int &packedLength)
{
    // uint8_t data[8] ={0};
    packedData[0] = 0xff; // 帧头
    // 被控设备逻辑地址
    packedData[1] = _address;
    // “指令码”表示不同的动作
    packedData[2] = command1;
    packedData[3] = command2;
    // “数据码”1、2 表示吊舱指令对应的参数
    packedData[4] = data1;
    packedData[5] = data2;
    // “校验码”= 字节 2 + 字节 3 + 字节 4 + 字节 5 + 字节 6
    packedData[6] = checkCode(packedData + 1, 5);
    packedLength = 7;
}

bool InyyoA102Pro::unpackData(uint8_t *data, int dataLength)
{
    bool success = false;
    if ( dataLength < 29 ) //回复数据长度有29（吊舱）和34（挂载平台）两种
        return success;
    int messageId = 0;
    int messageLength = 0;
    uint8_t checkSum = 0;     // 消息包中的校验和
    uint8_t caculatedSum = 0; // 计算的校验和
    for(int i = 0; i < dataLength; i++) {
        if (data[i] == 0xEE) {// 可能是帧头
            messageId = data[i + 1];
            if (messageId != 0x01 && messageId != 0x81) // 总共接受两包数据，不是这两包数据就重新找帧头
                continue;
            messageLength = data[i + 2];
            if (messageLength < 29 || i + messageLength < dataLength) // 长度太小不够一包，当前帧头错误，继续循环寻找帧头
                continue;
            checkSum = data[i + messageLength - 1];
            for(int j = i; j < i + messageLength - 1; j++) {
                caculatedSum += data[j];
            }
            if (caculatedSum == checkSum) {
                uint16_t distance = 0;
                memcpy(&distance, data + i + 3, 2);
                _laserDistance = distance * 0.1f;
                int16_t roll = 0;
                memcpy(&roll, data + i + 5, 2);
                _podRoll = roll * 0.1f;
                emit podRollChanged();
                int16_t pitch = 0;
                memcpy(&pitch, data + i + 7, 2);
                _podPitch = pitch * 0.1f;
                emit podPitchChanged();
                int16_t yaw = 0;
                memcpy(&yaw, data + i + 9, 2);
                _podYaw = yaw * 0.1f;
                emit podYawChanged();
                _zoom = data[i + 11];
                emit zoomChanged();
                _trackStatus = data[i + 12];
                int32_t lat = 0;
                memcpy(&lat, data + i + 13, 4);
                _targetLatitude = lat / 10000000.0;
                int32_t lon = 0;
                memcpy(&lon, data + i + 17, 4);
                _targetLongitude = lon / 10000000.0;
                int16_t alt = 0;
                memcpy(&alt, data + i + 21, 2);
                _targetAltitude = alt * 0.1f;
                uint16_t zoomDecimal = 0;
                memcpy(&zoomDecimal, data + i + 23, 2);
                _targetAltitude = alt * 0.1f;
                _podMode = data[i + 27];
                success = true;
                break;
            }
        }
    }
    return success;
}

uint8_t InyyoA102Pro::checkCode(uint8_t *data, int length)
{
    uint8_t sum = 0;
    for (int i = 0; i < length; i++) {
        sum += data[i];
    }
    return sum;
}

int InyyoA102Pro::sendToPod(uint8_t *data, int dataLength)
{
    // 发送
    if (_tcpSocket.isValid() && _connected) {
        int length = _tcpSocket.write((char*)data, dataLength);
        return length;
    }
    else {
        qDebug() << "吊舱tcpsocket不可用，或者未连接";
        return -1;
    }
}

float InyyoA102Pro::podRoll() const
{
    return _podRoll;
}

float InyyoA102Pro::podPitch() const
{
    return _podPitch;
}

float InyyoA102Pro::podYaw() const
{
    return _podYaw;
}

float InyyoA102Pro::zoom() const
{
    return _zoom;
}

bool InyyoA102Pro::connected() const
{
    return _connected;
}


void InyyoA102Pro::bytesReceivedFromTcp()
{
    QByteArray bytes = _tcpSocket.readAll();
    unpackData(reinterpret_cast<uint8_t*>(bytes.data()), bytes.length());
}
