#include "CloudServerSettings.h"
#include <QQmlEngine>
#include <QJsonDocument>
#include <QJsonObject>
#include "MultiVehicleManager.h"

DECLARE_SETTINGGROUP(CloudServer, "")
{
    qmlRegisterUncreatableType<CloudServerSettings>("QGroundControl.SettingsManager", 1, 0, "CloudServerSettings", "Reference only");
    // 关键：设置 webChannel.id（唯一标识，供 JS 访问）
    setProperty("webChannel.id", "qmlReceiver");  // id 可自定义（如 "userData"、"appService"）
}

void CloudServerSettings::setLoginResult(const QString jsonStr)
{
    // {
    //  "username":"pilot",
    //     "user_id":"be7c6c3d-afe9-4be4-b9eb-c55066c0914e",
    //     "workspace_id":"e3dea0f5-37f2-4d79-ae58-490af3228069",
    //     "user_type":2,
    //     "mqtt_username":"pilot",
    //     "mqtt_password":"pilot123",
    //     "access_token":"eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzI1NiJ9.eyJ3b3Jrc3BhY2VfaWQiOiJlM2RlYTBmNS0zN2YyLTRkNzktYWU1OC00OTBhZjMyMjgwNjkiLCJzdWIiOiJDbG91ZEFwaVNhbXBsZSIsInVzZXJfdHlwZSI6IjIiLCJuYmYiOjE3NjMyNjM3NDEsImxvZyI6IkxvZ2dlcltjb20uZGppLnNhbXBsZS5jb21tb24ubW9kZWwuQ3VzdG9tQ2xhaW1dIiwiaXNzIjoiREpJIiwiaWQiOiJiZTdjNmMzZC1hZmU5LTRiZTQtYjllYi1jNTUwNjZjMDkxNGUiLCJleHAiOjE3NjMzNTAxNDEsImlhdCI6MTc2MzI2Mzc0MSwidXNlcm5hbWUiOiJwaWxvdCJ9.XQ362O-RkWRoVHMJYGSIjBEXzIWiFw_9S9c4R0MelHc",
    //     "mqtt_addr":"tcp://192.168.31.208:1883"
    // }
    QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonStr.toUtf8());
    QJsonObject jsonObj = jsonDoc.object();
    _mqttHostFact->setRawValue(jsonObj["mqtt_addr"].toString());
    _serverTokenFact->setRawValue(jsonObj["access_token"].toString());
    _userNameFact->setRawValue(jsonObj["username"].toString());
    _userPasswordFact->setRawValue(jsonObj["mqtt_password"].toString());
    _workSpaceIdFact->setRawValue(jsonObj["workspace_id"].toString());
    qDebug() << "setLoginResult" << jsonStr;
    MultiVehicleManager::instance()->connectToMqttHost();
}

QString CloudServerSettings::getToken()
{
    return _serverTokenFact->rawValueString();
}

void CloudServerSettings::setToken(const QString token)
{
    _serverTokenFact->setRawValue(token);
}

DECLARE_SETTINGSFACT(CloudServerSettings, mqttHost)
DECLARE_SETTINGSFACT(CloudServerSettings, mqttUserName)
DECLARE_SETTINGSFACT(CloudServerSettings, mqttUserPassword)
DECLARE_SETTINGSFACT(CloudServerSettings, serverToken)
DECLARE_SETTINGSFACT(CloudServerSettings, appId)
DECLARE_SETTINGSFACT(CloudServerSettings, appKey)
DECLARE_SETTINGSFACT(CloudServerSettings, appLicense)
DECLARE_SETTINGSFACT(CloudServerSettings, serverUrl)
DECLARE_SETTINGSFACT(CloudServerSettings, websocketUrl)
DECLARE_SETTINGSFACT(CloudServerSettings, rtmURL)
DECLARE_SETTINGSFACT(CloudServerSettings, userName)
DECLARE_SETTINGSFACT(CloudServerSettings, userId)
DECLARE_SETTINGSFACT(CloudServerSettings, userPassword)
DECLARE_SETTINGSFACT(CloudServerSettings, gcsSn)
DECLARE_SETTINGSFACT(CloudServerSettings, droneSn)
DECLARE_SETTINGSFACT(CloudServerSettings, workSpaceId)
DECLARE_SETTINGSFACT(CloudServerSettings, workSpaceDesc)

