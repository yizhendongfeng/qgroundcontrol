/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "DjiCloudProperties.h"

#include "SettingsManager.h"
#include "CloudServerSettings.h"
#include "DjiFlightModeNames.h"
#include "Vehicle.h"
#include "VideoSettings.h"
#include "GPSManager.h"
#include "GPSRtk.h"
#include "VehicleBatteryFactGroup.h"

namespace {
// 直播相机/视频索引（QGC 侧目前只有单路视频，用固定值占位；后续接多路时改为从 VideoManager 读取）
const QString kCameraIndex     = QStringLiteral("66-0-0");
// video_index 必须是 "{video_type}-{index}" 格式（如 "normal-0"），网页会把它拼进
// video_id = "{sn}/{camera_index}/{video_index}"，后端 VideoId 再按 "-" 拆出 video_type。
// 若只填纯数字（如 "1"），后端 VideoTypeEnum 解析会报 "unknown data"。
const QString kVideoIndex      = QStringLiteral("normal-0");
const QString kDefaultVideoType = QStringLiteral("normal");
} // namespace

QJsonObject DjiCloudProperties::buildGcsOsd(Vehicle* vehicle, VideoSettings* videoSettings)
{
    const QString gcsSn = SettingsManager::instance()->cloudServerSettings()->gcsSn()->rawValueString();

    QJsonObject data;
    data["capacity_percent"] = 100; // 地面站（PC）电量，无真实来源，固定满格

    // 直播状态（每个流一项）。video_id 沿用历史格式 {sn}/{camera_index}/{video_type}-{index}，
    // 与 buildGcsState 的 video_index 字段不一致，保留历史行为，待多路视频时统一。
    QJsonArray liveStatus;
    QJsonObject videoLive;
    videoLive["video_id"]      = gcsSn + "/" + kCameraIndex + "/" + "normal-0";
    videoLive["video_type"]    = kDefaultVideoType;
    videoLive["video_quality"] = 3; // 0自适应 1流畅 2标清 3高清 4超清；真实清晰度需 VideoReceiver 暴露 getter 后回读
    videoLive["status"]        = videoSettings ? videoSettings->streamingOut() : false; // 0未直播 1在直播
    videoLive["error_status"]  = 0;
    liveStatus.append(videoLive);
    data["live_status"] = liveStatus;

    // TODO 以后要改成设备自己获取的地址   当前地面站测试使用电脑，位置沿用 home 位置
    if (vehicle) {
        data["latitude"]  = vehicle->homePosition().latitude();
        data["longitude"] = vehicle->homePosition().longitude();
        data["height"]    = vehicle->homePosition().altitude();
    }

    return data;
}

QJsonObject DjiCloudProperties::buildGcsState()
{
    QJsonObject data;
    data["live_capacity"]    = buildLiveCapacity();

    // 地面站固件版本用 QGC 版本占位；真实 GCS 固件版本无来源
    data["firmware_version"] = QStringLiteral("QGC-DGCS");

    return data;
}

QJsonObject DjiCloudProperties::buildLiveCapacity()
{
    const QString droneSn = SettingsManager::instance()->cloudServerSettings()->droneSn()->rawValueString();

    QJsonObject liveCapacity;
    liveCapacity["available_video_number"]      = 1;
    liveCapacity["coexist_video_number_max"]    = 1;

    QJsonArray deviceList;
    QJsonObject device;
    device["sn"]                        = droneSn;
    device["available_video_number"]    = 1;
    device["coexist_video_number_max"]  = 1;

    QJsonArray cameraList;
    QJsonObject camera;
    camera["camera_index"]              = kCameraIndex;
    camera["available_video_number"]    = 1;
    camera["coexist_video_number_max"]  = 1;

    QJsonArray videoList;
    QJsonObject video;
    video["video_index"]            = kVideoIndex;
    video["video_type"]             = kDefaultVideoType;
    video["switchable_video_types"] = QJsonArray{"zoom", "wide", "thermal", "normal", "ir"};
    videoList.append(video);

    camera["video_list"] = videoList;
    cameraList.append(camera);
    device["camera_list"] = cameraList;
    deviceList.append(device);

    liveCapacity["device_list"] = deviceList;
    return liveCapacity;
}

