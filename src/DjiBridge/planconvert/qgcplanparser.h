#pragma once

#include <QString>
#include "waypointmodel.h"

// ---------------------------------------------------------------------------
// qgcplanparser.h
// 解析 QGroundControl plan 文件（JSON）到统一中间模型 wpt::Plan。
//
// 支持的 SimpleItem 命令：
//   NAV_WAYPOINT / NAV_SPLINE_WAYPOINT / NAV_LOITER_*  -> 航点
//   NAV_RETURN_TO_LAUNCH / NAV_LAND                    -> finishAction
//   DO_CHANGE_SPEED        -> 后续航点速度
//   DO_SET_CAM_TRIGG_DIST  -> 后续航点等距拍照
//   DO_SET_CAM_TRIGG_INTERVAL -> 后续航点等时拍照
//   DO_DIGICAM_CONTROL / IMAGE_START_CAPTURE -> 航点单拍动作
//   DO_MOUNT_CONTROL       -> 后续航点云台俯仰角
//   DO_SET_ROI_LOCATION    -> 后续航点朝向兴趣点
// 其余命令跳过并记警告。
// ---------------------------------------------------------------------------

namespace wpt {

class QgcPlanParser
{
public:
    // 从 plan 文件解析；失败返回 false 并通过 report 输出原因。
    bool parseFile(const QString& planPath, Plan& out, Report& report);
    // 从 JSON 文本解析。
    bool parseJson(const QByteArray& json, Plan& out, Report& report);
};

} // namespace wpt
