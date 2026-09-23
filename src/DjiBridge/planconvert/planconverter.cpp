#include "planconverter.h"

#include "qgcplanparser.h"
#include "qgcplanwriter.h"
#include "djiwpmlparser.h"

namespace wpt {

bool PlanConverter::qgcToDji(const QString& qgcPlanPath, const QString& kmzOutPath,
                             const DjiWriteOptions& options, Report& report)
{
    Plan plan;
    QgcPlanParser parser;
    if (!parser.parseFile(qgcPlanPath, plan, report))
        return false;

    report.convertedWaypoints = plan.firstWayline().waypoints.size();

    // 应用转换选项
    plan.mission.flyToWaylineMode = options.flyToWaylineMode;
    plan.mission.finishAction = options.finishAction;
    plan.mission.exitOnRCLost = options.exitOnRCLost;
    plan.mission.executeRCLostAction = options.executeRCLostAction;
    plan.mission.takeOffSecurityHeight = options.takeOffSecurityHeight;
    plan.mission.globalTransitionalSpeed = options.globalTransitionalSpeed;
    plan.mission.globalRTHHeight = options.globalRTHHeight;
    plan.mission.droneEnumValue = options.droneEnumValue;
    plan.mission.droneSubEnumValue = options.droneSubEnumValue;
    plan.mission.payloadEnumValue = options.payloadEnumValue;
    plan.mission.payloadPositionIndex = options.payloadPositionIndex;

    // 参考起飞点：优先用 QGC plannedHomePosition（DJI 格式为 "lon,lat,height"）
    if (plan.hasPlannedHome) {
        plan.mission.takeOffRefPoint = QStringLiteral("%1,%2,%3")
                                           .arg(plan.homeLon, 0, 'f', 8)
                                           .arg(plan.homeLat, 0, 'f', 8)
                                           .arg(plan.homeAltAmsl, 0, 'f', 3);
    }

    DjiWpmlWriter writer;
    return writer.writeKmz(plan, kmzOutPath, options, report);
}

bool PlanConverter::djiToQgc(const QString& kmzPath, const QString& planOutPath,
                             Report& report)
{
    Plan plan;
    DjiWpmlParser parser;
    if (!parser.parseFile(kmzPath, plan, report))
        return false;

    report.convertedWaypoints = plan.firstWayline().waypoints.size();

    QgcPlanWriter writer;
    return writer.writeFile(plan, planOutPath, report);
}

} // namespace wpt
