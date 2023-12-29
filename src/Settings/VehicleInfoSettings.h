/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include "SettingsGroup.h"

class VehicleInfoSettings : public SettingsGroup
{
    Q_OBJECT

public:
    VehicleInfoSettings(QObject* parent = nullptr);


    DEFINE_SETTING_NAME_GROUP()
    DEFINE_SETTINGFACT(vehicleName)
    DEFINE_SETTINGFACT(uid)
    DEFINE_SETTINGFACT(uid2LowBytes)
    DEFINE_SETTINGFACT(uid2MidBytes)
    DEFINE_SETTINGFACT(uid2HighBytes)
    DEFINE_SETTINGFACT(mav_type)
    DEFINE_SETTINGFACT(mav_autopilot)
    DEFINE_SETTINGFACT(cameraType)
    DEFINE_SETTINGFACT(gimbalType)
};
