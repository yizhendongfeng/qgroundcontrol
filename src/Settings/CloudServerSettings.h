/**************************************************************
    保存mqtt设置
 **************************************************************/
#pragma once

#include <QObject>

#include "SettingsGroup.h"


/// Application Settings
class CloudServerSettings : public SettingsGroup
{
    Q_OBJECT

   public:
    CloudServerSettings(QObject* parent = nullptr);

    Q_INVOKABLE void setLoginResult(const QString jsonStr);
    Q_INVOKABLE void setLiveshareConfig(int type, const QString jsonStr);

    DEFINE_SETTING_NAME_GROUP()

    DEFINE_SETTINGFACT(mqttHost)
    DEFINE_SETTINGFACT(mqttUserName)
    DEFINE_SETTINGFACT(mqttUserPassword)
    DEFINE_SETTINGFACT(serverToken)
    DEFINE_SETTINGFACT(appId)
    DEFINE_SETTINGFACT(appKey)
    DEFINE_SETTINGFACT(appLicense)
    DEFINE_SETTINGFACT(serverUrl)
    DEFINE_SETTINGFACT(serverIp)
    DEFINE_SETTINGFACT(websocketUrl)
    DEFINE_SETTINGFACT(rtmURL)
    DEFINE_SETTINGFACT(userName)
    DEFINE_SETTINGFACT(userPassword)
    DEFINE_SETTINGFACT(userId)
    DEFINE_SETTINGFACT(gcsSn)
    DEFINE_SETTINGFACT(droneSn)
    DEFINE_SETTINGFACT(workSpaceId)
    DEFINE_SETTINGFACT(workSpaceDesc)
    DEFINE_SETTINGFACT(nativeCloudConnect)
    /// 后端图形是 GCJ-02（高德）时开启：进来转 WGS84、出去转回 GCJ-02。
    /// 默认关：本项目的后台存的就是 WGS84，开着反而整体偏 300~600 米（实机验证过）。
    DEFINE_SETTINGFACT(coordinateTransform)
};
