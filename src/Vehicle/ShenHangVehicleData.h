#pragma once
#pragma pack(1)

/******************** 航线信息 ********************/


/******************** 航线回复数据 ********************/
struct TotalBankInfo {                  // 整体航路信息（ty_msg0=194,ty_msg1=0）
    uint16_t largeBankInfslCapacity;   // 飞行器可存储的最大大航线中infoslot容量
    uint16_t smallBankInfoslCapacity;   // 飞行器可存储的最大小航线中infoslot容量
    uint16_t largeBankNumber;           // 飞行器可存储的最大大航线条数
    uint16_t smallBankNumber;           // 飞行器可存储的最大小航线条数
    uint16_t idTransientBank;           // 过渡航线对应的编号
};
Q_DECLARE_METATYPE(TotalBankInfo)
struct SingleBankInfo {                 // 单个bank信息（ty_msg0=194,ty_msg1=1，2，5，或6）
    uint16_t idBank;                    // 对应的bank编号
    uint16_t tyBank;                    // 对应的bank类型码
    uint16_t nInfoslotMax;              // 对应的bank最大infoslot容量
    uint16_t iWp;                       // 当前bank中待执行航路点编号
    uint16_t nWp;                       // 前bank中航路点总数
    uint16_t nInfoslot;                 // 当前bank中infoslot总数
    uint16_t idBankSuc;                 // 接续航线编号
    uint16_t idBankIwpSuc;              // 接续航路点编号
    uint16_t actBankEnd;                // 对应航线执行完成后动作
    uint8_t stateBank;                  // 对应航线状态，bit0，0：未装载，1：装载；bit1， 0：未校验，1：已校验；bit2， 0：未锁定；1：已锁定；bit3， 0：空闲，1：占用；bit4：7：保留
    uint8_t switchState;                // 对应的bank编号，0：自动切换开，1：自动切换关
};
struct SingleBankCheckInfo {            // 单个bank校验信息（ty_msg0=194,ty_msg1=3）
    uint16_t idBank;                    // 对应的bank编号
    uint16_t tyBank;                    // 对应的bank类型码
    uint16_t nWp;                       // 前bank中航路点总数
    uint16_t nInfosl;                   // 当前bank中infoslot总数
    uint32_t crc32;                     // 对应bank的crc32校验码
};
struct ErrorBankInfo {                  // 查询单个infoslot返回信息（ty_msg0=1,ty_msg1=4）
    uint8_t errTyMsg1;                  // 出错的命令对应的ty_msg1编号
    uint8_t dgnCode;                    // 错误编码，2：编号超界，4：无效指令，其他：保留
};

