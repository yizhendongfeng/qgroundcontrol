#include "djiwpmlwriter.h"

#include <QDateTime>
#include <QFile>
#include <QXmlStreamWriter>

#include "ziputil.h"

namespace wpt {

namespace {

const QString kKmlNs  = QStringLiteral("http://www.opengis.net/kml/2.2");
const QString kWpmlNs = QStringLiteral("http://www.dji.com/wpmz/1.0.2");

QString coord(double v)
{
    return QString::number(v, 'f', 8);
}

QString num(double v, int prec = 3)
{
    return QString::number(v, 'f', prec);
}

QString headingModeName(Waypoint::HeadingMode m)
{
    switch (m) {
    case Waypoint::HeadingMode::FollowWayline:    return QStringLiteral("followWayline");
    case Waypoint::HeadingMode::Manually:         return QStringLiteral("manually");
    case Waypoint::HeadingMode::Fixed:            return QStringLiteral("fixed");
    case Waypoint::HeadingMode::SmoothTransition: return QStringLiteral("smoothTransition");
    case Waypoint::HeadingMode::TowardPoi:        return QStringLiteral("towardPOI");
    }
    return QStringLiteral("followWayline");
}

QString turnModeName(Waypoint::TurnMode m)
{
    switch (m) {
    case Waypoint::TurnMode::CoordinateTurn:              return QStringLiteral("coordinateTurn");
    case Waypoint::TurnMode::ToPointAndStopDiscontinuity: return QStringLiteral("toPointAndStopWithDiscontinuityCurvature");
    case Waypoint::TurnMode::ToPointAndStopContinuity:    return QStringLiteral("toPointAndStopWithContinuityCurvature");
    case Waypoint::TurnMode::ToPointAndPassContinuity:    return QStringLiteral("toPointAndPassWithContinuityCurvature");
    }
    return QStringLiteral("toPointAndStopWithDiscontinuityCurvature");
}

// 航点是否有航点级偏航设置（而非跟随全局）。
// 对应 QGC plan params[3] 有数值（手动偏航）或 DJI towardPOI 模式。
// 仅当此类航点 useGlobalHeadingParam=0，waypointHeadingParam 才生效。
bool hasManualHeading(const Waypoint& wp)
{
    return wp.headingMode == Waypoint::HeadingMode::Manually
        || wp.headingMode == Waypoint::HeadingMode::Fixed
        || wp.headingMode == Waypoint::HeadingMode::SmoothTransition
        || wp.headingMode == Waypoint::HeadingMode::TowardPoi;
}

// 写入 wpml:missionConfig
void writeMissionConfig(QXmlStreamWriter& w, const Plan& plan, const DjiWriteOptions& opt)
{
    w.writeStartElement(kWpmlNs, QStringLiteral("missionConfig"));
    w.writeTextElement(kWpmlNs, QStringLiteral("flyToWaylineMode"), opt.flyToWaylineMode);
    w.writeTextElement(kWpmlNs, QStringLiteral("finishAction"), opt.finishAction);
    w.writeTextElement(kWpmlNs, QStringLiteral("exitOnRCLost"), opt.exitOnRCLost);
    if (opt.exitOnRCLost == QStringLiteral("executeLostAction"))
        w.writeTextElement(kWpmlNs, QStringLiteral("executeRCLostAction"), opt.executeRCLostAction);
    w.writeTextElement(kWpmlNs, QStringLiteral("takeOffSecurityHeight"), num(opt.takeOffSecurityHeight));
    w.writeTextElement(kWpmlNs, QStringLiteral("globalTransitionalSpeed"), num(opt.globalTransitionalSpeed));
    w.writeTextElement(kWpmlNs, QStringLiteral("globalRTHHeight"), num(opt.globalRTHHeight));

    if (!plan.mission.takeOffRefPoint.isEmpty()) {
        w.writeTextElement(kWpmlNs, QStringLiteral("takeOffRefPoint"), plan.mission.takeOffRefPoint);
        w.writeTextElement(kWpmlNs, QStringLiteral("takeOffRefPointAGLHeight"), num(plan.mission.takeOffRefPointAGLHeight));
    }

    w.writeStartElement(kWpmlNs, QStringLiteral("droneInfo"));
    w.writeTextElement(kWpmlNs, QStringLiteral("droneEnumValue"), QString::number(opt.droneEnumValue));
    w.writeTextElement(kWpmlNs, QStringLiteral("droneSubEnumValue"), QString::number(opt.droneSubEnumValue));
    w.writeEndElement();

    w.writeStartElement(kWpmlNs, QStringLiteral("payloadInfo"));
    w.writeTextElement(kWpmlNs, QStringLiteral("payloadEnumValue"), QString::number(opt.payloadEnumValue));
    w.writeTextElement(kWpmlNs, QStringLiteral("payloadPositionIndex"), QString::number(opt.payloadPositionIndex));
    w.writeEndElement();

    w.writeEndElement(); // missionConfig
}

// 写入航点动作
void writeAction(QXmlStreamWriter& w, const DjiAction& act, int actionId, int payloadIndex)
{
    w.writeStartElement(kWpmlNs, QStringLiteral("action"));
    w.writeTextElement(kWpmlNs, QStringLiteral("actionId"), QString::number(actionId));
    w.writeTextElement(kWpmlNs, QStringLiteral("actionActuatorFunc"), DjiAction::funcName(act.func));
    w.writeStartElement(kWpmlNs, QStringLiteral("actionActuatorFuncParam"));

    switch (act.func) {
    case DjiAction::Func::TakePhoto:
        w.writeTextElement(kWpmlNs, QStringLiteral("payloadPositionIndex"), QString::number(payloadIndex));
        w.writeTextElement(kWpmlNs, QStringLiteral("fileSuffix"), act.fileSuffix);
        break;
    case DjiAction::Func::StartRecord:
        w.writeTextElement(kWpmlNs, QStringLiteral("payloadPositionIndex"), QString::number(payloadIndex));
        w.writeTextElement(kWpmlNs, QStringLiteral("fileSuffix"), act.fileSuffix);
        break;
    case DjiAction::Func::StopRecord:
        w.writeTextElement(kWpmlNs, QStringLiteral("payloadPositionIndex"), QString::number(payloadIndex));
        break;
    case DjiAction::Func::Hover:
        w.writeTextElement(kWpmlNs, QStringLiteral("hoverTime"), num(act.hoverTime));
        break;
    case DjiAction::Func::GimbalRotate:
        w.writeTextElement(kWpmlNs, QStringLiteral("payloadPositionIndex"), QString::number(payloadIndex));
        w.writeTextElement(kWpmlNs, QStringLiteral("gimbalHeadingYawBase"), QStringLiteral("north"));
        w.writeTextElement(kWpmlNs, QStringLiteral("gimbalRotateMode"), QStringLiteral("absoluteAngle"));
        w.writeTextElement(kWpmlNs, QStringLiteral("gimbalPitchRotateEnable"), QStringLiteral("1"));
        w.writeTextElement(kWpmlNs, QStringLiteral("gimbalPitchRotateAngle"), num(act.gimbalPitchAngle));
        w.writeTextElement(kWpmlNs, QStringLiteral("gimbalRollRotateEnable"), QStringLiteral("0"));
        w.writeTextElement(kWpmlNs, QStringLiteral("gimbalRollRotateAngle"), num(0));
        w.writeTextElement(kWpmlNs, QStringLiteral("gimbalYawRotateEnable"), QStringLiteral("1"));
        w.writeTextElement(kWpmlNs, QStringLiteral("gimbalYawRotateAngle"), num(act.gimbalYawAngle));
        w.writeTextElement(kWpmlNs, QStringLiteral("gimbalRotateTimeEnable"), act.gimbalRotateTime > 0.0 ? QStringLiteral("1") : QStringLiteral("0"));
        w.writeTextElement(kWpmlNs, QStringLiteral("gimbalRotateTime"), num(act.gimbalRotateTime > 0.0 ? act.gimbalRotateTime : 0.0));
        break;
    case DjiAction::Func::Zoom:
        w.writeTextElement(kWpmlNs, QStringLiteral("payloadPositionIndex"), QString::number(payloadIndex));
        w.writeTextElement(kWpmlNs, QStringLiteral("focalLength"), num(act.focalLength));
        break;
    default:
        break;
    }

    w.writeEndElement(); // actionActuatorFuncParam
    w.writeEndElement(); // action
}

// 写一个 actionGroup
void writeActionGroup(QXmlStreamWriter& w, int& groupId, int waypointIndex,
                      const QString& triggerType, double triggerParam,
                      const QVector<DjiAction>& acts, int payloadIndex)
{
    if (acts.isEmpty())
        return;
    w.writeStartElement(kWpmlNs, QStringLiteral("actionGroup"));
    w.writeTextElement(kWpmlNs, QStringLiteral("actionGroupId"), QString::number(groupId++));
    w.writeTextElement(kWpmlNs, QStringLiteral("actionGroupStartIndex"), QString::number(waypointIndex));
    w.writeTextElement(kWpmlNs, QStringLiteral("actionGroupEndIndex"), QString::number(waypointIndex));
    w.writeTextElement(kWpmlNs, QStringLiteral("actionGroupMode"), QStringLiteral("sequence"));
    w.writeStartElement(kWpmlNs, QStringLiteral("actionTrigger"));
    w.writeTextElement(kWpmlNs, QStringLiteral("actionTriggerType"), triggerType);
    if (triggerParam > 0.0)
        w.writeTextElement(kWpmlNs, QStringLiteral("actionTriggerParam"), num(triggerParam));
    w.writeEndElement(); // actionTrigger
    int actionId = 0;
    for (const DjiAction& act : acts)
        writeAction(w, act, actionId++, payloadIndex);
    w.writeEndElement(); // actionGroup
}

void writeWaypointXml(QXmlStreamWriter& w, const Waypoint& wp, int index,
                      const Wayline& line, int payloadIndex, int& actionGroupId, Report& report)
{
    w.writeStartElement(kKmlNs, QStringLiteral("Placemark"));
    w.writeStartElement(kKmlNs, QStringLiteral("Point"));
    // KML 坐标顺序：经度,纬度（与 QGC 的 [lat,lon] 相反）
    w.writeTextElement(kKmlNs, QStringLiteral("coordinates"),
                       coord(wp.lon) + QLatin1Char(',') + coord(wp.lat));
    w.writeEndElement(); // Point

    w.writeTextElement(kWpmlNs, QStringLiteral("index"), QString::number(index));
    w.writeTextElement(kWpmlNs, QStringLiteral("executeHeight"), num(wp.executeHeight, 3));
    // 每个航点显式给出速度（局部或全局），与官方示例一致
    const double wpSpeed = wp.hasLocalSpeed ? wp.speed : line.autoFlightSpeed;
    w.writeTextElement(kWpmlNs, QStringLiteral("waypointSpeed"), num(wpSpeed));

    // 偏航参数：仅航点级偏航设置（手动偏航/朝向 POI）时写出；
    // 跟随全局的航点不写 waypointHeadingParam（DJI 缺省即使用全局参数）
    if (hasManualHeading(wp)) {
        w.writeStartElement(kWpmlNs, QStringLiteral("waypointHeadingParam"));
        w.writeTextElement(kWpmlNs, QStringLiteral("waypointHeadingMode"), headingModeName(wp.headingMode));
        if (wp.headingMode == Waypoint::HeadingMode::SmoothTransition
            || wp.headingMode == Waypoint::HeadingMode::Fixed
            || wp.headingMode == Waypoint::HeadingMode::Manually) {
            w.writeTextElement(kWpmlNs, QStringLiteral("waypointHeadingAngle"), num(wp.headingAngle, 1));
        }
        if (wp.headingMode == Waypoint::HeadingMode::TowardPoi && wp.hasPoi) {
            w.writeTextElement(kWpmlNs, QStringLiteral("waypointPoiPoint"),
                               coord(wp.poiLat) + QLatin1Char(',') + coord(wp.poiLon) + QLatin1Char(',') + num(wp.poiAlt));
            w.writeTextElement(kWpmlNs, QStringLiteral("waypointHeadingPathMode"), QStringLiteral("clockwise"));
        }
        w.writeEndElement(); // waypointHeadingParam
    }

    // 转弯参数
    w.writeStartElement(kWpmlNs, QStringLiteral("waypointTurnParam"));
    w.writeTextElement(kWpmlNs, QStringLiteral("waypointTurnMode"), turnModeName(wp.turnMode));
    const bool needDamping = (wp.turnMode == Waypoint::TurnMode::CoordinateTurn);
    if (needDamping)
        w.writeTextElement(kWpmlNs, QStringLiteral("waypointTurnDampingDist"), num(wp.turnDampingDist));
    w.writeEndElement(); // waypointTurnParam

    // 动作组（actionGroupId 在整个 kmz 内唯一，由调用方传入共享计数器）
    // 等距/等时拍照
    if (wp.photoDistanceInterval > 0.0) {
        QVector<DjiAction> acts;
        DjiAction a;
        a.func = DjiAction::Func::TakePhoto;
        a.fileSuffix = QStringLiteral("wp%1").arg(index);
        acts.append(a);
        writeActionGroup(w, actionGroupId, index, QStringLiteral("multipleDistance"), wp.photoDistanceInterval, acts, payloadIndex);
    }
    if (wp.photoTimeInterval > 0.0) {
        QVector<DjiAction> acts;
        DjiAction a;
        a.func = DjiAction::Func::TakePhoto;
        a.fileSuffix = QStringLiteral("wp%1").arg(index);
        acts.append(a);
        writeActionGroup(w, actionGroupId, index, QStringLiteral("multipleTiming"), wp.photoTimeInterval, acts, payloadIndex);
    }
    // reachPoint 触发的动作
    if (!wp.actions.isEmpty())
        writeActionGroup(w, actionGroupId, index, QStringLiteral("reachPoint"), 0.0, wp.actions, payloadIndex);

    w.writeEndElement(); // Placemark
}

} // namespace

QByteArray DjiWpmlWriter::buildWaylinesXml(const Plan& plan, const DjiWriteOptions& opt, Report& report)
{
    QByteArray out;
    QXmlStreamWriter w(&out);
    w.setAutoFormatting(true);
    w.setAutoFormattingIndent(2);
    w.writeStartDocument(QStringLiteral("1.0"), true);
    w.writeStartElement(kKmlNs, QStringLiteral("kml"));
    w.writeDefaultNamespace(kKmlNs);
    w.writeNamespace(kWpmlNs, QStringLiteral("wpml"));
    w.writeStartElement(kKmlNs, QStringLiteral("Document"));

    writeMissionConfig(w, plan, opt);

    for (const Wayline& line : plan.waylines) {
        w.writeStartElement(kKmlNs, QStringLiteral("Folder"));
        w.writeTextElement(kWpmlNs, QStringLiteral("templateId"), QString::number(line.templateId));
        w.writeTextElement(kWpmlNs, QStringLiteral("waylineId"), QString::number(line.waylineId));
        w.writeTextElement(kWpmlNs, QStringLiteral("executeHeightMode"), line.executeHeightMode);
        w.writeTextElement(kWpmlNs, QStringLiteral("autoFlightSpeed"), num(line.autoFlightSpeed));

        int index = 0;
        int actionGroupId = 0;
        for (const Waypoint& wp : line.waypoints)
            writeWaypointXml(w, wp, index++, line, opt.payloadPositionIndex, actionGroupId, report);

        w.writeEndElement(); // Folder
    }

    w.writeEndElement(); // Document
    w.writeEndElement(); // kml
    w.writeEndDocument();
    return out;
}

QByteArray DjiWpmlWriter::buildTemplateXml(const Plan& plan, const DjiWriteOptions& opt, Report& report)
{
    QByteArray out;
    QXmlStreamWriter w(&out);
    w.setAutoFormatting(true);
    w.setAutoFormattingIndent(2);
    w.writeStartDocument(QStringLiteral("1.0"), true);
    w.writeStartElement(kKmlNs, QStringLiteral("kml"));
    w.writeDefaultNamespace(kKmlNs);
    w.writeNamespace(kWpmlNs, QStringLiteral("wpml"));
    w.writeStartElement(kKmlNs, QStringLiteral("Document"));

    // 创建信息
    w.writeTextElement(kWpmlNs, QStringLiteral("author"), opt.author);
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    w.writeTextElement(kWpmlNs, QStringLiteral("createTime"), QString::number(nowMs));
    w.writeTextElement(kWpmlNs, QStringLiteral("updateTime"), QString::number(nowMs));

    writeMissionConfig(w, plan, opt);

    for (const Wayline& line : plan.waylines) {
        w.writeStartElement(kKmlNs, QStringLiteral("Folder"));
        w.writeTextElement(kWpmlNs, QStringLiteral("templateType"), QStringLiteral("waypoint"));
        w.writeTextElement(kWpmlNs, QStringLiteral("templateId"), QString::number(line.templateId));

        // 坐标系参数
        w.writeStartElement(kWpmlNs, QStringLiteral("waylineCoordinateSysParam"));
        w.writeTextElement(kWpmlNs, QStringLiteral("coordinateMode"), opt.coordinateMode);
        w.writeTextElement(kWpmlNs, QStringLiteral("heightMode"), opt.templateHeightMode);
        w.writeTextElement(kWpmlNs, QStringLiteral("positioningType"), QStringLiteral("GPS"));
        w.writeTextElement(kWpmlNs, QStringLiteral("globalShootHeight"), num(opt.takeOffSecurityHeight));
        w.writeTextElement(kWpmlNs, QStringLiteral("surfaceFollowModeEnable"), QString::number(plan.surfaceFollowModeEnable));
        if (plan.surfaceFollowModeEnable)
            w.writeTextElement(kWpmlNs, QStringLiteral("surfaceRelativeHeight"), num(plan.surfaceRelativeHeight));
        w.writeEndElement(); // waylineCoordinateSysParam

        w.writeTextElement(kWpmlNs, QStringLiteral("autoFlightSpeed"), num(line.autoFlightSpeed));
        w.writeTextElement(kWpmlNs, QStringLiteral("gimbalPitchMode"), opt.gimbalPitchMode);

        // 全局偏航角
        w.writeStartElement(kWpmlNs, QStringLiteral("globalWaypointHeadingParam"));
        w.writeTextElement(kWpmlNs, QStringLiteral("waypointHeadingMode"), headingModeName(plan.globalHeadingMode));
        if (plan.globalHeadingMode == Waypoint::HeadingMode::SmoothTransition)
            w.writeTextElement(kWpmlNs, QStringLiteral("waypointHeadingAngle"), num(plan.globalHeadingAngle, 1));
        w.writeTextElement(kWpmlNs, QStringLiteral("waypointHeadingPathMode"), QStringLiteral("clockwise"));
        w.writeEndElement(); // globalWaypointHeadingParam

        w.writeTextElement(kWpmlNs, QStringLiteral("globalWaypointTurnMode"), turnModeName(plan.globalTurnMode));
        w.writeTextElement(kWpmlNs, QStringLiteral("globalUseStraightLine"), QString::number(plan.globalUseStraightLine));

        // 航点
        int index = 0;
        for (const Waypoint& wp : line.waypoints) {
            w.writeStartElement(kKmlNs, QStringLiteral("Placemark"));
            w.writeStartElement(kKmlNs, QStringLiteral("Point"));
            w.writeTextElement(kKmlNs, QStringLiteral("coordinates"),
                               coord(wp.lon) + QLatin1Char(',') + coord(wp.lat));
            w.writeEndElement(); // Point

            w.writeTextElement(kWpmlNs, QStringLiteral("index"), QString::number(index));
            // template 中椭球高与编辑高度成对出现
            const double editH = wp.ellipsoidHeight - opt.geoidOffset;
            w.writeTextElement(kWpmlNs, QStringLiteral("ellipsoidHeight"), num(wp.ellipsoidHeight));
            w.writeTextElement(kWpmlNs, QStringLiteral("height"), num(editH));
            w.writeTextElement(kWpmlNs, QStringLiteral("useGlobalHeight"), QStringLiteral("0"));
            w.writeTextElement(kWpmlNs, QStringLiteral("useGlobalSpeed"), wp.hasLocalSpeed ? QStringLiteral("0") : QStringLiteral("1"));
            // 偏航：仅航点级偏航设置时 useGlobalHeadingParam=0（waypointHeadingParam 生效），
            // 否则 =1 跟随全局偏航参数
            w.writeTextElement(kWpmlNs, QStringLiteral("useGlobalHeadingParam"),
                               hasManualHeading(wp) ? QStringLiteral("0") : QStringLiteral("1"));
            w.writeTextElement(kWpmlNs, QStringLiteral("useGlobalTurnParam"), QStringLiteral("0"));
            // 云台俯仰：有航点级角度时写本点角度（useGlobalGimbalPitch=0），
            // 否则跟随全局（useGlobalGimbalPitch=1，全局角未设置即为 0）
            if (wp.hasGimbalPitch) {
                w.writeTextElement(kWpmlNs, QStringLiteral("useGlobalGimbalPitch"), QStringLiteral("0"));
                w.writeTextElement(kWpmlNs, QStringLiteral("gimbalPitchAngle"), num(wp.gimbalPitchAngle, 1));
            } else {
                w.writeTextElement(kWpmlNs, QStringLiteral("useGlobalGimbalPitch"), QStringLiteral("1"));
            }
            w.writeEndElement(); // Placemark
            ++index;
        }

        w.writeEndElement(); // Folder
    }

    w.writeEndElement(); // Document
    w.writeEndElement(); // kml
    w.writeEndDocument();
    return out;
}

bool DjiWpmlWriter::writeKmz(const Plan& plan, const QString& kmzPath,
                             const DjiWriteOptions& opt, Report& report)
{
    const QByteArray templateXml = buildTemplateXml(plan, opt, report);
    const QByteArray waylinesXml = buildWaylinesXml(plan, opt, report);
    if (templateXml.isEmpty() || waylinesXml.isEmpty())
        return false;

    QFile file(kmzPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        report.error(QStringLiteral("无法写入 kmz 文件: %1").arg(kmzPath));
        return false;
    }
    file.close();

    QList<ziputil::Entry> entries;
    entries.append({ QStringLiteral("template.kml"), templateXml });
    entries.append({ QStringLiteral("waylines.wpml"), waylinesXml });

    QString err;
    if (!ziputil::create(kmzPath, entries, &err)) {
        report.error(QStringLiteral("KMZ 打包失败: %1").arg(err));
        return false;
    }
    return true;
}

} // namespace wpt
