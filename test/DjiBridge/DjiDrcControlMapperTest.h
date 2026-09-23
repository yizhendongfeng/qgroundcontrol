/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include "UnitTest.h"

/// DjiDrcControlMapper 的**纯映射**部分，外加 DjiFlightModeNames 的模式名比对。
///
/// 刻意只测不需要飞机、也不需要碰设置项的那部分：
/// - 不连 MockLink（这些断言与飞机无关，连了反而慢且引入无关变量）
/// - **不改任何设置项**。QSettings 写的是用户真实那份 ini（`DGCS Daily.ini`），
///   测试里翻一下 drcInvertX 等于偷偷改了现场的配置 —— 所以下面遇到 invert 开着的
///   情况是跳过并说明，而不是"先改再改回来"。
///
/// 留在台架上的不变量（需要飞机，本文件故意不覆盖）：
/// - `applyDroneControl` 的三道门禁（无飞机 / 未解锁 / 模式不吃手动）
/// - "归零之后 25 Hz 保活流不能把旧杆量复活" —— 这条依赖真的发出去什么，
///   只能靠真机观测（或临时在 `Vehicle::sendJoystickDataThreadSafe` 里插桩看 x/y/z/r 原值）
///   （见 DjiDrcControlMapper::zeroSticks 的注释）
class DjiDrcControlMapperTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _testThrustZeroPoint();
    void _testThrustEndpointsAndClamp();
    void _testDefaultAxesAreHover();
    void _testHorizontalAxes();
    void _testYawAxis();
    void _testJoystickResultCode();
    void _testFlightModeNameMatch();
    void _testManualControlModeWhitelist();
};
