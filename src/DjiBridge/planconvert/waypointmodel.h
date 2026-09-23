#pragma once

#include <QString>
#include <QVector>

// ---------------------------------------------------------------------------
// waypointmodel.h
// QGC plan 与 DJI WPML 之间的统一中间数据模型。
// 两种格式解析后都落到本模型，转换 = 模型 <-> 各格式的序列化/反序列化。
// ---------------------------------------------------------------------------

namespace wpt {

// ---------- DJI 航点动作（actionGroup 中的 action） ----------
struct DjiAction {
    enum class Func {
        TakePhoto,      // 单拍
        StartRecord,    // 开始录像
        StopRecord,     // 结束录像
        Hover,          // 悬停等待
        GimbalRotate,   // 旋转云台（reachPoint 触发）
        Zoom,           // 变焦
        CustomDirName,  // 创建文件夹
        Unknown
    };

    Func func = Func::Unknown;
    // takePhoto
    QString fileSuffix = QStringLiteral("point");
    // hover
    double hoverTime = 0.0;
    // gimbalRotate
    double gimbalPitchAngle = 0.0;
    double gimbalYawAngle = 0.0;
    double gimbalRotateTime = 0.0;
    // zoom
    double focalLength = 0.0;

    static QString funcName(Func f);
};

// ---------- 航点 ----------
struct Waypoint {
    // 航点偏航角模式（DJI waypointHeadingMode / QGC yaw 参数）
    enum class HeadingMode {
        FollowWayline,      // 沿航线方向
        Manually,           // 手动控制
        Fixed,              // 锁定当前偏航角
        SmoothTransition,   // 自定义偏航角，航段内均匀过渡
        TowardPoi           // 朝向兴趣点
    };

    // 航点转弯模式（DJI waypointTurnMode）
    enum class TurnMode {
        CoordinateTurn,                         // 协调转弯，提前转弯
        ToPointAndStopDiscontinuity,            // 直线飞行，到点停
        ToPointAndStopContinuity,               // 曲线飞行，到点停
        ToPointAndPassContinuity                // 曲线飞行，过点不停
    };

    double lat = 0.0;                    // 纬度 [-90,90]
    double lon = 0.0;                    // 经度 [-180,180]
    int index = 0;                       // DJI 航点序号（解析用）
    double executeHeight = 0.0;          // waylines.wpml: 航点执行高度（按 executeHeightMode 参考系）
    double ellipsoidHeight = 0.0;        // template.kml: WGS84 椭球高
    double height = 0.0;                 // template.kml: 编辑高度（EGM96 海拔/相对起飞点/AGL）
    bool useGlobalHeight = true;         // template.kml: 是否使用全局高度

    bool hasLocalSpeed = false;          // 是否覆盖全局速度
    double speed = 0.0;                  // 航点飞行速度 m/s（飞向下一点）

    HeadingMode headingMode = HeadingMode::FollowWayline;
    double headingAngle = 0.0;           // smoothTransition 时目标偏航角 [-180,180]
    // template.kml: 是否使用全局偏航参数。1=跟随全局（航点级 waypointHeadingParam 不生效），
    // 0=使用航点级偏航设置。对应 QGC plan params[3] 为 null（跟随）或有偏航角（手动）。
    bool useGlobalHeadingParam = true;
    bool hasPoi = false;                 // towardPoi 时有效
    double poiLat = 0.0, poiLon = 0.0, poiAlt = 0.0;

    TurnMode turnMode = TurnMode::ToPointAndStopDiscontinuity;
    double turnDampingDist = 0.0;        // 转弯截距 m

    bool hasGimbalPitch = false;         // template.kml: 航点云台俯仰角
    double gimbalPitchAngle = 0.0;

    // 等间隔拍照（DO_SET_CAM_TRIGG_DIST -> multipleDistance+takePhoto）
    double photoDistanceInterval = -1.0; // >0 有效，单位 m
    // 等时拍照（DO_SET_CAM_TRIGG_INTERVAL -> multipleTiming+takePhoto）
    double photoTimeInterval = -1.0;     // >0 有效，单位 s

