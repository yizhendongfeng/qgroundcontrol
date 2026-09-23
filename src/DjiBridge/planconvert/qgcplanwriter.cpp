#include "qgcplanwriter.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <cmath>

#include "mavlinkdefs.h"

namespace wpt {

namespace {

QJsonObject makeSimpleItem(int command, int frame, int doJumpId,
                           double p1, double p2, double p3, const QJsonValue& p4,
                           double lat, double lon, double z)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("AMSLAltAboveTerrain"), QJsonValue(QJsonValue::Null));
    obj.insert(QStringLiteral("Altitude"), z);
    obj.insert(QStringLiteral("AltitudeMode"),
               frame == mav::FRAME_GLOBAL ? 2 : 1); // QGC AltMode: 2=Absolute(AMSL) 1=Relative
    obj.insert(QStringLiteral("autoContinue"), true);
    obj.insert(QStringLiteral("command"), command);
    obj.insert(QStringLiteral("doJumpId"), doJumpId);
    obj.insert(QStringLiteral("frame"), frame);
    QJsonArray params;
    auto addParam = [&params](double v) { params.append(v); };
    addParam(p1); addParam(p2); addParam(p3);
    // params[3]（偏航）：航点手动偏航时为角度，否则为真正的 null
    params.append(p4);
    params.append(lat);
    params.append(lon);
    params.append(z);
    obj.insert(QStringLiteral("params"), params);
    obj.insert(QStringLiteral("type"), QStringLiteral("SimpleItem"));
    return obj;
}

int heightFrameFor(const QString& executeHeightMode, Report& report)
{
    if (executeHeightMode == QStringLiteral("WGS84"))
        return mav::FRAME_GLOBAL;
    if (executeHeightMode == QStringLiteral("relativeToStartPoint"))
        return mav::FRAME_GLOBAL_RELATIVE_ALT;
    report.warn(QStringLiteral("未知执行高度模式 %1，按相对起飞点高度输出").arg(executeHeightMode));
    return mav::FRAME_GLOBAL_RELATIVE_ALT;
}

} // namespace

