#pragma once

#include <QByteArray>
#include <QString>
#include "waypointmodel.h"

// ---------------------------------------------------------------------------
// qgcplanwriter.h
// 将统一中间模型 wpt::Plan 序列化为 QGC plan 文件（JSON）。
// ---------------------------------------------------------------------------

namespace wpt {

class QgcPlanWriter
{
public:
    // 生成 QGC plan JSON 文本。
    QByteArray toJson(const Plan& plan, Report& report);
    // 写出到文件；成功返回 true。
    bool writeFile(const Plan& plan, const QString& planPath, Report& report);
};

} // namespace wpt
