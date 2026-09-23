#pragma once

#include <QByteArray>
#include <QString>
#include "waypointmodel.h"

// ---------------------------------------------------------------------------
// djiwpmlwriter.h
// 将统一中间模型 wpt::Plan 序列化为 DJI WPML 航线文件（.kmz）。
//
// 生成内容：
//   template.kml   —— 航点飞行模板（供 Pilot2 / FlightHub2 重新编辑）
//   waylines.wpml  —— 可执行航线文件（飞机直接执行）
// 两者经 ZIP 打包为 .kmz。
// ---------------------------------------------------------------------------

namespace wpt {

struct DjiWriteOptions {
    QString author = QStringLiteral("DjiQgcPlanFileConvert");
    QString coordinateMode = QStringLiteral("WGS84");
    // template 编辑高度模式：EGM96 | relativeToStartPoint
    QString templateHeightMode = QStringLiteral("relativeToStartPoint");
    // 大地水准面偏移：WGS84 椭球高 = 海拔高 + geoidOffset（默认 0，仅提示）
    double geoidOffset = 0.0;
    QString gimbalPitchMode = QStringLiteral("usePointSetting");
    QString flyToWaylineMode = QStringLiteral("safely");
    QString finishAction = QStringLiteral("goHome");
    QString exitOnRCLost = QStringLiteral("goContinue");
    QString executeRCLostAction = QStringLiteral("hover");
    double takeOffSecurityHeight = 20.0;
    double globalTransitionalSpeed = 8.0;
    double globalRTHHeight = 50.0;
    int droneEnumValue = 67;
    int droneSubEnumValue = 0;
    int payloadEnumValue = 52;
    int payloadPositionIndex = 0;
    int templateType = 0; // 0=waypoint（本转换器只输出航点飞行模板）
};

class DjiWpmlWriter
{
public:
    // 生成 .kmz 文件（ZIP 归档 template.kml + waylines.wpml）。
    bool writeKmz(const Plan& plan, const QString& kmzPath,
                  const DjiWriteOptions& options, Report& report);

    // 生成 waylines.wpml 文本（供调试/检查）
    QByteArray buildWaylinesXml(const Plan& plan, const DjiWriteOptions& options, Report& report);
    // 生成 template.kml 文本（供调试/检查）
    QByteArray buildTemplateXml(const Plan& plan, const DjiWriteOptions& options, Report& report);
};

} // namespace wpt
