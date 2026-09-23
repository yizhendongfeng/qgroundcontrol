#include "waypointmodel.h"

namespace wpt {

QString DjiAction::funcName(Func f)
{
    switch (f) {
    case Func::TakePhoto:     return QStringLiteral("takePhoto");
    case Func::StartRecord:   return QStringLiteral("startRecord");
    case Func::StopRecord:    return QStringLiteral("stopRecord");
    case Func::Hover:         return QStringLiteral("hover");
    case Func::GimbalRotate:  return QStringLiteral("gimbalRotate");
    case Func::Zoom:          return QStringLiteral("zoom");
    case Func::CustomDirName: return QStringLiteral("customDirName");
    default:                  return QStringLiteral("unknown");
    }
}

Wayline& Plan::firstWayline()
{
    if (waylines.isEmpty())
        waylines.append(Wayline{});
    return waylines.first();
}

const Wayline& Plan::firstWayline() const
{
    static const Wayline empty;
    return waylines.isEmpty() ? empty : waylines.first();
}

QString Report::toString() const
{
    QStringList lines;
    for (const QString& w : warnings)
        lines << QStringLiteral("  [警告] ") + w;
    for (const QString& e : errors)
        lines << QStringLiteral("  [错误] ") + e;
    return lines.join(QLatin1Char('\n'));
}

} // namespace wpt
