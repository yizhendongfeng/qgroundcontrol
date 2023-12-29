/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "VehicleInfoSettings.h"

#include <QQmlEngine>
#include <QtQml>

DECLARE_SETTINGGROUP(VehicleInfo, "VehicleInfo")
{
    qmlRegisterUncreatableType<VehicleInfoSettings>("QGroundControl.SettingsManager", 1, 0, "VehicleInfoSettings", "Reference only");
}

DECLARE_SETTINGSFACT(VehicleInfoSettings, vehicleName)
DECLARE_SETTINGSFACT(VehicleInfoSettings, uid)
DECLARE_SETTINGSFACT(VehicleInfoSettings, uid2LowBytes)
DECLARE_SETTINGSFACT(VehicleInfoSettings, uid2MidBytes)
DECLARE_SETTINGSFACT(VehicleInfoSettings, uid2HighBytes)
DECLARE_SETTINGSFACT(VehicleInfoSettings, mav_type)
DECLARE_SETTINGSFACT(VehicleInfoSettings, mav_autopilot)
DECLARE_SETTINGSFACT(VehicleInfoSettings, cameraType)
DECLARE_SETTINGSFACT(VehicleInfoSettings, gimbalType)
