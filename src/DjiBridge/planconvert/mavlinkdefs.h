#pragma once

#include <QString>

// ---------------------------------------------------------------------------
// mavlinkdefs.h
// 转换涉及到的 MAVLink MAV_CMD / MAV_FRAME 常量（取自 common.xml 协议定义）。
// ---------------------------------------------------------------------------

namespace mav {

// ---- MAV_FRAME ----
constexpr int FRAME_GLOBAL              = 0;  // 经纬度 + AMSL 高度
constexpr int FRAME_GLOBAL_RELATIVE_ALT = 3;  // 经纬度 + 相对 home 高度
constexpr int FRAME_GLOBAL_TERRAIN_ALT  = 10; // 经纬度 + 相对地形高度

// ---- MAV_CMD ----
constexpr int CMD_NAV_WAYPOINT              = 16;
constexpr int CMD_NAV_LOITER_UNLIM          = 17;
constexpr int CMD_NAV_LOITER_TURNS          = 18;
constexpr int CMD_NAV_LOITER_TIME           = 19;
constexpr int CMD_NAV_RETURN_TO_LAUNCH      = 20;
constexpr int CMD_NAV_LAND                  = 21;
constexpr int CMD_NAV_TAKEOFF               = 22;
constexpr int CMD_NAV_VTOL_TAKEOFF          = 84;
constexpr int CMD_NAV_SPLINE_WAYPOINT       = 82;
constexpr int CMD_DO_JUMP                   = 177;
constexpr int CMD_DO_CHANGE_SPEED           = 178;
constexpr int CMD_DO_SET_HOME               = 179;
constexpr int CMD_DO_SET_ROI_LOCATION       = 190;
constexpr int CMD_DO_SET_ROI_WPNEXT_OFFSET  = 191;
constexpr int CMD_DO_SET_ROI_NONE           = 192;
constexpr int CMD_DO_MOUNT_CONTROL          = 203;
constexpr int CMD_DO_SET_CAM_TRIGG_INTERVAL = 205;
constexpr int CMD_DO_SET_CAM_TRIGG_DIST     = 206;
constexpr int CMD_DO_DIGICAM_CONTROL        = 200;
constexpr int CMD_IMAGE_START_CAPTURE       = 2450;
constexpr int CMD_IMAGE_STOP_CAPTURE        = 2451;
constexpr int CMD_VIDEO_START_CAPTURE       = 2500;
constexpr int CMD_VIDEO_STOP_CAPTURE        = 2501;

// MAV_CMD 的可读名称（用于日志/警告）
QString commandName(int cmd);

} // namespace mav
