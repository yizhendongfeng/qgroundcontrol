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

    // ---------- 指令飞行 / 远程控制（DRC） ----------
    /// 三个摇杆轴的取反开关：协议里 x/y/w 的正方向在不同后端/机型上说法不一，
    /// 台架打单轴实测才知道，所以留给设置项。见 DjiDrcControlMapper::applyDroneControl。
    DEFINE_SETTINGFACT(drcInvertX)
    DEFINE_SETTINGFACT(drcInvertY)
    DEFINE_SETTINGFACT(drcInvertW)
    /// 镜头水平视场角（度）。camera_aim 要把画面归一化坐标换成云台角度，
    /// 而 QGC 读不到镜头 FOV，只能按镜头手工配。见 DjiDrcControlMapper::cameraAim。
    DEFINE_SETTINGFACT(drcCameraHFov)
    /// 云端要控制权时是否必须本地操作员点头。**默认开**（JSON 里是 true；
    /// 这里原先写成"默认关"，与生成出来的默认值打架）。
    DEFINE_SETTINGFACT(drcRequireLocalConsent)
    /// 是否允许云端下发的紧急停桨到达飞机（映射为飞行终止）。默认关。
    DEFINE_SETTINGFACT(drcEmergencyStopEnabled)
};
