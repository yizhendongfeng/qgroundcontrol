#ifndef GCU_H
#define GCU_H

#include "VideoSettings.h"
#include <QObject>
#include <QTimer>
#include <QTcpSocket>

#define MAXBUFFERLENGTH 128 //目前最长帧长度为84

enum CameraCommand {
    /**********   吊舱功能 **********/
    EMPTY       = 0x00,    //用于隔开带有相同指令的控制命令
    //吊舱校准，校准时需保持吊舱静止，校准过程将持续数秒钟。校准时需保持吊舱静止，校准过程将持续数秒钟
    // 执行状态：成功：0x00；  失败：0x01 ；执行中：0x02
    CALIBRATE   = 0x01,
    //回中，锁定与跟随模式下，吊舱俯仰及指向回中，工作模式不变；俯拍模式下，吊舱指向回中，工作模式不变；
    //角度控制、欧拉角控制、凝视模式、跟踪模式及 FPV 模式下，吊舱不响应回中命令；执行过程中，控制量需置为无效
    CENTER      = 0x03,
    //角度控制 控制量：滚转 - 期望欧拉角；俯仰 - 期望欧拉角；偏航 - 期望相对角度；成功:0x00，失败:0x01
    ANGLECONTROL= 0x10,
    //指向锁定，控制量：滚转 - 无效；俯仰 - 期望角速度；偏航 - 期望角速度；成功：0x00，失败：0x01
    YAWLOCK     = 0x11,
    //指向跟随，控制量：滚转 - 无效；俯仰 - 期望角速度；偏航 - 期望角速度（为 0 或无效时跟随载机）；成功：0x00，失败：0x01
    YAWFOLLOW   = 0x12,
    //俯拍,控制量：滚转 - 无效；俯仰 - 无效（欧拉角 -90°）；成功：0x00，失败：0x01
    //偏航 - 期望角速度（由指向跟随模式切换到俯拍模式后，控制量为 0 或无效时跟随载机）
    HIGHANGLESHOT= 0x13,
    // 欧拉角控制,控制量：滚转 - 期望欧拉角；俯仰 - 期望欧拉角；偏航 - 期望欧拉角；成功：0x00，失败：0x01
    EULERCONTROL = 0x14,
    GAZEGEO      = 0x15,   //凝视（地理坐标引导），需要上传目标点经纬高类型均为int32_t
    GAZETARGET   = 0x16,   // 凝视（地理目标锁定）,锁定吊舱画面中心的地理坐标
    TRACK        = 0x17,    //跟踪，框选目标跟踪，需传入左上角，右下角像素点
    POINTANDMOVE = 0x1a,   // 指点平移，应该是画面上点一下作为相机画面中心
    FPV          = 0x1c,   //横滚、俯仰、偏航的期望相对角度

    /**********    相机功能   **********/
    OSDCOORD     = 0x06,   // osd显示坐标类型TT：U8，0x00- 载机坐标；0x01- 目标坐标
    IMAGEFLIP    = 0x07,   //图像自动翻转,TT：U8，0x00- 自动翻转开；0x01- 自动翻转关
    TIMEZONE     = 0x08,   //时区,TT：S8
    TAKEPHOTO    = 0x20,   //拍照,参数：0x01
    RECORD       = 0x21,   //录像开始/停止，参数：0x01
    ZOOMIN       = 0x22,   //连续放大
    ZOOMOUT      = 0x23,   //连续缩小
    ZOOMSTOP     = 0x24,   //停止变倍
    //指定倍率，参数需要三个字节KK（触发相机序号） ZZ ZZ S16，[-32768, -10]，[1, 10000]，负值区为期望倍率，分辨率为 0.1x；
    //正值区为期望倍率比例，1 对应最小倍率，10000 对应最大倍率。本命令对应的最大倍率以吊舱所能实现为准以最大倍率为 30 倍的相机为例，
    //-10 与 1均对应 1 倍，-150 与 5000 均对应 15 倍，-300与 10000 均对应 30 倍
    SETZOOM      = 0x25,
    FOCUS        = 0x26,   // 参数0x01
    PALETTE      = 0x2a,   //参数0x02    TT U8，[0,10]，要切换的热成像调色盘模式。此值为 0 时，调色盘循环切换
    NIGHTVISION  = 0x2b,   //参数0x01 TT : U8，0x00- 夜视关；0x01- 夜视开；0x02- 自动
    AREATEMP     = 0x30,   //区域测温，需上传左上及右下坐标
    //温度报警,TT：U8，0x00- 关闭温度报警；0x01- 开启温度报警HH HH / LL LL：S16，高低温警告温度，分辨率 0.1℃
    TEMPWARN     = 0x31,
    //等温线,TT：U8，0x00- 关闭等温线；0x01- 区间内模式HH HH / LL LL：S16，高低温阈值，分辨率0.1℃
    ISOTHERM     = 0x32,
    //指点测温,TT：U8，0x00- 关闭指点测温；0x01- 开启指点测温X0 X0 / Y0 Y0 : U16，
    //[0,10000]，分别为光标测温目标点的水平与垂直坐标，原点为图像左上角，X 坐标向右为正，Y 坐标向下为正，
    //左上角坐标为 [0,0], 右下角坐标为[10000,10000]
    POINTTEMP    = 0x33,
    OSD          = 0x73,   // TT：U8，0x00-OSD 关；0x01-OSD 开
    PIP          = 0x74,   //画中画,TT：U8，[0,4], 要切换的画中画模式。此值为0时，画中画模式循环切换
    OBJIDENTIFY  = 0x75,   //TT：U8，0x00- 目标识别关；0x01- 目标识别开
    DIGITALZOOM  = 0x76,   //数字变焦,TT：U8，0x00- 数字变焦关；0x01- 数字变焦开
    FILLINLIGHT  = 0x80,   //TT：U8，[0,255]，补光照明亮度
    RANGING      = 0x81    //连续测距,TT：U8，0x00- 测距关闭；0x02- 测距开启

};

