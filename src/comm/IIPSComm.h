/**
 * @copyright (C) 2021 版权所有
 * @brief QGroundControl（简写QGC）发送无人机信息至综合信息处理系统（Integrated information processing system，简写IIPS），并接收相关命令及航线文件
 * 使用UDP进行通信，接收机发送的IP地址及端口可在"C:\Users\当前用户\AppData\Roaming\QGroundControl.org\QGroundControl.ini"中iips组中设置
 * @author 张纪敏 <869159813@qq.com>
 * @version 1.0.0
 * @date 2021-02-17
 */
#pragma once

#include <QObject>
#include <QUdpSocket>

#include "IIPSProtocol.h"

#pragma pack(1)
struct QGCStatusData        // 发送给综合信息处理系统的无人机状态
{
    uint16_t id;            //载具编号
    double_t timestamp;     //(s)
    uint8_t status;         //0:故障，1:正常
    double_t latitude;      //(rad)
    double_t longitude;     //(rad)
    double_t altitude;      //(m)
    float_t roll;           //(rad)
    float_t pitch;          //(rad)
    float_t yaw;            //(rad)
    float_t velocityNorth;  //(m/s)
    float_t velocityEast;   //(m/s)
    float_t velocityDown;   //(m/s)
    float_t gyroscopeX;     //(rad/s)
    float_t gyroscopeY;     //(rad/s)
    float_t gyroscopeZ;     //(rad/s)
//    float_t accelerometerX; //(m/ss)
//    float_t accelerometerY; //(m/ss)
//    float_t accelerometerZ; //(m/ss)
//    uint8_t satellitesUsed; //卫星数
//    uint8_t fixType;        //定位类型
//    float_t battery;        //(v)
};

struct IIPSCommand     //综合信息处理系统发来的命令
{
    uint16_t id;                    //载具编号
    double_t timestamp;
    uint8_t setting;                //0:锁定，1:解锁，2:起飞，3:悬停，4:跟踪UGV，5:降落，6:盘旋go around，7:返航
    float_t desiredVelocityNorth;   //期望/目标北向速度(m/s)
    float_t desiredVelocityEast;    //期望/目标东向速度(m/s)
    float_t desiredVelocityDown;    //期望/目标地向速度(m/s)
    float_t desiredHeading;         //期望航向(rad)
    double_t desiredLatitude;       //(rad)
    double_t desiredLongitude;      //(rad)
    double_t desiredAltitude;       //(m)
};
typedef struct
{
    double timestamp;
    double latitude;
    double longitude;
    float altitude;
} Waypoint;
#pragma pack()
Q_DECLARE_METATYPE(IIPSCommand);

class Vehicle;
class IIPSComm : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool iipsConnected READ getIipsConnected  NOTIFY iipsConnectedChanged)
    enum UploadType     //data[10]的值
    {
        START = 0,      //任务上传开始
        WAYPOINT = 1,   //航点上传
        END = 2,        //任务上传结束
        RESPONSE = 0XFF //QGC应答
    };

    enum PacketId
    {
        HEARTBEAT = 0,    //心跳
        STANDBY = 1,      //待命
        SURVEY = 2,       //测绘
        LINE = 3,         //航线
        REGION = 4,       //区域
        FOLLOW = 5        //目标跟踪
    };
public:
    explicit IIPSComm(QObject *parent = nullptr);
    ~IIPSComm();
    int SaveWaypointsToJsonFile(PacketId type);
    Vehicle* getVehicle() const;
    void setVehicle(Vehicle *value);
    void AppendVehicle(Vehicle *value);
    void RemoveVehicle(Vehicle *value);
    bool getIipsConnected() const;
    /**
     * @brief SendAck 发送任务上传的应答
     */
    void SendAck();
    /**
     * @brief GetWayPoints 从数据包中获取完整的航点列表，包括开始、航点、结束判断
     * @param type  只分航点（Line）与区域两种
     * @param true:获取成功 false:接收获取失败，id不匹配
     */
    bool GetWayPoints(PacketId type);

public slots:
    void SlotSendData(const QGCStatusData &data);
    void SlotReceiveData();
    void SlotTestTimeout();
private:
    const uint8_t heartbeatPacketId = 0;              //IIPS心跳数据包编号
//    const uint8_t commandPacketId = 128;              //IIPS命令数据包编号
    const uint8_t commandPacketLength = 51;           //IIPS命令数据包长度
    const uint8_t statusPacketId = 128;               //QGC状态数据包编号
    const uint8_t statusPacketLength = 71;            //QGC状态数据包长度

    /* IIPS任务上传的开始、发送与结束消息编号都是4，是通过数据包长度及数据包中且data[10]来区分，
     * 开始上传数据包长度为11 且data[10]=0，航点上传数据包长度为31且data[10]=1，上传结束数据包长度为11且data[10]=2
     * qgc请求数据包编号为3，数据包长度为11，data[10]=0xFF
    */
    const uint8_t missionUploadPacketId = 4;                //IIPS任务上传数据包编号
    const uint8_t missionUploadLength = 11;                 //数据长度
    const uint8_t missionUploadWaypointLength = 31;         //路点数据长度

    const uint8_t missionUploadResponsePacketId = 129;      //qgc发送至IIPS的航线任务请求数据包编号
    const uint8_t missionUploadResponsePacketLength = 11;   //qgc发送至IIPS的航线任务请求数据包长度


    uint16_t sysId = 0;       //载具编号
    QList<Waypoint> listWaypoints;
    Vehicle* vehicle = nullptr;
    QList<Vehicle*> vehicles;
    QGCStatusData data;

    /******************** 通信相关 ********************/
    BinaryPacket sendPacket;
    BinaryPacket receivePacket;
    BinaryBuffer receiveBuffer;
    QGCStatusData statusData;
    IIPSProtocol iipsProtocol;
    QUdpSocket udpSocket;
    QString iipsIp = "127.0.0.1";   //综合信息处理系统默认ip地址
    QHostAddress iipsAddress;
    uint16_t iipsPort = 8000;       //综合信息处理系统默认端口
    uint16_t qgcPort = 8001;        //QGroundControl默认端口
    bool iipsConnected = false;     //综合信息处理系统是否已连接
    const int iipsTimeOut = 3;      //超过此事件认为综合信息处理系统已断开，图标消失
    QTimer timerIipsConnect;
signals:
    void sigSendPlanFile(const QString fileName);
    void iipsConnectedChanged();
};

