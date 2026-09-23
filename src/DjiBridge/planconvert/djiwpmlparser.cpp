#include "djiwpmlparser.h"

#include <QFile>
#include <QXmlStreamReader>

#include <algorithm>
#include <cmath>

#include "ziputil.h"

namespace wpt {

namespace {

const QString kKmlNs  = QStringLiteral("http://www.opengis.net/kml/2.2");
const QString kWpmlNs = QStringLiteral("http://www.dji.com/wpmz/1.0.2");

bool isElement(QXmlStreamReader& r, const QString& ns, const QString& name)
{
    return r.isStartElement() && r.namespaceUri() == ns && r.name() == name;
}

// QXmlStreamReader::name() 返回带前缀的限定名（如 "wpml:index"），
// 此处取本地名用于比较，避免依赖 DJI 固定前缀。
QString localName(const QXmlStreamReader& r)
{
    const QString qn = r.name().toString();
    const int colon = qn.indexOf(QLatin1Char(':'));
    return colon >= 0 ? qn.mid(colon + 1) : qn;
}

// 读取当前 startElement 的文本内容（跳过子元素），停在 endElement 上。
QString readElementText(QXmlStreamReader& r)
{
    QString text;
    while (!r.atEnd()) {
        r.readNext();
        if (r.isEndElement())
            break;
        if (r.isCharacters() && !r.isCDATA())
            text += r.text();
    }
    return text.trimmed();
}

// 跳过当前元素及其所有子元素
void skipElement(QXmlStreamReader& r)
{
    int depth = 1;
    while (!r.atEnd() && depth > 0) {
        r.readNext();
        if (r.isStartElement())
            ++depth;
        else if (r.isEndElement())
            --depth;
    }
}

// 解析 "lon,lat" / "lon,lat,alt" 形式的 KML 坐标
bool parseCoordinates(const QString& text, double& lon, double& lat, double* alt = nullptr)
{
    const QStringList parts = text.simplified().split(QLatin1Char(','));
    if (parts.size() < 2)
        return false;
    bool ok1 = false, ok2 = false;
    lon = parts.at(0).toDouble(&ok1);
    lat = parts.at(1).toDouble(&ok2);
    if (!ok1 || !ok2)
        return false;
    if (alt && parts.size() >= 3) {
        bool ok3 = false;
        *alt = parts.at(2).toDouble(&ok3);
    }
    return true;
}

double dbl(const QString& s, double def = 0.0)
{
    bool ok = false;
    const double v = s.toDouble(&ok);
    return ok ? v : def;
}

int intv(const QString& s, int def = 0)
{
    bool ok = false;
    const int v = s.toInt(&ok);
    return ok ? v : def;
}

Waypoint::HeadingMode headingModeFromString(const QString& s)
{
    if (s == QStringLiteral("manually"))          return Waypoint::HeadingMode::Manually;
    if (s == QStringLiteral("fixed"))             return Waypoint::HeadingMode::Fixed;
    if (s == QStringLiteral("smoothTransition"))  return Waypoint::HeadingMode::SmoothTransition;
    if (s == QStringLiteral("towardPOI"))         return Waypoint::HeadingMode::TowardPoi;
    return Waypoint::HeadingMode::FollowWayline;
}

Waypoint::TurnMode turnModeFromString(const QString& s)
{
    if (s == QStringLiteral("coordinateTurn"))                          return Waypoint::TurnMode::CoordinateTurn;
    if (s == QStringLiteral("toPointAndStopWithContinuityCurvature"))   return Waypoint::TurnMode::ToPointAndStopContinuity;
    if (s == QStringLiteral("toPointAndPassWithContinuityCurvature"))   return Waypoint::TurnMode::ToPointAndPassContinuity;
    return Waypoint::TurnMode::ToPointAndStopDiscontinuity;
}

} // namespace

// ---------------------------------------------------------------------------
// waylines.wpml 解析
// ---------------------------------------------------------------------------
bool DjiWpmlParser::parseWaylinesXml(const QByteArray& xml, Plan& out, Report& report)
{
    QXmlStreamReader r(xml);
    if (r.readNextStartElement() && localName(r) != QStringLiteral("kml")) {
        report.error(QStringLiteral("waylines.wpml 根元素不是 <kml>"));
        return false;
    }
    if (r.hasError()) {
        report.error(QStringLiteral("waylines.wpml XML 解析失败: %1").arg(r.errorString()));
        return false;
    }

    while (!r.atEnd()) {
        r.readNext();
        if (r.isStartElement()) {
            const QString ns = r.namespaceUri().toString();
            const QString name = localName(r);

            if (ns == kWpmlNs && name == QStringLiteral("missionConfig")) {
                // 解析 missionConfig 子元素
                while (!r.atEnd()) {
                    r.readNext();
                    if (r.isEndElement() && localName(r) == QStringLiteral("missionConfig"))
                        break;
                    if (!r.isStartElement())
                        continue;
                    const QString n = localName(r);
                    if (n == QStringLiteral("flyToWaylineMode"))      out.mission.flyToWaylineMode = readElementText(r);
                    else if (n == QStringLiteral("finishAction"))     out.mission.finishAction = readElementText(r);
                    else if (n == QStringLiteral("exitOnRCLost"))     out.mission.exitOnRCLost = readElementText(r);
                    else if (n == QStringLiteral("executeRCLostAction")) out.mission.executeRCLostAction = readElementText(r);
                    else if (n == QStringLiteral("takeOffSecurityHeight")) out.mission.takeOffSecurityHeight = dbl(readElementText(r));
                    else if (n == QStringLiteral("globalTransitionalSpeed")) out.mission.globalTransitionalSpeed = dbl(readElementText(r));
                    else if (n == QStringLiteral("globalRTHHeight"))  out.mission.globalRTHHeight = dbl(readElementText(r));
                    else if (n == QStringLiteral("takeOffRefPoint"))  out.mission.takeOffRefPoint = readElementText(r);
                    else if (n == QStringLiteral("takeOffRefPointAGLHeight")) out.mission.takeOffRefPointAGLHeight = dbl(readElementText(r));
                    else if (n == QStringLiteral("droneInfo")) {
                        while (!r.atEnd()) {
                            r.readNext();
                            if (r.isEndElement() && localName(r) == QStringLiteral("droneInfo"))
                                break;
                            if (!r.isStartElement())
                                continue;
                            const QString dn = localName(r);
                            if (dn == QStringLiteral("droneEnumValue")) out.mission.droneEnumValue = intv(readElementText(r));
                            else if (dn == QStringLiteral("droneSubEnumValue")) out.mission.droneSubEnumValue = intv(readElementText(r));
                        }
                    } else if (n == QStringLiteral("payloadInfo")) {
                        while (!r.atEnd()) {
                            r.readNext();
                            if (r.isEndElement() && localName(r) == QStringLiteral("payloadInfo"))
                                break;
                            if (!r.isStartElement())
                                continue;
                            const QString pn = localName(r);
                            if (pn == QStringLiteral("payloadEnumValue")) out.mission.payloadEnumValue = intv(readElementText(r));
                            else if (pn == QStringLiteral("payloadPositionIndex")) out.mission.payloadPositionIndex = intv(readElementText(r));
                        }
                    }
                }
            } else if (ns == kKmlNs && name == QStringLiteral("Folder")) {
                // 每个 Folder 是一条航线（倾斜摄影会生成多条，这里逐条保留）
                Wayline wl;
                while (!r.atEnd()) {
                    r.readNext();
                    if (r.isEndElement() && localName(r) == QStringLiteral("Folder"))
                        break;
                    if (!r.isStartElement())
                        continue;
                    const QString n = localName(r);
                    const QString ns2 = r.namespaceUri().toString();

                    if (ns2 == kWpmlNs && n == QStringLiteral("templateId")) {
                        wl.templateId = intv(readElementText(r));
                    } else if (ns2 == kWpmlNs && n == QStringLiteral("waylineId")) {
                        wl.waylineId = intv(readElementText(r));
                    } else if (ns2 == kWpmlNs && n == QStringLiteral("executeHeightMode")) {
                        wl.executeHeightMode = readElementText(r);
                    } else if (ns2 == kWpmlNs && n == QStringLiteral("autoFlightSpeed")) {
                        wl.autoFlightSpeed = dbl(readElementText(r), 5.0);
                    } else if (ns2 == kKmlNs && n == QStringLiteral("Placemark")) {
                        Waypoint wp;
                        // 先按 index 找到航点位置：index 元素可能出现在任意位置，先整体收集
                        while (!r.atEnd()) {
                            r.readNext();
                            if (r.isEndElement() && localName(r) == QStringLiteral("Placemark"))
                                break;
                            if (!r.isStartElement())
                                continue;
                            const QString cn = localName(r);
                            const QString cns = r.namespaceUri().toString();
                            if (cns == kWpmlNs && cn == QStringLiteral("index")) {
                                wp.index = intv(readElementText(r));
                            } else if (cns == kWpmlNs && cn == QStringLiteral("executeHeight")) {
                                wp.executeHeight = dbl(readElementText(r));
                            } else if (cns == kWpmlNs && cn == QStringLiteral("waypointSpeed")) {
                                wp.hasLocalSpeed = true;
                                wp.speed = dbl(readElementText(r));
                            } else if (cns == kWpmlNs && cn == QStringLiteral("waypointHeadingParam")) {
                                while (!r.atEnd()) {
                                    r.readNext();
                                    if (r.isEndElement() && localName(r) == QStringLiteral("waypointHeadingParam"))
                                        break;
                                    if (!r.isStartElement())
                                        continue;
                                    const QString hn = localName(r);
                                    const QString ht = readElementText(r);
                                    if (hn == QStringLiteral("waypointHeadingMode")) {
                                        wp.headingMode = headingModeFromString(ht);
                                    } else if (hn == QStringLiteral("waypointHeadingAngle")) {
                                        wp.headingAngle = dbl(ht);
                                    } else if (hn == QStringLiteral("waypointPoiPoint")) {
                                        // DJI 规范：waypointPoiPoint 格式为 "纬度,经度,高度"
                                        const QStringList poiParts = ht.simplified().split(QLatin1Char(','));
                                        if (poiParts.size() >= 2) {
                                            bool okLat = false, okLon = false;
                                            const double lat = poiParts.at(0).toDouble(&okLat);
                                            const double lon = poiParts.at(1).toDouble(&okLon);
                                            if (okLat && okLon) {
                                                wp.hasPoi = true;
                                                wp.poiLat = lat;
                                                wp.poiLon = lon;
                                                wp.poiAlt = poiParts.size() >= 3 ? poiParts.at(2).toDouble() : 0.0;
                                            }
                                        }
                                    }
                                }
                            } else if (cns == kWpmlNs && cn == QStringLiteral("waypointTurnParam")) {
                                while (!r.atEnd()) {
                                    r.readNext();
                                    if (r.isEndElement() && localName(r) == QStringLiteral("waypointTurnParam"))
                                        break;
                                    if (!r.isStartElement())
                                        continue;
                                    const QString tn = localName(r);
                                    const QString tt = readElementText(r);
                                    if (tn == QStringLiteral("waypointTurnMode")) {
                                        wp.turnMode = turnModeFromString(tt);
                                    } else if (tn == QStringLiteral("waypointTurnDampingDist")) {
                                        wp.turnDampingDist = dbl(tt);
                                    }
                                }
                            } else if (cns == kWpmlNs && cn == QStringLiteral("actionGroup")) {
                                // 解析动作组（触发类型 + 动作列表）
                                QString triggerType;
                                double triggerParam = -1.0;
                                int startIndex = -1, endIndex = -1;
                                QVector<DjiAction> acts;
                                while (!r.atEnd()) {
                                    r.readNext();
                                    if (r.isEndElement() && localName(r) == QStringLiteral("actionGroup"))
                                        break;
                                    if (!r.isStartElement())
                                        continue;
                                    const QString an = localName(r);
                                    if (an == QStringLiteral("actionTriggerType")) {
                                        triggerType = readElementText(r);
                                    } else if (an == QStringLiteral("actionTriggerParam")) {
                                        triggerParam = dbl(readElementText(r), -1.0);
                                    } else if (an == QStringLiteral("actionGroupStartIndex")) {
                                        startIndex = intv(readElementText(r), -1);
                                    } else if (an == QStringLiteral("actionGroupEndIndex")) {
                                        endIndex = intv(readElementText(r), -1);
                                    } else if (an == QStringLiteral("action")) {
                                        DjiAction act;
                                        while (!r.atEnd()) {
                                            r.readNext();
                                            if (r.isEndElement() && localName(r) == QStringLiteral("action"))
                                                break;
                                            if (!r.isStartElement())
                                                continue;
                                            const QString acn = localName(r);
                                            if (acn == QStringLiteral("actionActuatorFunc")) {
                                                const QString fn = readElementText(r);
                                                if (fn == QStringLiteral("takePhoto")) act.func = DjiAction::Func::TakePhoto;
                                                else if (fn == QStringLiteral("startRecord")) act.func = DjiAction::Func::StartRecord;
                                                else if (fn == QStringLiteral("stopRecord")) act.func = DjiAction::Func::StopRecord;
                                                else if (fn == QStringLiteral("hover")) act.func = DjiAction::Func::Hover;
                                                else if (fn == QStringLiteral("gimbalRotate")) act.func = DjiAction::Func::GimbalRotate;
                                                else if (fn == QStringLiteral("zoom")) act.func = DjiAction::Func::Zoom;
                                                else if (fn == QStringLiteral("customDirName")) act.func = DjiAction::Func::CustomDirName;
                                                else act.func = DjiAction::Func::Unknown;
                                            } else if (acn == QStringLiteral("actionActuatorFuncParam")) {
                                                while (!r.atEnd()) {
                                                    r.readNext();
                                                    if (r.isEndElement() && localName(r) == QStringLiteral("actionActuatorFuncParam"))
                                                        break;
                                                    if (!r.isStartElement())
                                                        continue;
                                                    const QString pn = localName(r);
                                                    const QString pt = readElementText(r);
                                                    if (pn == QStringLiteral("fileSuffix")) act.fileSuffix = pt;
                                                    else if (pn == QStringLiteral("hoverTime")) act.hoverTime = dbl(pt);
                                                    else if (pn == QStringLiteral("gimbalPitchRotateAngle")) act.gimbalPitchAngle = dbl(pt);
                                                    else if (pn == QStringLiteral("gimbalYawRotateAngle")) act.gimbalYawAngle = dbl(pt);
                                                    else if (pn == QStringLiteral("gimbalRotateTime")) act.gimbalRotateTime = dbl(pt);
                                                    else if (pn == QStringLiteral("focalLength")) act.focalLength = dbl(pt);
                                                }
                                            }
                                        }
                                        acts.append(act);
                                    }
                                }

                                // 关联到航点
                                if (triggerType == QStringLiteral("multipleDistance") && triggerParam > 0.0) {
                                    wp.photoDistanceInterval = triggerParam;
                                } else if (triggerType == QStringLiteral("multipleTiming") && triggerParam > 0.0) {
                                    wp.photoTimeInterval = triggerParam;
                                } else if (triggerType == QStringLiteral("reachPoint")) {
                                    for (const DjiAction& a : acts) {
                                        if (a.func != DjiAction::Func::Unknown)
                                            wp.actions.append(a);
                                    }
                                } else if (triggerType == QStringLiteral("betweenAdjacentPoints")) {
                                    report.warn(QStringLiteral("航段均匀转动云台动作暂不支持，已忽略"));
                                }
                            } else if (cns == kKmlNs && cn == QStringLiteral("Point")) {
                                // coordinates 在 Point 内
                                while (!r.atEnd()) {
                                    r.readNext();
                                    if (r.isEndElement() && localName(r) == QStringLiteral("Point"))
                                        break;
                                    if (r.isStartElement() && localName(r) == QStringLiteral("coordinates")) {
                                        const QString c = readElementText(r);
                                        double lon = 0, lat = 0;
                                        if (parseCoordinates(c, lon, lat)) {
                                            wp.lon = lon;
                                            wp.lat = lat;
                                        } else {
                                            report.warn(QStringLiteral("航点坐标解析失败: \"%1\"").arg(c));
                                        }
                                    }
                                }
                            }
                        }
                        wl.waypoints.append(wp);
                    }
                }
                out.waylines.append(wl);
            }
        }
    }

    if (r.hasError()) {
        report.error(QStringLiteral("waylines.wpml 解析错误: %1").arg(r.errorString()));
        return false;
    }

    if (out.waylines.isEmpty()) {
        report.error(QStringLiteral("waylines.wpml 中没有 <Folder> 航线"));
        return false;
    }

    // 航点按 index 排序（解析顺序应与 index 一致，防御性排序）
    for (Wayline& wl : out.waylines) {
        std::stable_sort(wl.waypoints.begin(), wl.waypoints.end(),
                         [](const Waypoint& a, const Waypoint& b) { return a.index < b.index; });
        // 高度参考系
        if (wl.executeHeightMode == QStringLiteral("WGS84")) {
            out.templateHeightMode = QStringLiteral("EGM96");
        } else {
            out.templateHeightMode = QStringLiteral("relativeToStartPoint");
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// template.kml 解析（补充椭球高/编辑高度、云台角、模板级参数）
// ---------------------------------------------------------------------------
bool DjiWpmlParser::parseTemplateXml(const QByteArray& xml, Plan& out, Report& report)
{
    QXmlStreamReader r(xml);
    if (r.readNextStartElement() && localName(r) != QStringLiteral("kml")) {
        report.warn(QStringLiteral("template.kml 根元素不是 <kml>，跳过模板解析"));
        return true;
    }

    // 按航点 index 填充 template 属性（waylines 已解析时）
    auto findWaypoint = [&out](int idx, Waypoint** wpOut) -> bool {
        for (Wayline& wl : out.waylines) {
            for (Waypoint& wp : wl.waypoints) {
                if (wp.index == idx) {
                    *wpOut = &wp;
                    return true;
                }
            }
        }
        return false;
    };

    bool hasTemplateWaypoints = false;
    QVector<Waypoint> templateWaypoints; // 当 waylines 为空时用

    while (!r.atEnd()) {
        r.readNext();
        if (!r.isStartElement())
            continue;
        const QString ns = r.namespaceUri().toString();
        const QString name = localName(r);

        if (ns == kWpmlNs && name == QStringLiteral("missionConfig")) {
            while (!r.atEnd()) {
                r.readNext();
                if (r.isEndElement() && localName(r) == QStringLiteral("missionConfig"))
                    break;
                if (!r.isStartElement())
                    continue;
                const QString n = localName(r);
                if (n == QStringLiteral("finishAction")) out.mission.finishAction = readElementText(r);
                else if (n == QStringLiteral("takeOffSecurityHeight")) out.mission.takeOffSecurityHeight = dbl(readElementText(r));
                else if (n == QStringLiteral("globalTransitionalSpeed")) out.mission.globalTransitionalSpeed = dbl(readElementText(r));
                else if (n == QStringLiteral("globalRTHHeight")) out.mission.globalRTHHeight = dbl(readElementText(r));
                else if (n == QStringLiteral("flyToWaylineMode")) out.mission.flyToWaylineMode = readElementText(r);
                else if (n == QStringLiteral("exitOnRCLost")) out.mission.exitOnRCLost = readElementText(r);
                else if (n == QStringLiteral("executeRCLostAction")) out.mission.executeRCLostAction = readElementText(r);
            }
        } else if (ns == kKmlNs && name == QStringLiteral("Folder")) {
            while (!r.atEnd()) {
                r.readNext();
                if (r.isEndElement() && localName(r) == QStringLiteral("Folder"))
                    break;
                if (!r.isStartElement())
                    continue;
                const QString n = localName(r);
                const QString ns2 = r.namespaceUri().toString();

                if (ns2 == kWpmlNs && n == QStringLiteral("templateType")) {
                    // 仅支持 waypoint 模板
                } else if (ns2 == kWpmlNs && n == QStringLiteral("autoFlightSpeed")) {
                    const double spd = dbl(readElementText(r), 5.0);
                    if (!out.waylines.isEmpty())
                        out.firstWayline().autoFlightSpeed = spd;
                } else if (ns2 == kWpmlNs && n == QStringLiteral("gimbalPitchMode")) {
                    out.gimbalPitchMode = readElementText(r);
                } else if (ns2 == kWpmlNs && n == QStringLiteral("globalWaypointTurnMode")) {
                    out.globalTurnMode = turnModeFromString(readElementText(r));
                } else if (ns2 == kWpmlNs && n == QStringLiteral("globalUseStraightLine")) {
                    out.globalUseStraightLine = intv(readElementText(r));
                } else if (ns2 == kWpmlNs && n == QStringLiteral("globalWaypointHeadingParam")) {
                    while (!r.atEnd()) {
                        r.readNext();
                        if (r.isEndElement() && localName(r) == QStringLiteral("globalWaypointHeadingParam"))
                            break;
                        if (!r.isStartElement())
                            continue;
                        const QString hn = localName(r);
                        const QString ht = readElementText(r);
                        if (hn == QStringLiteral("waypointHeadingMode")) out.globalHeadingMode = headingModeFromString(ht);
                        else if (hn == QStringLiteral("waypointHeadingAngle")) out.globalHeadingAngle = dbl(ht);
                    }
                } else if (ns2 == kWpmlNs && n == QStringLiteral("waylineCoordinateSysParam")) {
                    while (!r.atEnd()) {
                        r.readNext();
                        if (r.isEndElement() && localName(r) == QStringLiteral("waylineCoordinateSysParam"))
                            break;
                        if (!r.isStartElement())
                            continue;
                        const QString pn = localName(r);
                        const QString pt = readElementText(r);
                        if (pn == QStringLiteral("heightMode")) out.templateHeightMode = pt;
                        else if (pn == QStringLiteral("surfaceFollowModeEnable")) out.surfaceFollowModeEnable = intv(pt);
                        else if (pn == QStringLiteral("surfaceRelativeHeight")) out.surfaceRelativeHeight = dbl(pt);
                    }
                } else if (ns2 == kKmlNs && n == QStringLiteral("Placemark")) {
                    Waypoint wp;
                    bool useGlobalGimbalPitch = false; // 缺省：本点角度优先
                    bool hasGimbalAngle = false;
                    double gimbalAngle = 0.0;
                    while (!r.atEnd()) {
                        r.readNext();
                        if (r.isEndElement() && localName(r) == QStringLiteral("Placemark"))
                            break;
                        if (!r.isStartElement())
                            continue;
                        const QString cn = localName(r);
                        const QString cns = r.namespaceUri().toString();
                        if (cns == kWpmlNs && cn == QStringLiteral("index")) {
                            wp.index = intv(readElementText(r));
                        } else if (cns == kWpmlNs && cn == QStringLiteral("ellipsoidHeight")) {
                            wp.ellipsoidHeight = dbl(readElementText(r));
                        } else if (cns == kWpmlNs && cn == QStringLiteral("height")) {
                            wp.height = dbl(readElementText(r));
                        } else if (cns == kWpmlNs && cn == QStringLiteral("useGlobalHeight")) {
                            wp.useGlobalHeight = (intv(readElementText(r)) == 1);
                        } else if (cns == kWpmlNs && cn == QStringLiteral("useGlobalSpeed")) {
                            // 0 表示局部速度（waypointSpeed 在本模板中给出）
                        } else if (cns == kWpmlNs && cn == QStringLiteral("useGlobalGimbalPitch")) {
                            useGlobalGimbalPitch = (intv(readElementText(r)) == 1);
                        } else if (cns == kWpmlNs && cn == QStringLiteral("useGlobalHeadingParam")) {
                            wp.useGlobalHeadingParam = (intv(readElementText(r)) == 1);
                        } else if (cns == kWpmlNs && cn == QStringLiteral("gimbalPitchAngle")) {
                            hasGimbalAngle = true;
                            gimbalAngle = dbl(readElementText(r));
                        } else if (cns == kKmlNs && cn == QStringLiteral("Point")) {
                            while (!r.atEnd()) {
                                r.readNext();
                                if (r.isEndElement() && localName(r) == QStringLiteral("Point"))
                                    break;
                                if (r.isStartElement() && localName(r) == QStringLiteral("coordinates")) {
                                    const QString c = readElementText(r);
                                    double lon = 0, lat = 0;
                                    if (parseCoordinates(c, lon, lat)) {
                                        wp.lon = lon;
                                        wp.lat = lat;
                                    }
                                }
                            }
                        }
                    }
                    // 云台角语义：useGlobalGimbalPitch=1 表示跟随全局角，不视为航点级设置
                    if (hasGimbalAngle && !useGlobalGimbalPitch) {
                        wp.hasGimbalPitch = true;
                        wp.gimbalPitchAngle = gimbalAngle;
                    }
                    templateWaypoints.append(wp);
                    hasTemplateWaypoints = true;
                }
            }
        }
    }

    // 若 waylines 已存在：用模板补充高度/云台信息
    if (!out.waylines.isEmpty()) {
        for (const Waypoint& twp : templateWaypoints) {
            Waypoint* dst = nullptr;
            if (findWaypoint(twp.index, &dst)) {
                // 偏航：useGlobalHeadingParam=1 时航点级偏航设置不生效（跟随全局）
                if (twp.useGlobalHeadingParam) {
                    dst->headingMode = Waypoint::HeadingMode::FollowWayline;
                    dst->headingAngle = 0.0;
                    dst->hasPoi = false;
                }
                if (!dst->hasGimbalPitch && twp.hasGimbalPitch) {
                    dst->hasGimbalPitch = twp.hasGimbalPitch;
                    dst->gimbalPitchAngle = twp.gimbalPitchAngle;
                }
                if (dst->ellipsoidHeight == 0.0 && twp.ellipsoidHeight != 0.0)
                    dst->ellipsoidHeight = twp.ellipsoidHeight;
                if (dst->height == 0.0 && twp.height != 0.0)
                    dst->height = twp.height;
            }
        }
    } else if (hasTemplateWaypoints) {
        // 只有模板：构造一条航线
        Wayline& line = out.firstWayline();
        line.waypoints = templateWaypoints;
        std::stable_sort(line.waypoints.begin(), line.waypoints.end(),
                         [](const Waypoint& a, const Waypoint& b) { return a.index < b.index; });
        // 模板高度语义 -> 执行高度
        if (out.templateHeightMode == QStringLiteral("relativeToStartPoint")) {
            line.executeHeightMode = QStringLiteral("relativeToStartPoint");
            for (Waypoint& wp : line.waypoints) {
                wp.executeHeight = wp.height;
                if (wp.ellipsoidHeight == 0.0)
                    wp.ellipsoidHeight = wp.height;
            }
        } else {
            line.executeHeightMode = QStringLiteral("WGS84");
            for (Waypoint& wp : line.waypoints) {
                wp.executeHeight = wp.ellipsoidHeight;
                if (wp.height == 0.0)
                    wp.height = wp.ellipsoidHeight;
            }
        }
    }

    return true;
}

// ---------------------------------------------------------------------------
// .kmz 文件解析
// ---------------------------------------------------------------------------
bool DjiWpmlParser::parseFile(const QString& kmzPath, Plan& out, Report& report)
{
    QList<ziputil::Entry> entries;
    QString err;
    if (!ziputil::open(kmzPath, entries, &err)) {
        report.error(QStringLiteral("无法打开 kmz 文件: %1 (%2)").arg(kmzPath, err));
        return false;
    }

    QString waylinesEntry, templateEntry;
    for (const ziputil::Entry& e : entries) {
        if (e.name.endsWith(QStringLiteral("waylines.wpml"), Qt::CaseInsensitive))
            waylinesEntry = e.name;
        else if (e.name.endsWith(QStringLiteral("template.kml"), Qt::CaseInsensitive))
            templateEntry = e.name;
    }

    if (waylinesEntry.isEmpty() && templateEntry.isEmpty()) {
        report.error(QStringLiteral("kmz 中既没有 waylines.wpml 也没有 template.kml"));
        return false;
    }

    auto findData = [&entries](const QString& name) -> QByteArray {
        for (const ziputil::Entry& e : entries)
            if (e.name == name)
                return e.data;
        return {};
    };

    out = Plan{};
    out.source = QStringLiteral("DJI WPML");

    if (!waylinesEntry.isEmpty()) {
        const QByteArray data = findData(waylinesEntry);
        if (data.isEmpty()) {
            report.error(QStringLiteral("waylines.wpml 内容为空"));
            return false;
        }
        if (!parseWaylinesXml(data, out, report))
            return false;
    }

    if (!templateEntry.isEmpty()) {
        const QByteArray data = findData(templateEntry);
        if (!data.isEmpty())
            parseTemplateXml(data, out, report); // 补充信息，失败仅警告
    }

    if (out.waylines.isEmpty() || out.firstWayline().waypoints.isEmpty()) {
        report.error(QStringLiteral("解析后没有可用的航点"));
        return false;
    }

    // 全局参数：从 missionConfig / 模板补齐
    out.hoverSpeed = out.firstWayline().autoFlightSpeed;
    out.cruiseSpeed = out.firstWayline().autoFlightSpeed;
    out.globalHeight = out.firstWayline().waypoints.first().height;
    out.globalEllipsoidHeight = out.firstWayline().waypoints.first().ellipsoidHeight;

    // 从起飞参考点（DJI 格式 "lon,lat,height"）恢复 QGC 起始位置
    if (!out.mission.takeOffRefPoint.isEmpty()) {
        const QStringList parts = out.mission.takeOffRefPoint.split(QLatin1Char(','));
        if (parts.size() >= 2) {
            bool okLon = false, okLat = false;
            const double lon = parts.at(0).trimmed().toDouble(&okLon);
            const double lat = parts.at(1).trimmed().toDouble(&okLat);
            if (okLon && okLat) {
                out.hasPlannedHome = true;
                out.homeLat = lat;
                out.homeLon = lon;
                out.homeAltAmsl = parts.size() >= 3 ? parts.at(2).trimmed().toDouble() : 0.0;
            }
        }
    }

    return true;
}

} // namespace wpt