struct WaypointInfoSlot {       // 航路点信息报文（ty_msg0=1）
    uint16_t idBank;        // 航线标号，从0开始依次递增，最大值通过配置文件设置
    uint16_t idWp;          // 航路点（waypoint）标号，每个bank之内从0开始，依照主要infoslot数目依次递增，若单一航点由多个infoslot描述，则该航点对应的所有infoslot中的id_wp字段均相等
    uint16_t idInfosl;      // infoslot标号，每个bank之内从0开始随所有类型infoslot数目依次递增
    uint16_t tyInfosl;      // infoslot类型，大类编号（bit0~3）：0：非法，1：主要infoslot，2：补充infoslot，2~15：保留；小类编号（bit4~11）：0：基本，1：动态；bit12~15：航点占用infoslot总数
    double lon;             // 经度：±180
    double lat;             // 纬度：±90
    float alt;              // 单位m
    float radiusCh;         // 特征圆半径，单位为米
    float rVGnd;            // 参考飞行速度，单位为米每秒，非负
    float rRocDoc;          // 参考爬升速度或梯度，单精度浮点数，正值爬升，负值下降，作为爬升速度时单位为米每秒，作为爬升梯度时无量纲。
    float swTd;             // 切换条件延迟时长，单精度浮点数，非负。
    uint16_t swVGnd10x;     // 切换条件速度阈值，双字节无符号整型，单位为0.1米每秒。
    int16_t rHdgDeg100x;    // 切换方位角条件阈值，双字节有号整型，单位为0.01度。
    uint16_t actDept;       // 离开动作字段，双字节无符号整型
    uint16_t actArrvl;      // 到达动作字段，双字节无符号整型
    uint16_t swCondSet;     // 切换条件设置
    uint8_t actInflt;       // 飞行中动作字段
    uint8_t heHdgCtrlMode;  // 高度和航向控制模式字段
    int8_t swAngDeg;        // 盘旋方位角切换条件字段
    uint8_t swTurns;        // 盘旋切换圈数字段
    uint8_t gdMode;         // 制导模式字段
    uint8_t mmvrSty;        // 机动风格字段
    uint8_t transSty;       // 过渡风格字段
    uint8_t reserved0;      // 保留变量
    uint8_t reserved1;      // 保留变量
    uint8_t cs;             // 校验字
};
struct GeneralStatus {      // 常规状态信息报文（ty_msg0=2）
    uint8_t tyObj;          // 0：通用类型，1：直升机，2：固定翼，3：多旋翼，4：串列矢量，5：并列矢量，6：倾转，其他：保留
    uint8_t tyVcf;          // 载具构型信息，0：旋翼构型，1：固定翼构型，其他：保留
    uint8_t modeOp;         // 操纵模式，0：遥控器角速度控制，1：遥控器姿态角控制，2：数传指令角速度控制，3：数传指令姿态角控制，64：定高+遥控器姿态控制，65：定高+数传指令姿态控制，66：定高+遥控器速度控制/航向控制，67：定高+数传指令速度控制/航向控制，192：程控飞行
    uint8_t modeGnssSv;     // gnss解算用卫星数目和解算状态标识字
    int16_t q1I16;          // 姿态四元数构成变量，双字节有符号整型，分度值为（1/32767），符号条件为q0非负
    int16_t q2I16;          // 姿态四元数构成变量
    int16_t q3I16;          // 姿态四元数构成变量
    uint16_t gnssTimeWeek;  // GPS时间周数
    uint32_t gnssTimeMs;    // GPS时间周内毫秒数
    float lonD;             // 经度小数值
    float latD;             // 纬度小数值
    uint16_t lonLatIComp;   // 经纬度整数值
    int16_t crsDeg100x;     // 航向角，双字节有符号整型，单位为0.01度
    float alt;              // 海拔高度，32位节浮点数，单位为米
    float vG;               // 地速，32位浮点数，单位为米每秒
    float vV;               // 天向速度，32位浮点数，单位为米每秒
    uint16_t bankId;        // 当前航线序号
    uint16_t wpId;          // 当前航路点序号
    uint16_t rsv0;          // 保留参数
    uint16_t sens00;        // 默认传感器采样数值
    uint16_t sens01;        // 默认传感器采样数值
};
struct PosVelTime {         //位置-速度-时间（ty_msg0=3）
    double lonDeg;          // 经度值，双精度浮点数，单位为度
    double latDeg;          // 纬度值，双精度浮点数，单位为度
    double alt;             // 海拔高度值，双精度浮点数，单位为米
    float vG;               // 飞行器运动地面投影速度，单精度浮点数，单位为米每秒
    float crsRad;           // 飞行器运动航迹角，单精度浮点数，单位为度。正北方向为0，向东为正，向西为负，取值范围为±pi
    float vUp;              // 飞行器垂直方向速度，单精度浮点数，单位为米每秒，天向为正方向
    uint32_t gpsMs;         // GPS计时信息，GPS周内秒，四字节无符号整型，单位为毫秒
    uint16_t gpsWeek;       // GPS计时信息，GPS周数，双字节无符号整型，单位为周
    int16_t q1I16;          // 姿态四元数构成变量，双字节有符号整型，分度值为（1/32767），符号条件为q0非负
    int16_t q2I16;          // 姿态四元数构成变量，双字节有符号整型，分度值为（1/32767），符号条件为q0非负
    int16_t q3I16;          // 姿态四元数构成变量，双字节有符号整型，分度值为（1/32767），符号条件为q0非负
};
struct Attitude {           // 2.4姿态信息（ty_msg0=4）
    int16_t q1Int16;        // 姿态四元数构成变量，双字节有符号整型，分度值为（1/32767），符号条件为q0非负
    int16_t q2Int16;        // 姿态四元数构成变量，双字节有符号整型，分度值为（1/32767），符号条件为q0非负
    int16_t q3Int16;        // 姿态四元数构成变量，双字节有符号整型，分度值为（1/32767），符号条件为q0非负
    int16_t p1000x;         // 机体x轴方向的角速度分量，双字节有符号整型，分度值为0.001rad/s
    int16_t q1000x;         // 机体y轴方向的角速度分量，双字节有符号整型，分度值为0.001rad/s
    int16_t r1000x;         // 机体z轴方向的角速度分量，双字节有符号整型，分度值为0.001rad/s
    int16_t ax100x;         // 机体x轴方向的加速度分量，双字节有符号整型，分度值0.01m/ss
    int16_t ay100x;         // 机体y轴方向的加速度分量，双字节有符号整型，分度值0.01m/ss
    int16_t az100x;         // 机体z轴方向的加速度分量，双字节有符号整型，分度值0.01m/ss
};