    QVector<DjiAction> actions;          // 该航点处触发的动作（reachPoint）
};

// ---------- 一条航线（waylines.wpml 中一个 Folder） ----------
struct Wayline {
    int templateId = 0;                  // [0,65535]
    int waylineId = 0;                   // [0,65535]
    double autoFlightSpeed = 5.0;        // 全局航线飞行速度 m/s [1,15]
    QString executeHeightMode = QStringLiteral("relativeToStartPoint"); // WGS84 | relativeToStartPoint
    QVector<Waypoint> waypoints;
};

// ---------- missionConfig ----------
struct MissionConfig {
    QString flyToWaylineMode = QStringLiteral("safely");       // safely | pointToPoint
    QString finishAction = QStringLiteral("goHome");           // goHome | noAction | autoLand | gotoFirstWaypoint
    QString exitOnRCLost = QStringLiteral("goContinue");       // goContinue | executeLostAction
    QString executeRCLostAction = QStringLiteral("hover");     // goBack | landing | hover
    double takeOffSecurityHeight = 20.0;                       // 安全起飞高度 m
    double globalTransitionalSpeed = 8.0;                      // 全局过渡速度 m/s
    double globalRTHHeight = 50.0;                             // 全局返航高度 m
    QString takeOffRefPoint;                                   // "纬度,经度,椭球高"
    double takeOffRefPointAGLHeight = 0.0;                     // 参考起飞点海拔高
    int droneEnumValue = 67;                                   // 机型主类型（默认 M30）
    int droneSubEnumValue = 0;
    int payloadEnumValue = 52;                                 // 负载主类型（默认 M30 双光）
    int payloadPositionIndex = 0;
};

// ---------- 完整航线文档（中间表示） ----------
struct Plan {
    QString source;                        // 来源标记："QGroundControl" / "DJI WPML"
    QString author = QStringLiteral("DjiQgcPlanFileConvert");
    qint64 createTimeMs = 0;
    qint64 updateTimeMs = 0;

    MissionConfig mission;

    // template.kml 全局参数
    QString templateHeightMode = QStringLiteral("relativeToStartPoint"); // EGM96 | relativeToStartPoint | aboveGroundLevel
    double globalHeight = 0.0;             // 全局航线高度（编辑高度）
    double globalEllipsoidHeight = 0.0;    // 全局航线高度（椭球高）
    double globalShootHeight = 50.0;       // 飞行器离被摄面高度（mapping 模板用）
    int surfaceFollowModeEnable = 0;       // 是否仿地飞行
    double surfaceRelativeHeight = 0.0;
    QString gimbalPitchMode = QStringLiteral("usePointSetting"); // manual | usePointSetting

    Waypoint::HeadingMode globalHeadingMode = Waypoint::HeadingMode::FollowWayline;
    double globalHeadingAngle = 0.0;
    Waypoint::TurnMode globalTurnMode = Waypoint::TurnMode::ToPointAndStopDiscontinuity;
    int globalUseStraightLine = 0;

    QVector<Wayline> waylines;

    // QGC 特有字段（写回 QGC plan 时使用）
    double cruiseSpeed = 5.0;              // 固定翼巡航速度
    double hoverSpeed = 5.0;               // 多旋翼速度
    bool hasPlannedHome = false;
    double homeLat = 0.0, homeLon = 0.0, homeAltAmsl = 0.0;
    int vehicleType = 2;                   // MAV_TYPE_QUADROTOR
    int firmwareType = 12;                 // MAV_AUTOPILOT_PX4

    // 便捷访问：第一条航线（绝大多数情况只有一条）
    Wayline& firstWayline();
    const Wayline& firstWayline() const;
};

// ---------- 转换报告 ----------
struct Report {
    QVector<QString> warnings;
    QVector<QString> errors;
    int convertedWaypoints = 0;          // 本次转换的航点数（成功时填充）

    void warn(const QString& msg) { warnings.append(msg); }
    void error(const QString& msg) { errors.append(msg); }
    bool ok() const { return errors.isEmpty(); }

    QString toString() const;
};

} // namespace wpt