QByteArray QgcPlanWriter::toJson(const Plan& plan, Report& report)
{
    const Wayline& line = plan.firstWayline();
    const int frame = heightFrameFor(line.executeHeightMode, report);

    QJsonArray items;
    int doJumpId = 0;

    // 记录上一个航点的速度/拍照状态，用于插入 DO_ 命令
    double lastSpeed = -1.0;
    double lastPhotoDist = -1.0;
    double lastPhotoInterval = -1.0;

    const auto pushDoChangeSpeed = [&items, &doJumpId](double speed) {
        items.append(makeSimpleItem(mav::CMD_DO_CHANGE_SPEED, mav::FRAME_GLOBAL_RELATIVE_ALT,
                                    ++doJumpId, 1.0, speed, -1.0, 0.0, 0.0, 0.0, 0.0));
    };
    const auto pushDoCamDist = [&items, &doJumpId](double dist) {
        items.append(makeSimpleItem(mav::CMD_DO_SET_CAM_TRIGG_DIST, mav::FRAME_GLOBAL_RELATIVE_ALT,
                                    ++doJumpId, dist, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
    };
    const auto pushDoCamInterval = [&items, &doJumpId](double sec) {
        items.append(makeSimpleItem(mav::CMD_DO_SET_CAM_TRIGG_INTERVAL, mav::FRAME_GLOBAL_RELATIVE_ALT,
                                    ++doJumpId, 1.0, sec, 0.0, 0.0, 0.0, 0.0, 0.0));
    };
    const auto pushDoMount = [&items, &doJumpId](double pitch) {
        items.append(makeSimpleItem(mav::CMD_DO_MOUNT_CONTROL, mav::FRAME_GLOBAL_RELATIVE_ALT,
                                    ++doJumpId, pitch, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
    };
    const auto pushDoRoi = [&items, &doJumpId](double lat, double lon, double alt) {
        items.append(makeSimpleItem(mav::CMD_DO_SET_ROI_LOCATION, mav::FRAME_GLOBAL_RELATIVE_ALT,
                                    ++doJumpId, 0.0, 0.0, 0.0, 0.0, lat, lon, alt));
    };
    const auto pushImageStartCapture = [&items, &doJumpId]() {
        items.append(makeSimpleItem(mav::CMD_IMAGE_START_CAPTURE, mav::FRAME_GLOBAL_RELATIVE_ALT,
                                    ++doJumpId, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0));
    };
    const auto pushVideoStart = [&items, &doJumpId]() {
        items.append(makeSimpleItem(mav::CMD_VIDEO_START_CAPTURE, mav::FRAME_GLOBAL_RELATIVE_ALT,
                                    ++doJumpId, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
    };
    const auto pushVideoStop = [&items, &doJumpId]() {
        items.append(makeSimpleItem(mav::CMD_VIDEO_STOP_CAPTURE, mav::FRAME_GLOBAL_RELATIVE_ALT,
                                    ++doJumpId, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
    };

    int wpSeq = 0;
    for (const Waypoint& wp : line.waypoints) {
        // 1) 速度变化 -> DO_CHANGE_SPEED
        double wpSpeed = wp.hasLocalSpeed ? wp.speed : line.autoFlightSpeed;
        if (lastSpeed < 0.0) {
            lastSpeed = wpSpeed;
        } else if (qAbs(lastSpeed - wpSpeed) > 0.001) {
            pushDoChangeSpeed(wpSpeed);
            lastSpeed = wpSpeed;
        }

        // 2) 拍照模式变化 -> DO_SET_CAM_TRIGG_DIST / INTERVAL
        if (wp.photoDistanceInterval > 0.0 && qAbs(lastPhotoDist - wp.photoDistanceInterval) > 0.001) {
            pushDoCamDist(wp.photoDistanceInterval);
            lastPhotoDist = wp.photoDistanceInterval;
            lastPhotoInterval = -1.0;
        }
        if (wp.photoTimeInterval > 0.0 && qAbs(lastPhotoInterval - wp.photoTimeInterval) > 0.001) {
            pushDoCamInterval(wp.photoTimeInterval);
            lastPhotoInterval = wp.photoTimeInterval;
            lastPhotoDist = -1.0;
        }

        // 3) POI
        if (wp.hasPoi)
            pushDoRoi(wp.poiLat, wp.poiLon, wp.poiAlt);

        // 4) 云台俯仰
        if (wp.hasGimbalPitch)
            pushDoMount(wp.gimbalPitchAngle);

        // 5) 航点本体。hover 动作 -> NAV_LOITER_TIME
        bool hoverAsLoiter = false;
        double hoverTime = 0.0;
        for (const DjiAction& act : wp.actions) {
            if (act.func == DjiAction::Func::Hover && act.hoverTime > 0.0) {
                hoverAsLoiter = true;
                hoverTime = act.hoverTime;
            }
        }

        // params[3]（偏航）：仅手动设置偏航时有角度，否则为真正的 null
        const QJsonValue yaw =
            (wp.headingMode == Waypoint::HeadingMode::Manually
             || wp.headingMode == Waypoint::HeadingMode::Fixed
             || wp.headingMode == Waypoint::HeadingMode::SmoothTransition)
                ? QJsonValue(wp.headingAngle)
                : QJsonValue(QJsonValue::Null);

        if (hoverAsLoiter) {
            items.append(makeSimpleItem(mav::CMD_NAV_LOITER_TIME, frame, ++doJumpId,
                                        hoverTime, 0.0, 0.0, yaw, wp.lat, wp.lon, wp.executeHeight));
        } else {
            const int cmd = (wp.turnMode == Waypoint::TurnMode::ToPointAndPassContinuity)
                                ? mav::CMD_NAV_SPLINE_WAYPOINT
                                : mav::CMD_NAV_WAYPOINT;
            items.append(makeSimpleItem(cmd, frame, ++doJumpId,
                                        0.0, 0.0, 0.0, yaw, wp.lat, wp.lon, wp.executeHeight));
        }

        // 6) 航点动作（reachPoint 触发）
        for (const DjiAction& act : wp.actions) {
            switch (act.func) {
            case DjiAction::Func::TakePhoto:
                pushImageStartCapture();
                break;
            case DjiAction::Func::StartRecord:
                pushVideoStart();
                break;
            case DjiAction::Func::StopRecord:
                pushVideoStop();
                break;
            case DjiAction::Func::Hover:
                break; // 已在航点本体表达
            default:
                report.warn(QStringLiteral("动作 %1 无法在 QGC plan 中表达，已忽略")
                                .arg(DjiAction::funcName(act.func)));
                break;
            }
        }
        ++wpSeq;
    }

    // 7) 结束动作
    if (plan.mission.finishAction == QStringLiteral("autoLand")) {
        items.append(makeSimpleItem(mav::CMD_NAV_LAND, frame, ++doJumpId,
                                    0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
    } else if (plan.mission.finishAction == QStringLiteral("goHome")) {
        items.append(makeSimpleItem(mav::CMD_NAV_RETURN_TO_LAUNCH, frame, ++doJumpId,
                                    0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0));
    }

    if (items.isEmpty()) {
        report.error(QStringLiteral("没有可输出的航点"));
        return {};
    }

    // ---- 组装顶层对象 ----
    QJsonObject mission;
    mission.insert(QStringLiteral("version"), 2);
    mission.insert(QStringLiteral("firmwareType"), plan.firmwareType);
    mission.insert(QStringLiteral("globalPlanAltitudeMode"),
                   frame == mav::FRAME_GLOBAL ? 2 : 1);
    mission.insert(QStringLiteral("hoverSpeed"), plan.hoverSpeed > 0.0 ? plan.hoverSpeed : line.autoFlightSpeed);
    mission.insert(QStringLiteral("cruiseSpeed"), plan.cruiseSpeed > 0.0 ? plan.cruiseSpeed : line.autoFlightSpeed);
    mission.insert(QStringLiteral("vehicleType"), plan.vehicleType);
    mission.insert(QStringLiteral("items"), items);

    if (plan.hasPlannedHome) {
        QJsonArray home;
        home.append(plan.homeLat);
        home.append(plan.homeLon);
        home.append(plan.homeAltAmsl);
        mission.insert(QStringLiteral("plannedHomePosition"), home);
    } else if (!line.waypoints.isEmpty()) {
        QJsonArray home;
        home.append(line.waypoints.first().lat);
        home.append(line.waypoints.first().lon);
        home.append(0.0);
        mission.insert(QStringLiteral("plannedHomePosition"), home);
    }

    QJsonObject root;
    root.insert(QStringLiteral("fileType"), QStringLiteral("Plan"));
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("groundStation"), QStringLiteral("QGroundControl (converted from DJI WPML)"));
    root.insert(QStringLiteral("mission"), mission);
    // 空的地理围栏与返航点（DJI 航线无对应概念）
    QJsonObject geoFence;
    geoFence.insert(QStringLiteral("version"), 2);
    geoFence.insert(QStringLiteral("circles"), QJsonArray());
    geoFence.insert(QStringLiteral("polygons"), QJsonArray());
    root.insert(QStringLiteral("geoFence"), geoFence);
    QJsonObject rally;
    rally.insert(QStringLiteral("version"), 2);
    rally.insert(QStringLiteral("points"), QJsonArray());
    root.insert(QStringLiteral("rallyPoints"), rally);

    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

bool QgcPlanWriter::writeFile(const Plan& plan, const QString& planPath, Report& report)
{
    const QByteArray json = toJson(plan, report);
    if (json.isEmpty())
        return false;
    QFile f(planPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        report.error(QStringLiteral("无法写入 plan 文件: %1").arg(planPath));
        return false;
    }
    f.write(json);
    f.close();
    return true;
}

} // namespace wpt