struct GeneralData {        // 一般数据（ty_msg0=5）

};

struct GpsRawInt {
 int32_t lat; /*< [degE7] Latitude (WGS84, EGM96 ellipsoid)*/
 int32_t lon; /*< [degE7] Longitude (WGS84, EGM96 ellipsoid)*/
 int32_t alt; /*< [mm] Altitude (MSL). Positive for up. Note that virtually all GPS modules provide the MSL altitude in addition to the WGS84 altitude.*/
 uint16_t eph; /*<  GPS HDOP horizontal dilution of position (unitless). If unknown, set to: UINT16_MAX*/
 uint16_t epv; /*<  GPS VDOP vertical dilution of position (unitless). If unknown, set to: UINT16_MAX*/
 uint16_t vel; /*< [cm/s] GPS ground speed. If unknown, set to: UINT16_MAX*/
 int16_t cog; /*< [cdeg] Course over ground (NOT heading, but direction of movement) in degrees * 100, 0.0..359.99 degrees. If unknown, set to: UINT16_MAX*/
 uint8_t fixType; /*<  GPS fix type.*/
 uint8_t doubleAntenna; /*<  双天线状态*/
 uint8_t satellitesUsed; /*<  Number of satellites used. If unknown, set to 255*/
 int32_t altEllipsoid; /*< [mm] Altitude (above WGS84, EGM96 ellipsoid). Positive for up.*/
 int16_t yaw; /*< [cdeg] Yaw in earth frame from north. Use 0 if this GPS does not provide yaw. Use 65535 if this GPS is configured to provide yaw and is currently unable to provide it. Use 36000 for north.*/
};

#pragma pack ()


enum CommandParamType {          // ty_msg0=129时, ty_msg1用以对机上参数进行读取、设置、初始化、保存等操作，名称id_cfggroup
    COMMAND_RESET_PARAM = 0,       // 重置设置（ty_msg0=129,ty_msg1=0）
    COMMAND_LOAD_PARAM,            // 载入设置（ty_msg0=129,ty_msg1=1）
    COMMAND_SAVE_PARAM,            // 保存设置（ty_msg0=129,ty_msg1=1）
    COMMAND_QUERY_PARAM,           // 查询设置（ty_msg0=129,ty_msg1=3）
    COMMAND_SET_PARAM,             // 设置参数（ty_msg0=129,ty_msg1=4）
};

enum AckCommandParamType {       // 设置命令回复（ty_msg0=193），该报文为重置设置命令（ty_msg0=129）的回复报文
    ACK_RESET_PARAM = 0,           // 重置设置返回（ty_msg0=193,ty_msg1=0）
    ACK_LOAD_PARAM,                // 载入设置返回（ty_msg0=193,ty_msg1=1）
    ACK_SAVE_PARAM,                // 保存设置返回（ty_msg0=193,ty_msg1=2）
    ACK_QUERY_PARAM,               // 查询设置（ty_msg0=193,ty_msg1=3）
    ACK_SET_PARAM,                 // 设置参数（ty_msg0=193,ty_msg1=4）
    ACK_ERROR_PARAM = 255          // 错误信息返回（ty_msg0=193,ty_msg1=255）
};

enum CommandBank {              // 航线相关命令（ty_msg0=130）
    QUERY_ALL_BANK = 0,         // 查询整体航路信息（ty_msg0=130,ty_msg1=0）
    QUERY_SINGLE_BANK,          // 查询单个bank信息（ty_msg0=130,ty_msg1=1）
    SET_SINGLE_BANK,            // 设置单个bank（ty_msg0=130,ty_msg1=2）
    REFACTOR_INFO_SLOT,         // 重构infoslot表单（ty_msg0=130,ty_msg1=3）
    QUERY_SINGLE_INFO_SLOT,     // 查询单个infoslot（ty_msg0=130,ty_msg1=4）
    ENABLE_BANK_AUTO_SW,        // 航线自动跳转控制命令（ty_msg0=130,ty_msg1=5）
    WAYPOINT_AUTO_SW,           // 航路点跳转命令（ty_msg0=130,ty_msg1=6）
};