struct GcuMessageSend
{
    uint16_t length;
    uint8_t  version;

    // 上位机发送数据主帧格式32字节
    // 控制量为期望角速率时，范围[-1500,1500]，分辨率为0.1/当前主画面倍数（°/s）
    // 控制量为期望欧拉角时，数据范围 [-18000,18000]，分辨率 0.01°
    // 控制量为吊舱与载机的期望相对角度时，数据范围[-18000,18000]，分辨率 0.01°
    int16_t  roll;
    int16_t  pitch;
    int16_t  yaw;

    //状态标志,B7~B3：预留 , 此位为 0
    //B2：0- 控制量无效，此时期望角速率为 0，期望欧拉角与期望相对角度维持当前值；1- 控制量有效
    //B1：预留 , 此位为 0
    //B0：0- 载机惯导数据无效；1- 载机惯导数据有效
    uint8_t  status;

    int16_t  vehicleRoll;      // [-18000, 18000)，分辨率0.01deg
    int16_t  vehiclePitch;     // [-9000, 9000]，分辨率0.01deg
    uint16_t vehicleYaw;       // [0,36000)，分辨率0.01deg
    int16_t  vehicleAccNorth;  //载机北向加速度 S16，分辨率 0.01m/s2，北向为正
    int16_t  vehicleAccEast;   //载机东向加速度 S16，分辨率 0.01m/s2，东向为正
    int16_t  vehicleAccUp;     //载机天向加速度 S16，分辨率 0.01m/s2，天向为正
    int16_t  vehicleVelNorth;  //载机北向速度 S16，分辨率 0.01m/s，北向为正
    int16_t  vehicleVelEast;   //载机东向速度 S16，分辨率 0.01m/s，东向为正
    int16_t  vehicleVelUp;     //载机天向速度 S16，分辨率 0.01m/s，天向为正
    uint8_t  subFrameRequest;  //填写需要返回的副帧帧头，如不需要请求副帧，此处为 0x00
    uint8_t  reserved0[7];     //0x00

    // 上位机发送数据副帧格式
    uint8_t  subFrameHeader;   // 副帧帧头
    int32_t  vehicleLon;       //载机经度 S32，分辨率 1e-7deg
    int32_t  vehicleLat;       //载机纬度 S32，分辨率 1e-7deg
    int32_t  vehicleAlt;       //载机海拔高度 S32，分辨率 1mm
    uint8_t  satelliteNum;     // 载机卫星数
    uint32_t gnssMs;           // 载机 GNSS 毫秒数 U32
    int16_t  gnssWeek;         //载机 GNSS 周数
    int32_t  relativeAlt;      //相对高度 S32，分辨率 1mm，如不需要可为 0
    uint8_t  reserved1[9] ={}; //预留 0x00
    CameraCommand command;
};
Q_DECLARE_METATYPE(GcuMessageSend);
struct MessageRsv {
    uint16_t  length;
    uint8_t   version;

