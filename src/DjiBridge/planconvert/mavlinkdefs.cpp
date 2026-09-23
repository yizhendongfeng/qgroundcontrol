#include "mavlinkdefs.h"

namespace mav {

QString commandName(int cmd)
{
    switch (cmd) {
    case CMD_NAV_WAYPOINT:              return QStringLiteral("NAV_WAYPOINT");
    case CMD_NAV_LOITER_UNLIM:          return QStringLiteral("NAV_LOITER_UNLIM");
    case CMD_NAV_LOITER_TURNS:          return QStringLiteral("NAV_LOITER_TURNS");
    case CMD_NAV_LOITER_TIME:           return QStringLiteral("NAV_LOITER_TIME");
    case CMD_NAV_RETURN_TO_LAUNCH:      return QStringLiteral("NAV_RETURN_TO_LAUNCH");
    case CMD_NAV_LAND:                  return QStringLiteral("NAV_LAND");
    case CMD_NAV_TAKEOFF:               return QStringLiteral("NAV_TAKEOFF");
    case CMD_NAV_VTOL_TAKEOFF:          return QStringLiteral("NAV_VTOL_TAKEOFF");
    case CMD_NAV_SPLINE_WAYPOINT:       return QStringLiteral("NAV_SPLINE_WAYPOINT");
    case CMD_DO_JUMP:                   return QStringLiteral("DO_JUMP");
    case CMD_DO_CHANGE_SPEED:           return QStringLiteral("DO_CHANGE_SPEED");
    case CMD_DO_SET_HOME:               return QStringLiteral("DO_SET_HOME");
    case CMD_DO_SET_ROI_LOCATION:       return QStringLiteral("DO_SET_ROI_LOCATION");
    case CMD_DO_SET_ROI_WPNEXT_OFFSET:  return QStringLiteral("DO_SET_ROI_WPNEXT_OFFSET");
    case CMD_DO_SET_ROI_NONE:           return QStringLiteral("DO_SET_ROI_NONE");
    case CMD_DO_MOUNT_CONTROL:          return QStringLiteral("DO_MOUNT_CONTROL");
    case CMD_DO_SET_CAM_TRIGG_INTERVAL: return QStringLiteral("DO_SET_CAM_TRIGG_INTERVAL");
    case CMD_DO_SET_CAM_TRIGG_DIST:     return QStringLiteral("DO_SET_CAM_TRIGG_DIST");
    case CMD_DO_DIGICAM_CONTROL:        return QStringLiteral("DO_DIGICAM_CONTROL");
    case CMD_IMAGE_START_CAPTURE:       return QStringLiteral("IMAGE_START_CAPTURE");
    case CMD_IMAGE_STOP_CAPTURE:        return QStringLiteral("IMAGE_STOP_CAPTURE");
    case CMD_VIDEO_START_CAPTURE:       return QStringLiteral("VIDEO_START_CAPTURE");
    case CMD_VIDEO_STOP_CAPTURE:        return QStringLiteral("VIDEO_STOP_CAPTURE");
    default:
        return QStringLiteral("MAV_CMD_%1").arg(cmd);
    }
}

} // namespace mav