enum AckCommandBank {           // 航线相关命令（ty_msg0=194），该报文为查询整体航路信息命令（ty_msg0=130）的回复报文
    ACK_NONE = -1,              // 未请求
    ACK_QUERY_ALL = 0,          // 查询整体航路信息（ty_msg0=194,ty_msg1=0）
    ACK_QUERY_SINGLE_BANK,      // 查询单个bank信息（ty_msg0=194,ty_msg1=1）
    ACK_SET_SINGLE_BANK,        // 设置单个bank（ty_msg0=194,ty_msg1=2）
    ACK_REFACTOR_INFO_SLOT,     // 重构infoslot表单（ty_msg0=194,ty_msg1=3）
    ACK_QUERY_SINGLE_INFO_SLOT, // 查询单个infoslot（ty_msg0=194,ty_msg1=4）
    ACK_BANK_AUTO_SW,           // 航线自动跳转控制命令（ty_msg0=194,ty_msg1=5）
    ACK_WAYPOINT_AUTO_SW,       // 航路点跳转命令（ty_msg0=194,ty_msg1=6）
    ACK_BANK_ERROR = 255
};


enum FlyMode {
    RC_ANGULAR_VEL              = 0,        // 0：遥控器角速度控制
    RC_ANGLE                    = 1,        // 1：遥控器姿态角控制
    RADIO_ANGULAR_VEL           = 2,        // 2：数传指令角速度控制
    RADIO_ANGLE                 = 3,        // 3：数传指令姿态角控制
    ALTITUDE_RC_ANGLE           = 64,       // 64：定高+遥控器姿态控制
    ALTITUDE_RADIO_ANGLE        = 65,       // 65：定高+数传指令姿态控制
    ALTITUDE_RC_SPEED_HEAD      = 66,       // 66：定高+遥控器速度控制/航向控制
    ALTITUDE_RADIO_SPEED_HEAD   = 67,       // 67：定高+数传指令速度控制/航向控制
    PROGRAM_CONTROL             = 192       // 192：程控飞行
};


struct Bit2Name {
    FlyMode     flightMode;
    const char* name;
};

/******************** 通常报文编号 ********************/
enum MsgId {
    WAYPOINT_INFO_SLOT = 1,       // 航路点信息
    GENERAL_STATUS = 2,           // 常规状态信息
    POS_VEL_TIME = 3,             // 位置-速度-时间
    ATTITUDE = 4,                 // 姿态信息
    GENERAL_DATA = 5,             // 一般数据

    /******************** 命令报文编号 ********************/
    COMMAND_VEHICLE = 128,        // 载具操纵命令
    COMMAND_PARAM = 129,            // 设置相关命令
    COMMAND_BANK = 130,           // 航路点命令
    COMMAND_PAYLOAD = 131,        // 载荷命令
    COMMAND_REMOTE_CONTROL = 132, // 遥控命令
    COMMAND_COMM = 133,           // 通讯命令
    COMMAND_ORGANIZE = 134,       // 组织命令

    /******************** 回复报文编号 ********************/
    ACK_COMMAND_VEHICLE = 192,    // 载具操纵命令回复
    ACK_COMMAND_PARAM = 193,      // 设置参数相关命令回复
    ACK_COMMAND_BANK = 194,       // 航路点命令回复
    ACK_COMMAND_PAYLOAD = 195,    // 载荷命令回复
    ACK_COMMAND_REMOTE_CONTROL = 196,    // 遥控命令回复
    ACK_COMMAND_COMM = 197,       // 通讯命令回复
    ACK_COMMAND_ORGANIZE = 198   // 组织命令回复
};


/******************** 命令报文编号 ********************/
//enum CommandType {
//    COMMAND_VEHICLE = 128,        // 载具操纵命令
//    COMMAND_PARAM = 129,            // 设置相关命令
//    COMMAND_BANK = 130,           // 航路点命令
//    COMMAND_PAYLOAD = 131,        // 载荷命令
//    COMMAND_REMOTE_CONTROL = 132, // 遥控命令
//    COMMAND_COMM = 133,           // 通讯命令
//    COMMAND_ORGANIZE = 134,       // 组织命令
//};

struct CommandInfo {
    uint8_t tyMsg0;
    uint8_t tyMsg1;
    QString commandName;
};