    // 数据主帧
    // 吊舱工作模式:0x10- 角度控制模式;0x11- 指向锁定模式；0x12- 指向跟随模式；
    // 0x13- 俯拍模式；0x14- 欧拉角控制模式；0x16- 凝视模式；0x17- 跟踪模式 ;
    uint8_t   mode;
    // 吊舱状态标志B15~B13：预留;B12：0- 正置上电，1- 倒置上电；B11：预留；B10：0- 补光关；，1- 补光开；B9：0- 夜视关，1- 夜视开；
    // B8：0- 测距关,1- 测距开; B7：0- 测距值与目标坐标无效,1- 测距值与目标坐标有效;B6~B1：预留;
    // B0：0- 跟踪目标失败，脱靶量数据无效, 1- 跟踪目标成功，脱靶量数据有效;
    uint16_t  status;
    int16_t   missHorizentol; //水平方向脱靶量 S16，[-1000,1000]，原点为图像中心，向右为正
    int16_t   missVerticle;   //垂直方向脱靶量 S16，[-1000,1000]，原点为图像中心，向下为正

    // S16，[-18000,18000)，相机相对于载机的角度值，分辨率 0.01deg。此数据由电机编码器角度解算得出，不依赖载机惯导数据。
    // 相机坐标系定义及旋转顺序详见附录 3
    int16_t   offsetAngleX;   // 相机 X 轴相对角度
    int16_t   offsetAngleY;   // 相机 Y 轴相对角度
    int16_t   offsetAngleZ;   // 相机 Z 轴相对角度

    // 欧拉角，分辨率 0.01deg。此数据依赖载机惯导数据
    int16_t   absRoll;        //相机绝对滚转角S16，[-9000,9000]
    int16_t   absPitch;       //相机绝对俯仰角S16，[-18000,18000)
    uint16_t  absYaw;         //相机绝对偏航角U16，[0,36000)

    //S16, 分辨率 0.1deg/s，相机坐标系定义及旋转顺序详见附录 3
    int16_t   absXRate;       //相机 X 轴绝对角速度
    int16_t   absYRate;       //相机 Y 轴绝对角速度
    int16_t   absZRate;       //相机 Z 轴绝对角速度

    uint8_t   reserved0[7];


    //GCU返回数据副帧格式
    uint8_t   subFrameHeader;  //0x01 副帧帧头
    uint8_t   hardwareVersion; // 硬件版本
    uint8_t   firmwareVersion; //固件版本
    uint8_t   podCode;         // 吊舱代码 吊舱型号代码详见附录 7
    //
    uint16_t  errorCode;       //故障代码,B15：GCU 硬件故障；B14：GNSS 未定位；B13：Mavlink 通信频率异常；B12~B8：预留；B7：吊舱硬件故障；B6~B0：预留；

    int32_t   targetDist;      //S32，测距仪测得的目标距离，分辨率 0.1m输出 -1m 或 0 时表示距离无效
    int32_t   targetLon;       //S32，分辨率 1e-7deg
    int32_t   targetLat;       //S32，分辨率 1e-7deg
    int32_t   targetAlt;       //S32，分辨率 1mm

    uint16_t  camera1Zoom;     //1 号相机（默认为可见光变焦相机）当前倍率 U16，分辨率 0.1x
    uint16_t  camera2Zoom;     //2 号相机（默认为热成像相机）当前倍率 U16，分辨率 0.1x
    //B7：0- 测温不可用，1- 测温可用；B6：0- 区域测温关，1- 区域测温开；B5：0- 温度报警关，1- 温度报警开；
    //B4：0- 等温线关，1- 等温线开；B3：0- 指点测温关，1- 指点测温开；B2：预留；B1：高温报警；B0：低温报警
    uint8_t   thermalStatus;
    //B15：0- 目标检测关，1- 目标检测开；B14：0- 数字变倍关，1- 数字变倍开；B13：0-OSD 显示关，1-OSD 显示开；
    //B12：0-OSD 载机坐标，1-OSD 目标点坐标；B11：0- 图像自动翻转开，1- 图像自动翻转关；B10~B5：预留；
    //B4：0- 未录像，1- 录像中；B3：预留；B2~B0：uint_t，画中画模式
    uint16_t  cameraStatus;
    int8_t    timeZone;        //时区,S8
    uint8_t   reserved1[2];

    uint8_t   command;         // 控制命令
    uint8_t   commandStatus;   //只有非空命令才有
};
enum CameraDirection {
    LOOKFORWARD  = 0,
    LOOKBACKWARD = 1,
    LOOKLEFT     = 2,
    LOOKRIGHT    = 3
};

class GCU : public QObject
{
    Q_OBJECT
public:
    explicit GCU(VideoSettings * videoSettings);

