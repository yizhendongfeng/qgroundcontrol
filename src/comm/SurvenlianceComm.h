/**
 * @copyright (C) 2021 版权所有
 * @brief 岛礁检测，传输飞机状态至另外一个地面站
 * @author 张纪敏 <869159813@qq.com>
 * @version 1.0.0
 * @date 2021-05-24
 */
#pragma once

#include <QObject>
#include <QUdpSocket>
#include <QFile>

#pragma pack(1)
struct UavStatusData        // 发送给综合信息处理系统的无人机状态
{
    uint16_t year;          //年
    uint8_t month;          //月
    uint8_t day;            //日
    uint8_t hour;           //时
    uint8_t minute;         //分
    uint8_t second;         //秒

    uint32_t longitude;      // =经度（度.度）/360*pow(2.0,32)；
    uint32_t latitude;       // =纬度（度.度）/180*pow(2.0,32)；

    float_t yaw;                //航向（度）
    float_t vehicleVelocity;    //航速(m/s)
    float_t windVelocityReal;   //真风速(m/s)
    float_t windDirectionReal;  //真风向（度）

    float_t airPressure;        //气压（Pa）
    float_t temperature;        //温度（摄氏度）
    float_t humidity;           //湿度（%）
    float_t windSpeedVisual;    //视风速（m/s）
    float_t windDirectionVisual;//视风向（m/s）
    float_t altitude;           //高度(m)
    float_t reserved4;
    float_t reserved5;
    float_t reserved6;
    float_t reserved7;
    float_t reserved8;
};
#pragma pack()
Q_DECLARE_METATYPE(UavStatusData);

class Vehicle;

class SurvenlianceComm : public QObject
{
    Q_OBJECT
public:
    explicit SurvenlianceComm(QObject *parent = nullptr);
    ~SurvenlianceComm() override;
    Vehicle* getVehicle() const;
    void setVehicle(Vehicle *value);

private:
    Vehicle* vehicle;
    uint8_t sendBuffer[82] = {'$', 'D', 'a', 't', 'a', 76, 2};
    UavStatusData uavStatusData = {};

    QUdpSocket udpSocket;
    QString targetIp = "127.0.0.1";   //综合信息处理系统默认ip地址
    uint16_t targetPort = 8000;       //综合信息处理系统默认端口
    QHostAddress targetAddress;
    uint16_t qgcPort = 8001;          //QGroundControl默认端口
    QFile fileLogger;
    QTextStream* fileStream = nullptr;
signals:
public slots:
    void SlotTestTimeout();
    void SlotSendData(const UavStatusData& data);
};