QJsonObject DjiCloudProperties::buildDroneOsd(Vehicle* vehicle)
{
    if (!vehicle) {
        return QJsonObject();
    }

    QJsonObject data;

    data["mode_code"]      = flightModeCode(vehicle);
    data["position_state"] = buildPositionState(vehicle);
    data["battery"]        = buildBattery(vehicle);

    data["home_distance"]  = vehicle->distanceToHome()->rawValue().toDouble();
    data["home_latitude"]  = vehicle->homePosition().latitude();
    data["home_longitude"] = vehicle->homePosition().longitude();

    data["attitude_head"]  = vehicle->heading()->rawValue().toInt();
    data["attitude_roll"]  = vehicle->roll()->rawValue().toDouble();
    data["attitude_pitch"] = vehicle->pitch()->rawValue().toDouble();

    data["elevation"] = vehicle->altitudeRelative()->rawValue().toDouble();
    data["height"]    = vehicle->altitudeAMSL()->rawValue().toDouble();
    data["latitude"]  = vehicle->latitude();
    data["longitude"] = vehicle->longitude();

    data["vertical_speed"]   = vehicle->climbRate()->rawValue().toDouble();
    data["horizontal_speed"] = vehicle->groundSpeed()->rawValue().toDouble();

    data["firmware_version"] = QString::number(vehicle->firmwareMajorVersion()) + "." +
                               QString::number(vehicle->firmwareMinorVersion()) + "." +
                               QString::number(vehicle->firmwarePatchVersion()) + ".";

    data["wind_direction"] = vehicle->windFactGroup()->getFact("direction")->rawValue().toDouble();
    data["wind_speed"]     = vehicle->windFactGroup()->getFact("speed")->rawValue().toDouble();

    // 以下属性 QGC 暂无可靠遥测来源，暂不上报（DJI 允许 osd 只携带部分属性）：
    //   country / gear / control_source / obstacle_avoidance / night_lights_state /
    //   height_limit / is_near_area_limit / is_near_height_limit /
    //   activation_time / total_flight_sorties / total_flight_distance /
    //   total_flight_time / storage / cameras / track_id / maintain_status 等。

    return data;
}

QJsonObject DjiCloudProperties::buildBattery(Vehicle* vehicle)
{
    QJsonObject battery;
    if (vehicle->batteries()->count() > 0) {
        auto batteryFactGroup = qobject_cast<VehicleBatteryFactGroup*>(vehicle->batteries()->get(0));
        if (batteryFactGroup) {
            battery["capacity_percent"]   = batteryFactGroup->percentRemaining()->rawValue().toDouble();
            battery["remain_flight_time"] = batteryFactGroup->timeRemaining()->rawValue().toDouble();
        }
    }
    return battery;
}

QJsonObject DjiCloudProperties::buildPositionState(Vehicle* vehicle)
{
    QJsonObject positionState;
    switch (vehicle->gpsFactGroup()->getFact("lock")->enumIndex()) {
    // "None,None,2D Lock,3D Lock,3D DGPS Lock,3D RTK GPS Lock (float),3D RTK GPS Lock (fixed),Static (fixed)"
    case 0:
    case 1:
        positionState["is_fixed"] = 0;
        break;
    case 2:
        positionState["is_fixed"] = 1;
        break;
    case 3:
    case 4:
    case 5:
        positionState["is_fixed"] = 2;
        break;
    default:
        positionState["is_fixed"] = 0;
        break;
    }
    positionState["gps_number"] = vehicle->gpsFactGroup()->getFact("count")->rawValue().toInt();

    GPSRtk* gpsRtk = GPSManager::instance()->gpsRtk();
    positionState["rtk_number"] = gpsRtk->connected()
            ? gpsRtk->gpsRtkFactGroup()->getFact("numSatellites")->rawValue().toInt()
            : 0;
    return positionState;
}

int DjiCloudProperties::flightModeCode(Vehicle* vehicle)
{
    if (!vehicle) {
        return 0;
    }

    // 未起飞一律视为待机
    if (!vehicle->flying()) {
        return 0;
    }

    const QString mode = vehicle->flightMode();

    // 模式名是**译文**（中文界面下 "Position" 是 "定点Position"），所以比对走
    // DjiFlightModeNames 把英文原名翻一遍再比 —— 直接写字面量的话，非英文界面下
    // 这张表一条都命中不了，上报给云端的 mode_code 永远是 0。

    // ---- PX4 字符串（历史映射，保持不变） ----
    if (DjiFlightModeNames::modeIs(mode, "Ready"))    return 2;  // 起飞准备完毕
    if (DjiFlightModeNames::modeIs(mode, "Takeoff"))  return 3;  // 手动飞行
    if (DjiFlightModeNames::modeIs(mode, "Position")) return 4;  // 自动起飞
    if (DjiFlightModeNames::modeIs(mode, "Mission"))  return 5;  // 航线飞行
    if (DjiFlightModeNames::modeIs(mode, "Return"))   return 9;  // 自动返航
    if (DjiFlightModeNames::modeIs(mode, "Land"))     return 10; // 自动降落

    // ---- ArduPilot 字符串 ----
    if (DjiFlightModeNames::modeIsAny(mode, {"Stabilize", "Altitude Hold", "Loiter",
                                             "Position Hold", "Brake"}))     return 3;  // 手动飞行
    if (DjiFlightModeNames::modeIs(mode, "Auto"))                             return 5;  // 航线飞行
    if (DjiFlightModeNames::modeIsAny(mode, {"Guided", "Guided No GPS"}))     return 17; // 指令飞行
    if (DjiFlightModeNames::modeIsAny(mode, {"RTL", "Smart RTL"}))            return 9;  // 自动返航
    if (DjiFlightModeNames::modeIs(mode, "Follow"))                           return 7;  // 智能跟随

    return 0;
}