    Q_PROPERTY(QString cameraName READ cameraName  NOTIFY cameraNameChanged FINAL)
    Q_PROPERTY(bool visualLight READ visualLight WRITE setVisualLight NOTIFY visualLightChanged FINAL)
    Q_PROPERTY(double roll  READ roll   NOTIFY rollChanged FINAL)
    Q_PROPERTY(double pitch READ pitch  NOTIFY pitchChanged FINAL)
    Q_PROPERTY(double yaw READ yaw NOTIFY yawChanged FINAL)
    Q_PROPERTY(double zoom READ zoom  NOTIFY zoomChanged FINAL)
    Q_PROPERTY(QString ip READ ip WRITE setIp NOTIFY ipChanged FINAL)
    Q_PROPERTY(int port READ port WRITE setPort NOTIFY portChanged FINAL)
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged FINAL)
    Q_PROPERTY(bool recording READ recording  NOTIFY recordingChanged FINAL)
    Q_PROPERTY(int podMode READ podMode WRITE setPodMode NOTIFY podModeChanged FINAL)
    Q_PROPERTY(bool fillLight READ fillLight WRITE setFillLight NOTIFY fillLightChanged FINAL)
    Q_PROPERTY(bool nightVision READ nightVision WRITE setNightVision NOTIFY nightVisionChanged FINAL)


    Q_INVOKABLE void connectToGcu(bool connect);
    Q_INVOKABLE void startRecording(bool start);
    Q_INVOKABLE void takePhoto();
    // Q_INVOKABLE void adjustRoll(double roll);
    Q_INVOKABLE void adjustPitch(double pitch);
    Q_INVOKABLE void adjustYaw(double yaw);
    Q_INVOKABLE void setDirection(CameraDirection direction, const double pitch);
    Q_INVOKABLE void setCameraCommand(CameraCommand command);
    /**
     * @brief continuousZoom
     * @param zoom 1：放大，-1：缩小，0：停止缩放
     */
    Q_INVOKABLE void continuousZoom(const double zoom);
    Q_INVOKABLE void setVisualLight(const bool visualLight);
    Q_INVOKABLE void setOsd(const bool open);
    /**
     * @brief setPodHeadMode 设置吊舱模式，如指向的锁定，跟随等
     * @param mode
     */
    Q_INVOKABLE void setPodHeadMode(const int mode);

    /**
     * @brief rotatePod 旋转吊舱
     * @param axis 0: 偏航， 1： 俯仰
     * @param rate 偏航正：向右转，负：向左转。俯仰正：向下，负：向上
     */
    Q_INVOKABLE void rotatePod(const int axis, float rate);

    /**
     * @brief setPodAngle
     * @param axis 0: 偏航， 1： 俯仰，2：横滚
     * @param angle
     */
    Q_INVOKABLE void setPodAngle(const int axis, const float angle);
    /**
     * @brief setZoom 设置放大倍数
     * @param value [-400, -10]表示1~40倍
     */
    Q_INVOKABLE void setZoom(const float value);

    /**
     * @brief moveToPoint指点平移,X 轴向右为正，Y 轴向下为正，左上角坐标为 [0,0],
     * 右下角坐标为 [10000,10000]；执行过程中，控制量需置为无效。此命令执行后，
     * 吊舱将切换为指向锁定模式
     * @param x 为目标点水平坐标
     * @param y 为目标点垂直坐标
     */
    Q_INVOKABLE void moveToPoint(const int x, const int y);

    /**
     * @brief calibratePod 吊舱校准
     */
    Q_INVOKABLE void calibratePod();
    /**
     * @brief turnOnFillLight 打开补光灯
     * @param brightness，照明亮度[0,255]
     */
    Q_INVOKABLE void turnOnFillLight(int brightness);

    /**
     * @brief podCentering 吊舱回中
     */
    Q_INVOKABLE void podCentering();

    /**
     * @brief podTrack 跟踪选中框中物体
     * @param start  进入或退出跟踪模式
     * @param startX 屏幕左上角x，
     * @param startY 屏幕左上角y
     * @param endX   屏幕右下角x
     * @param endY   屏幕右下角y
     */
    Q_INVOKABLE void podTrack(bool start, const int startX, const int startY, const int endX, const int endY);

    QString cameraName()  {return _cameraName;}
    bool    visualLight() {return _visualLight;}
    double  roll()        {return _roll;}
    double  pitch()       {return _pitch;}
    double  yaw()         {return _yaw;}
    double  zoom()        {return _zoom;}
    QString ip()          {return _ip;}
    int     port()        {return _port;}
    bool    connected()   {return _connected;}
    bool    recording()   {return _recording;}
    int     podMode()     {return (int)_podMode;}
    bool    fillLight()   {return _fillLight;}
    bool    nightVision()   {return _nightVision;}
    //相机设置
    void setIp(const QString ip);
    void setPort(const int port);
    void setPodMode(const int mode);
    void setFillLight(const bool turnOn);
    void setNightVision(const bool nightVision);

    /**
     * @brief Encode 消息打包
     * @param msg    要打包的消息
     * @return       发送结果， -1:发送失败，消息长度：发送成功
     */
    int sendMsg(GcuMessageSend msg, uint8_t* commandParam, uint8_t commandLength);
    /**
     * @brief Decode
     * @param buf    缓存
     * @param length 长度
     * @param msg    解包后的信息
     * @return       是否解析成功
     */
    bool decode(uint8_t* buf, uint16_t length, MessageRsv& msg);

    uint16_t CaculateCrc16(uint8_t *ptr, uint8_t len);

