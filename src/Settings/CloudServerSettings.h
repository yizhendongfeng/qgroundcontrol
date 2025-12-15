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
    Q_INVOKABLE QString getToken();
    Q_INVOKABLE void setToken(const QString token);

    DEFINE_SETTING_NAME_GROUP()

    DEFINE_SETTINGFACT(mqttHost)
    DEFINE_SETTINGFACT(mqttUserName)
    DEFINE_SETTINGFACT(mqttUserPassword)
    DEFINE_SETTINGFACT(serverToken)
    DEFINE_SETTINGFACT(appId)
    DEFINE_SETTINGFACT(appKey)
    DEFINE_SETTINGFACT(appLicense)
    DEFINE_SETTINGFACT(serverUrl)
    DEFINE_SETTINGFACT(websocketUrl)
    DEFINE_SETTINGFACT(rtmURL)
    DEFINE_SETTINGFACT(userName)
    DEFINE_SETTINGFACT(userPassword)
    DEFINE_SETTINGFACT(userId)
    DEFINE_SETTINGFACT(gcsSn)
    DEFINE_SETTINGFACT(droneSn)
    DEFINE_SETTINGFACT(workSpaceId)
    DEFINE_SETTINGFACT(workSpaceDesc)
};
