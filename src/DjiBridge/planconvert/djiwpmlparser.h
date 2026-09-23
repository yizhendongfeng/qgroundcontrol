#pragma once

#include <QString>
#include "waypointmodel.h"

// ---------------------------------------------------------------------------
// djiwpmlparser.h
// 解析 DJI WPML 航线文件（.kmz）到统一中间模型 wpt::Plan。
//
// 优先读取 waylines.wpml（执行文件）获得航点；template.kml 用于补充
// 椭球高/编辑高度、云台角及模板级全局参数。若 kmz 内只有 template.kml
// （纯模板），则由模板航点构造 Plan。
// ---------------------------------------------------------------------------

namespace wpt {

class DjiWpmlParser
{
public:
    // 从 .kmz 文件解析；失败返回 false 并通过 report 输出原因。
    bool parseFile(const QString& kmzPath, Plan& out, Report& report);

    // 供调试：直接解析 waylines.wpml / template.kml 文本
    bool parseWaylinesXml(const QByteArray& xml, Plan& out, Report& report);
    bool parseTemplateXml(const QByteArray& xml, Plan& out, Report& report);
};

} // namespace wpt
