/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include <QtCore/QCoreApplication>
#include <QtCore/QLatin1String>
#include <QtCore/QString>

#include <initializer_list>

/// Vehicle::flightMode() 返回的是**给人看的**名字，不是协议常量：
/// FirmwarePlugin 里每个模式名都被 tr() 包过（见 PX4FirmwarePlugin.cc:35-52 与
/// ArduCopterFirmwarePlugin.h:79-84），中文界面下 "Position" 就是 "定点Position"、
/// "Altitude" 就是 "高度"。所以拿英文常量直接和它比，在任何非英文界面下都全不命中 ——
/// 中文界面里 DRC 会一条 drone_control 都发不出去，跟飞机状态无关。
///
/// 这里按插件自己的 tr() 上下文把英文名再翻一遍来比：界面语言变了，比对跟着变。
/// 上下文名就是那些类的类名（tr() 的上下文默认取类名），改名会静默失效，
/// 所以改动那两个类名时记得回来同步。
namespace DjiFlightModeNames {

/// tr() 上下文名 = 那个类的类名。提成常量是因为散在代码里的字面量在类改名时会
/// 静默失效，而这里的失效方式是"模式比对全不命中、且不报错"。
inline constexpr const char* kPx4Context = "PX4FirmwarePlugin";
inline constexpr const char* kApmContext = "ArduCopterFirmwarePlugin";

/// 模式名可能出自这两个插件（DRC 只对多旋翼有意义，所以没有 ArduPlane 等）
inline const char* const* contexts()
{
    static const char* const kContexts[] = {
        kPx4Context,
        kApmContext,
    };
    return kContexts;
}
inline constexpr int contextCount = 2;

/// actual 是否等于 englishName 本身（英文界面，或该词没被翻译），
/// 或等于它在任一插件上下文下的译文
inline bool modeIs(const QString& actual, const char* englishName)
{
    if (actual == QLatin1String(englishName)) {
        return true;
    }
    const char* const* ctx = contexts();
    for (int i = 0; i < contextCount; ++i) {
        if (actual == QCoreApplication::translate(ctx[i], englishName)) {
            return true;
        }
    }
    return false;
}

/// modeIs 的多名字版本：任一命中即可
inline bool modeIsAny(const QString& actual, std::initializer_list<const char*> englishNames)
{
    for (const char* name : englishNames) {
        if (modeIs(actual, name)) {
            return true;
        }
    }
    return false;
}

} // namespace DjiFlightModeNames
