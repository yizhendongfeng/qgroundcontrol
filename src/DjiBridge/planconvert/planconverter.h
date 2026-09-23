#pragma once

#include <QString>
#include "waypointmodel.h"
#include "djiwpmlwriter.h"

// ---------------------------------------------------------------------------
// planconverter.h
// 双向转换的高层统一入口：
//   qgcToDji: QGC plan(.plan JSON) -> DJI WPML(.kmz)
//   djiToQgc: DJI WPML(.kmz)      -> QGC plan(.plan JSON)
// ---------------------------------------------------------------------------

namespace wpt {

class PlanConverter
{
public:
    // QGC -> DJI。options 控制 DJI 侧机型枚举、高度模式、安全参数等。
    static bool qgcToDji(const QString& qgcPlanPath, const QString& kmzOutPath,
                         const DjiWriteOptions& options, Report& report);

    // DJI -> QGC。
    static bool djiToQgc(const QString& kmzPath, const QString& planOutPath,
                         Report& report);
};

} // namespace wpt