signals:
    void cameraNameChanged();
    void visualLightChanged();
    void rollChanged();
    void pitchChanged();
    void yawChanged();
    void zoomChanged();
    void connectedChanged();
    void ipChanged();
    void portChanged();
    void recordingChanged();
    void podModeChanged();
    void fillLightChanged();
    void nightVisionChanged();
    void calibrateStateChanged(int status);
public slots:

    // void    _mavlinkMessageReceived (const mavlink_message_t& message);
    void    bytesReceivedFromTcp();
    void    sendEmptyCommand();
    /**
     * @brief receiveVehicleMessage 接收无人机导航数据
     * @param msg
     */
    void    receiveVehicleMessage(const GcuMessageSend& msg);
private:
    QTimer timerSendEmptyCommand;
    QTimer timerResendEmptyCommand;  // 如果长时间接收不到gcu的数据，重新发送空命令
    uint8_t bufferSend[MAXBUFFERLENGTH] = {};
    uint8_t bufferRsv[MAXBUFFERLENGTH]  = {};
    // uint16_t lengthInBuffer = MAXBUFFERLENGTH;// 剩余空间长度
    uint16_t bytesCountInRsvBuffer = 0;// 已占用缓存长度
    uint16_t bytesCountInSendBuffer = 0;          // 需要发送的命令缓存长度
    bool _needResendLastCommand = false;
    QTcpSocket tcpSocket;
    VideoSettings * _videoSettings;
    // 相机信息
    QString _cameraName;
    bool    _visualLight = true;
    double  _roll = 0.0;
    double  _pitch = 0.0;
    double  _yaw   = 0.0;
    double  _zoom  = 1.0;
    int16_t _yawRateTarget = 0;
    int16_t _pitchRateTarget = 0;
    int16_t _yawAngleTarget = 0;
    int16_t _pitchAngleTarget = 0;
    int16_t _rollAngleTarget = 0;
    uint8_t _upRight = 0;  // 0：正置上电，1：倒置上电
    uint16_t _podStatus;   // 吊舱状态MessageRsv.status
    bool    _fillLight = false;    // 补光灯
    bool    _nightVision = false;  // 夜视

    GcuMessageSend msgFromVehicle = {};

    bool    _recording = false;
    CameraCommand _lastCommandSent; // 最后一次发送的命令，用于检查应答
    CameraCommand _lastCommandSentNonEmpty; // 最后一次发送的非空命令
    CameraCommand _podMode = YAWFOLLOW; // 吊舱的控制模式，如：跟随，锁定，角度控制等
    QTimer* timerConnectToGcu;
    QString _ip;
    int     _port;
    bool    _connected  = false;   //是否连接相机
    bool    _autoConnect = false;
    bool    _imuValid = false;
    const QMap<uint8_t, QString> _podCodeNames = {{0, "Z-6A"}, {2, "Z-6C"},{25, "Z-8RB"},{26,"Z-8RC"},
                                                 {31,"Z-9B_V3"},{40,"D-80AI"},{41,"D-90AI"},{44,"D-80Pro"},
                                                 {45,"D-90Pro(TA)"},{49,"Z-1Pro"},{50,"Z-1Mini"},{52,"Z-2Mini"},
                                                 {53,"D-125AI(T)"},{55,"D-90DE"},{57,"D-125AI(V)"},
                                                 {58,"Z-9B_V4(T)"},{59,"Z-9B_V4(V)"},{60,"D-90Pro(VA)"},
                                                 {61,"D-90Pro(T)"},{62,"D-90Pro(V)"}};
signals:
};

#endif // GCU_H
