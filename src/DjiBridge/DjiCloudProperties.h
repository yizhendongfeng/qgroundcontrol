/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>

class Vehicle;
class VideoSettings;

/// @file
/// @brief DJI 上云 API —— 设备属性（物模型）JSON 组装器
///
/// 将 QGC 遥测（Vehicle / VideoSettings / GPS）组装成 DJI Pilot-to-Cloud
/// 物模型属性结构。无状态纯函数，不持有 MQTT，便于单独测试。
///
/// 属性上报走两个 topic（RC Pro / Pilot-to-Cloud）：
///   - osd   （pushMode=0，0.5Hz 周期上报）
///   - state （pushMode=1，仅变化时上报）
///
/// 参考：dji-sdk/Cloud-API-Doc  rc-pro/00.properties.md 与 aircraft/00.properties.md
class DjiCloudProperties
{
public:
    /// 地面站（RC）osd 属性：capacity_percent / height / latitude / longitude / live_status[]
    static QJsonObject buildGcsOsd(Vehicle* vehicle, VideoSettings* videoSettings);

    /// 地面站（RC）state 属性：live_capacity（结构化能力）+ firmware_version
    static QJsonObject buildGcsState();

    /// 无人机（aircraft）osd 属性：mode_code / position_state / battery / attitude / 位置速度等
    static QJsonObject buildDroneOsd(Vehicle* vehicle);

    /// 将 QGC 飞行模式字符串映射为 DJI mode_code（0~18 枚举）
    /// 支持 PX4 与 ArduPilot 两套字符串。
    static int flightModeCode(Vehicle* vehicle);

private:
    /// live_capacity 结构化能力对象
    static QJsonObject buildLiveCapacity();
    /// 电池结构：capacity_percent / remain_flight_time / batteries[]
    static QJsonObject buildBattery(Vehicle* vehicle);
    /// 定位状态：is_fixed / gps_number / rtk_number
    static QJsonObject buildPositionState(Vehicle* vehicle);
};
