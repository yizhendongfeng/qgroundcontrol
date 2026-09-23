#include "qgcplanparser.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <cmath>

#include "mavlinkdefs.h"

namespace wpt {

namespace {

// QGC 中 SimpleItem 的 "type" 标识
constexpr auto kSimpleItem = "SimpleItem";
constexpr auto kComplexItem = "ComplexItem";

double paramAt(const QJsonArray& params, int idx, double def = 0.0)
{
    if (idx < 0 || idx >= params.size())
        return def;
    const QJsonValue v = params.at(idx);
    if (v.isNull())
        return def;
    if (v.isDouble())
        return v.toDouble();
    if (v.isString()) {
        bool ok = false;
        const double d = v.toString().toDouble(&ok);
        return ok ? d : def;
    }
    return def;
}

// 将 QGC 高度帧转换为 DJI 执行高度模式；返回是否成功。
bool frameToHeightMode(int frame, QString& heightMode, Report& report)
{
    switch (frame) {
    case mav::FRAME_GLOBAL:
        heightMode = QStringLiteral("WGS84");
        return true;
    case mav::FRAME_GLOBAL_RELATIVE_ALT:
        heightMode = QStringLiteral("relativeToStartPoint");
        return true;
    case mav::FRAME_GLOBAL_TERRAIN_ALT:
        report.warn(QStringLiteral("MAV_FRAME_GLOBAL_TERRAIN_ALT(10) 相对地形高度无法精确映射，按相对起飞点高度处理"));
        heightMode = QStringLiteral("relativeToStartPoint");
        return true;
    default:
        report.warn(QStringLiteral("不支持的 MAV_FRAME=%1，按相对起飞点高度处理").arg(frame));
        heightMode = QStringLiteral("relativeToStartPoint");
        return true;
    }
}

} // namespace

bool QgcPlanParser::parseFile(const QString& planPath, Plan& out, Report& report)
{
    QFile f(planPath);
    if (!f.open(QIODevice::ReadOnly)) {
        report.error(QStringLiteral("无法打开 plan 文件: %1").arg(planPath));
        return false;
    }
    return parseJson(f.readAll(), out, report);
}

bool QgcPlanParser::parseJson(const QByteArray& json, Plan& out, Report& report)
{
    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseErr);
    if (parseErr.error != QJsonParseError::NoError) {
        report.error(QStringLiteral("plan JSON 解析失败: %1").arg(parseErr.errorString()));
        return false;
    }
    if (!doc.isObject()) {
        report.error(QStringLiteral("plan 文件顶层必须是 JSON 对象"));
        return false;
    }

    const QJsonObject root = doc.object();
    const QString fileType = root.value(QStringLiteral("fileType")).toString();
    if (fileType != QStringLiteral("Plan")) {
        report.error(QStringLiteral("不是有效的 QGC plan 文件 (fileType=\"%1\")").arg(fileType));
        return false;
    }

    out = Plan{};
    out.source = QStringLiteral("QGroundControl");
    out.author = root.value(QStringLiteral("groundStation")).toString(QStringLiteral("QGroundControl"));

    const QJsonObject missionObj = root.value(QStringLiteral("mission")).toObject();
    if (missionObj.isEmpty()) {
        report.error(QStringLiteral("plan 缺少 mission 对象"));
        return false;
    }

    out.cruiseSpeed = missionObj.value(QStringLiteral("cruiseSpeed")).toDouble(5.0);
    out.hoverSpeed  = missionObj.value(QStringLiteral("hoverSpeed")).toDouble(5.0);
    out.vehicleType = missionObj.value(QStringLiteral("vehicleType")).toInt(2);
    out.firmwareType = missionObj.value(QStringLiteral("firmwareType")).toInt(12);
    out.mission.finishAction = QStringLiteral("goHome"); // 默认；遇到 RTL/LAND 覆盖

    const QJsonArray homeArr = missionObj.value(QStringLiteral("plannedHomePosition")).toArray();
    if (homeArr.size() >= 3 && homeArr.at(0).isDouble() && homeArr.at(1).isDouble()) {
        out.hasPlannedHome = true;
        out.homeLat = homeArr.at(0).toDouble();
        out.homeLon = homeArr.at(1).toDouble();
        out.homeAltAmsl = homeArr.at(2).toDouble(0.0);
    }

    // 全局巡航/悬停速度作为 DJI 全局航线速度默认值
    Wayline& line = out.firstWayline();
    line.autoFlightSpeed = out.hoverSpeed > 0.0 ? out.hoverSpeed
                                                : (out.cruiseSpeed > 0.0 ? out.cruiseSpeed : 5.0);
    out.mission.globalTransitionalSpeed = line.autoFlightSpeed;

