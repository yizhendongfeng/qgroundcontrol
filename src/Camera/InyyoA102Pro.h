#ifndef INYYOA1_2PRO_H
#define INYYOA1_2PRO_H

#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include "VideoSettings.h"

class InyyoA102Pro : public QObject
{
    Q_OBJECT
public:
    explicit InyyoA102Pro(VideoSettings * videoSettings);
    Q_PROPERTY(float podRoll READ podRoll NOTIFY podRollChanged FINAL)
    Q_PROPERTY(float podPitch READ podPitch NOTIFY podPitchChanged FINAL)
    Q_PROPERTY(float podYaw READ podYaw NOTIFY podYawChanged FINAL)
    Q_PROPERTY(float zoom READ zoom NOTIFY zoomChanged FINAL)
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged FINAL)



    Q_INVOKABLE void connectToPod(bool connect);
    Q_INVOKABLE void stopRoate();
    Q_INVOKABLE void pitchRotate(bool up, uint8_t speed = 0xff);
    Q_INVOKABLE void yawRotate(bool left, uint8_t speed = 0xff);
    Q_INVOKABLE void zoomChange(bool zoomIn, uint8_t speed = 0x04);
    Q_INVOKABLE void zoomStop();
    Q_INVOKABLE void moveToPoint(uint8_t x, uint8_t y);// 点范围0x00-0xff
    Q_INVOKABLE void setPitchAngle(float angle);
    Q_INVOKABLE void setYawAngle(float angle);
    //指点跟踪(画面XY坐标)（指令码1为0x11,指令码2为0x00，数据码1为X值(↔)，数据码2为Y值(↕)，速度范围0x00-0xFF）
    Q_INVOKABLE void startTrackToPoint(uint8_t x, uint8_t y); // 跟踪该点下的目标，速度范围0x00-0xFF
    Q_INVOKABLE void stopTrackToPoint(); //
    Q_INVOKABLE void targetDetect(bool on);
    Q_INVOKABLE void auxiliaryTracking(bool on); //辅辅助检测功能是利用检测能力防止跟踪遮挡时丢失目标，仅具备AI检测识别跟踪功能的光电吊舱支持此功能
    Q_INVOKABLE void takePhoto();
    Q_INVOKABLE void recording(bool on);
    Q_INVOKABLE void followPlatform(bool on); // 默认跟随飞机
    Q_INVOKABLE void lookDown();    //一键向下
    Q_INVOKABLE void lookForward(); //一键回中
    Q_INVOKABLE void pictureInPictureSwitch(uint8_t mode); //画中画切换
    Q_INVOKABLE void thermalModeSwitch(uint8_t mode); //热成像切换
    Q_INVOKABLE void thermalZoom(uint8_t zoom); //热成像电子变倍
    Q_INVOKABLE void stabilization(bool on); //开启/关闭电子稳像
    Q_INVOKABLE void osd(bool on); //开启OSD
    Q_INVOKABLE void manualFocus(bool on); //可见光聚焦
    Q_INVOKABLE void changeFocus(bool near); //手动聚焦
    Q_INVOKABLE void stopFocus(); //停止手动聚焦
    Q_INVOKABLE void requestSbusChannel(); //查询sbus通道配置
    Q_INVOKABLE void requestPodTime();     //查询机芯时间
    Q_INVOKABLE void requestPodVersion();  //查询版本
    Q_INVOKABLE void setReturnFrequency(uint8_t frequency);  //设置回传频率
    Q_INVOKABLE void resetStreamIp();  //恢复默认拉流地址
    Q_INVOKABLE void requestStreamIp();  //查询拉流地址
    //云台开机（指令码2为0x00）,云台关机（指令码2为0x01）,云台重启（指令码2为0x02）,机芯重启（指令码2为0x03）
    Q_INVOKABLE void podPower(uint8_t operation);  // 云台开关机重启




    void packData(uint8_t command1, uint8_t command2, uint8_t data1, uint8_t data2, uint8_t *packedData, int& packedLength);
    bool unpackData(uint8_t *data, int dataLength);
    uint8_t checkCode(uint8_t* data, int length);
    int   sendToPod(uint8_t* data, int dataLength);
    float podRoll() const;
    float podPitch() const;

    float podYaw() const;

    float zoom() const;

    bool connected() const;

private:
    QTcpSocket _tcpSocket;
    VideoSettings* _videoSettings;
    QTimer* _timerConnectToPod;
    QString _ip;
    int     _port;
    bool    _connected  = false;   //是否连接相机
    bool    _autoConnect = false;


    uint8_t _address = 0x01; //被控设备逻辑地址，地址范围:0x01-0xFF
    float _laserDistance = 0;  // 激光测距
    float _podRoll = 0;        // 云台横滚角度，左倾为负，右倾为正
    float _podPitch = 0;       // 云台俯仰角度，低头为负，抬头为正
    float _podYaw = 0;         // 云台航向角度，右转为 0～180，左转为 0～-180
    float _zoom = 0;           // 可见光变倍倍数
    uint8_t _trackStatus = 0;  // 跟踪状态，0x00:未跟踪，0x01:正在跟踪
    double _targetLatitude = 0; // 目标纬度
    double _targetLongitude = 0;// 目标经度
    float _targetAltitude = 0; // 目标高度
    uint8_t _podMode = 0x01;   // 云台工作模式 0x00:启用跟随，0x01:禁用跟随，0x02:一键向下
    const uint8_t HEADER = 0xff;

signals:
    void connectedChanged();
    void podRollChanged();
    void podPitchChanged();
    void podYawChanged();
    void zoomChanged();

public slots:
    void bytesReceivedFromTcp();
};

#endif // INYYOA1_2PRO_H