    // 解析 items：DO_ 状态命令与航点命令混排，保持顺序
    const QJsonArray items = missionObj.value(QStringLiteral("items")).toArray();

    // 待应用状态（QGC 语义：DO_ 命令影响后续航点直到被覆盖）
    bool pendingLocalSpeed = false;
    double pendingSpeed = 0.0;
    double pendingPhotoDist = -1.0;
    double pendingPhotoInterval = -1.0;
    bool pendingPoi = false;
    double pendingPoiLat = 0.0, pendingPoiLon = 0.0, pendingPoiAlt = 0.0;
    bool pendingGimbalPitch = false;
    double pendingGimbalPitchAngle = 0.0;

    int itemIndex = 0;
    // 将待应用状态（速度/拍照/POI/云台）复制到航点
    const auto applyPending = [&](Waypoint& wp) {
        wp.hasLocalSpeed = pendingLocalSpeed;
        wp.speed = pendingSpeed;
        wp.photoDistanceInterval = pendingPhotoDist;
        wp.photoTimeInterval = pendingPhotoInterval;
        wp.hasPoi = pendingPoi;
        wp.poiLat = pendingPoiLat;
        wp.poiLon = pendingPoiLon;
        wp.poiAlt = pendingPoiAlt;
        if (pendingPoi)
            wp.headingMode = Waypoint::HeadingMode::TowardPoi;
        wp.hasGimbalPitch = pendingGimbalPitch;
        wp.gimbalPitchAngle = pendingGimbalPitchAngle;
    };
    for (const QJsonValue& itemVal : items) {
        ++itemIndex;
        if (!itemVal.isObject())
            continue;
        const QJsonObject item = itemVal.toObject();
        const QString type = item.value(QStringLiteral("type")).toString();

        if (type == kComplexItem) {
            const QString complexType = item.value(QStringLiteral("complexItemType")).toString();
            report.warn(QStringLiteral("第 %1 项为复杂任务项(%2)，暂不支持展开，已跳过。若需要建图/倾斜摄影模板转换请另行处理")
                            .arg(itemIndex).arg(complexType));
            continue;
        }
        if (type != kSimpleItem) {
            report.warn(QStringLiteral("第 %1 项 type=\"%2\" 未知，已跳过").arg(itemIndex).arg(type));
            continue;
        }

        const int command = item.value(QStringLiteral("command")).toInt();
        const int frame = item.value(QStringLiteral("frame")).toInt(mav::FRAME_GLOBAL_RELATIVE_ALT);
        const QJsonArray params = item.value(QStringLiteral("params")).toArray();

        switch (command) {
        case mav::CMD_NAV_WAYPOINT:
        case mav::CMD_NAV_SPLINE_WAYPOINT: {
            Waypoint wp;
            wp.lat = paramAt(params, 4);
            wp.lon = paramAt(params, 5);
            wp.executeHeight = paramAt(params, 6);
            if (!frameToHeightMode(frame, line.executeHeightMode, report))
                return false;

            // 偏航：params[3] 仅手动设置时才有数值（QGC 通常为 null）
            const QJsonValue yawVal = params.size() > 3 ? params.at(3) : QJsonValue();
            if (yawVal.isDouble())
                wp.headingMode = Waypoint::HeadingMode::SmoothTransition;
            else
                wp.headingMode = Waypoint::HeadingMode::FollowWayline;
            if (yawVal.isDouble())
                wp.headingAngle = yawVal.toDouble();

            // 高度模式影响 template 高度语义
            if (line.executeHeightMode == QStringLiteral("relativeToStartPoint")) {
                out.templateHeightMode = QStringLiteral("relativeToStartPoint");
                wp.height = wp.executeHeight;
                wp.ellipsoidHeight = wp.executeHeight; // 相对模式两者相同（DJI 规范）
                wp.useGlobalHeight = false;
            } else {
                out.templateHeightMode = QStringLiteral("EGM96");
                wp.ellipsoidHeight = wp.executeHeight; // WGS84 椭球高
                wp.height = wp.executeHeight;          // 无大地水准面模型时近似
                wp.useGlobalHeight = false;
            }

            // 待应用状态
            applyPending(wp);

            // 转弯模式：spline 用曲线过点（近似），普通航点用默认
            if (command == mav::CMD_NAV_SPLINE_WAYPOINT)
                wp.turnMode = Waypoint::TurnMode::ToPointAndPassContinuity;

            line.waypoints.append(wp);
            break;
        }
        case mav::CMD_NAV_LOITER_UNLIM:
        case mav::CMD_NAV_LOITER_TURNS:
        case mav::CMD_NAV_LOITER_TIME: {
            Waypoint wp;
            wp.lat = paramAt(params, 4);
            wp.lon = paramAt(params, 5);
            wp.executeHeight = paramAt(params, 6);
            frameToHeightMode(frame, line.executeHeightMode, report);

            // 偏航：同 NAV_WAYPOINT，仅手动设置时 params[3] 有数值
            const QJsonValue yawVal = params.size() > 3 ? params.at(3) : QJsonValue();
            if (yawVal.isDouble()) {
                wp.headingMode = Waypoint::HeadingMode::SmoothTransition;
                wp.headingAngle = yawVal.toDouble();
            }

            applyPending(wp);

            if (command == mav::CMD_NAV_LOITER_TIME && paramAt(params, 0) > 0.0) {
                DjiAction act;
                act.func = DjiAction::Func::Hover;
                act.hoverTime = paramAt(params, 0);
                wp.actions.append(act);
            } else {
                report.warn(QStringLiteral("LOITER 命令转换为悬停航点（无限盘旋无法在 DJI 航点中表达）"));
            }
            line.waypoints.append(wp);
            break;
        }
        case mav::CMD_NAV_RETURN_TO_LAUNCH:
            out.mission.finishAction = QStringLiteral("goHome");
            break;
        case mav::CMD_NAV_LAND:
            out.mission.finishAction = QStringLiteral("autoLand");
            report.warn(QStringLiteral("检测到 NAV_LAND，DJI 侧以 finishAction=autoLand 表达（降落点由飞机自主决定）"));
            break;
        case mav::CMD_NAV_TAKEOFF:
        case mav::CMD_NAV_VTOL_TAKEOFF:
            report.warn(QStringLiteral("NAV_TAKEOFF 由 DJI 起飞流程接管，已忽略"));
            break;
        case mav::CMD_DO_CHANGE_SPEED: {
            // p1: 0=空速 1=地速；p2: 速度 m/s
            const double spd = paramAt(params, 1);
            if (spd > 0.0) {
                pendingLocalSpeed = true;
                pendingSpeed = spd;
            } else {
                pendingLocalSpeed = false;
            }
            break;
        }
        case mav::CMD_DO_SET_CAM_TRIGG_DIST:
            pendingPhotoDist = paramAt(params, 0);
            pendingPhotoInterval = -1.0;
            break;
        case mav::CMD_DO_SET_CAM_TRIGG_INTERVAL:
            pendingPhotoInterval = paramAt(params, 1);
            pendingPhotoDist = -1.0;
            break;
        case mav::CMD_DO_DIGICAM_CONTROL:
        case mav::CMD_IMAGE_START_CAPTURE: {
            // 单次拍照：附加到后续航点
            if (!line.waypoints.isEmpty()) {
                DjiAction act;
                act.func = DjiAction::Func::TakePhoto;
                act.fileSuffix = QStringLiteral("wp%1").arg(line.waypoints.size());
                line.waypoints.last().actions.append(act);
            } else {
                report.warn(QStringLiteral("拍照命令出现在任何航点之前，已忽略"));
            }
            break;
        }
        case mav::CMD_DO_MOUNT_CONTROL: {
            // p1 = pitch (deg, 前倾为负)
            const double pitch = paramAt(params, 0);
            if (!std::isnan(pitch)) {
                pendingGimbalPitch = true;
                pendingGimbalPitchAngle = pitch;
            }
            break;
        }
        case mav::CMD_DO_SET_ROI_LOCATION:
            pendingPoi = true;
            pendingPoiLat = paramAt(params, 4);
            pendingPoiLon = paramAt(params, 5);
            pendingPoiAlt = paramAt(params, 6);
            break;
        case mav::CMD_DO_SET_ROI_NONE:
            pendingPoi = false;
            break;
        default:
            report.warn(QStringLiteral("第 %1 项命令 %2 (%3) 不支持转换为 DJI 航点动作，已跳过")
                            .arg(itemIndex).arg(command).arg(mav::commandName(command)));
            break;
        }
    }

    if (line.waypoints.isEmpty()) {
        report.error(QStringLiteral("plan 中没有可转换的导航航点"));
        return false;
    }

    // 由航点高度推导全局高度（template 编辑用）
    out.globalHeight = line.waypoints.first().height;
    out.globalEllipsoidHeight = line.waypoints.first().ellipsoidHeight;
    if (line.executeHeightMode == QStringLiteral("WGS84"))
        out.templateHeightMode = QStringLiteral("EGM96");

    // 默认速度（若所有航点都无局部速度）
    if (!line.waypoints.first().hasLocalSpeed)
        line.autoFlightSpeed = out.hoverSpeed > 0.0 ? out.hoverSpeed : 5.0;

    return true;
}

} // namespace wpt
