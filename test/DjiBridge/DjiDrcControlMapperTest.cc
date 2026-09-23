/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "DjiDrcControlMapperTest.h"
#include "DjiDrcControlMapper.h"
#include "DjiFlightModeNames.h"
#include "CloudServerSettings.h"
#include "SettingsManager.h"

#include <QtCore/QCoreApplication>
#include <QtTest/QTest>

namespace {

/// 浮点比较。映射里有 0.4 / 0.8 这类二进制不精确的中间值，
/// 直接 QCOMPARE 比 double 会随表达式写法飘。
#define VERIFY_AXIS(actual, expected, what)                                                              \
    do {                                                                                                 \
        const double _actual   = (actual);                                                               \
        const double _expected = (expected);                                                             \
        QVERIFY2(qAbs(_actual - _expected) <= 1e-9,                                                      \
                 qPrintable(QStringLiteral(what "：实际 %1，期望 %2")                                    \
                                .arg(_actual, 0, 'g', 17)                                                \
                                .arg(_expected, 0, 'g', 17)));                                           \
    } while (0)

CloudServerSettings* cloudSettings()
{
    return SettingsManager::instance()->cloudServerSettings();
}

} // namespace

// ---------------------------------------------------------------------------
// 推力：h（升降速度 m/s）→ QGC 的 thrust 轴 [0,1]
// ---------------------------------------------------------------------------

void DjiDrcControlMapperTest::_testThrustZeroPoint()
{
    DjiDrcControlMapper mapper;

    // 本测试存在的理由。thrust 是 [0,1] 且**零点在中位 0.5**，不是 [-1,1] 的 0 ——
    // PX4 把 MANUAL_CONTROL.z 读作 [0,1000]、500 才是悬停，按 [-1,1] 送出的话
    // h=0（中立）会被读成"全速下降"。这条已经判错过一次方向，就是因为没有它。
    VERIFY_AXIS(mapper.mapAxes(0, 0, 0, 0).thrust, 0.5, "h=0（中立）应为悬停 0.5");

    VERIFY_AXIS(mapper.mapAxes(0, 0, 2.5, 0).thrust, 0.75, "h=+2.5（升半程）应为 0.75");
    VERIFY_AXIS(mapper.mapAxes(0, 0,  -2, 0).thrust, 0.25, "h=-2（降半程）应为 0.25");

    // 升 5 降 4 的不对称是协议规定（官方 cloud-sdk 的 DroneControlRequest 校验注解），
    // 不是笔误。写成对称量程的话下面这两条会挂 —— 那正是想要的告警。
    VERIFY_AXIS(mapper.mapAxes(0, 0, 5, 0).thrust, 1.0, "h=+5（升满）应为 1.0");
    VERIFY_AXIS(mapper.mapAxes(0, 0, 4, 0).thrust, 0.9, "h=+4 应为 0.9（升量程是 5，不是 4）");
    VERIFY_AXIS(mapper.mapAxes(0, 0, -4, 0).thrust, 0.0, "h=-4（降满）应为 0.0");
}

void DjiDrcControlMapperTest::_testThrustEndpointsAndClamp()
{
    DjiDrcControlMapper mapper;

    // 超量程必须夹住。QGC 的 sendJoystickDataThreadSafe 会把 thrust ×1000 直通成
    // MAVLink 的 z，超出 [0,1000] 的 z 是未定义行为 —— 不能线性外推出去。
    VERIFY_AXIS(mapper.mapAxes(0, 0,  100, 0).thrust, 1.0, "h=+100 应夹到 1.0");
    VERIFY_AXIS(mapper.mapAxes(0, 0, -100, 0).thrust, 0.0, "h=-100 应夹到 0.0");

    // 夹取发生在**比值**上，所以刚超一点和超很多必须得到同一个值。
    // 量程常数写小一点就会让满杆也到不了 1.0，这条能拦住。
    VERIFY_AXIS(mapper.mapAxes(0, 0, 6, 0).thrust, 1.0, "h=+6 应与 h=+5 同为 1.0");
    VERIFY_AXIS(mapper.mapAxes(0, 0, -5, 0).thrust, 0.0, "h=-5 应与 h=-4 同为 0.0");
}

void DjiDrcControlMapperTest::_testDefaultAxesAreHover()
{
    // Axes 的默认构造值就是"中立杆量"，且推力必须是 0.5 而不是 0 ——
    // zeroSticks()（失权 / 死锁 / 退出 DRC 的收尾）和"没有上一次杆量时的保活重发"
    // 都直接发 Axes{}。谁把 thrust 的默认值改成 0.0，那些收尾动作就都变成
    // "全速下降"，而且每一步都发生在该往下的时机之外。这条断言就是拦它的。
    const DjiDrcControlMapper::Axes neutral;
    VERIFY_AXIS(neutral.roll,   0.0, "默认 roll 应为 0");
    VERIFY_AXIS(neutral.pitch,  0.0, "默认 pitch 应为 0");
    VERIFY_AXIS(neutral.yaw,    0.0, "默认 yaw 应为 0");
    VERIFY_AXIS(neutral.thrust, 0.5, "默认 thrust 应为 0.5（悬停），不是 0");
}

// ---------------------------------------------------------------------------
// 水平轴与偏航轴：量程、夹取、符号
// ---------------------------------------------------------------------------

void DjiDrcControlMapperTest::_testHorizontalAxes()
{
    DjiDrcControlMapper mapper;

    // 量程与夹取（与符号无关，invert 开着也成立）
    VERIFY_AXIS(qAbs(mapper.mapAxes( 17,  17, 0, 0).roll),  1.0, "|x|=17 应到满量程");
    VERIFY_AXIS(qAbs(mapper.mapAxes(-17, -17, 0, 0).roll),  1.0, "|x|=17 应到满量程");
    VERIFY_AXIS(qAbs(mapper.mapAxes( 100, 100, 0, 0).roll), 1.0, "|x|=100 应夹到 1.0");
    VERIFY_AXIS(qAbs(mapper.mapAxes( 8.5, 8.5, 0, 0).roll), 0.5, "|x|=8.5（半程）应为 0.5");
    VERIFY_AXIS(mapper.mapAxes(0, 0, 0, 0).pitch, 0.0, "y=0 应为 0");

    // 符号约定按官方网页端 demo 的注释：x 正 = 左移、y 正 = 前进，
    // 而 QGC 的 roll 正 = 右，所以只有 x→roll 这一路取负。
    //
    // drcInvertX/Y 是给台架实测准备的（社区对 x 的正方向说法不一）。开着的时候
    // 符号就是**故意**反的，这里只能警告不能断言 —— 但也不去改用户的设置项：
    // QSettings 写的是现场那份 ini，测试进程翻一下就等于改了配置。
    if (cloudSettings()->drcInvertX()->rawValue().toBool()) {
        qWarning("drcInvertX 开着，跳过 x→roll 的符号断言（该设置需台架实测确认）");
    } else {
        VERIFY_AXIS(mapper.mapAxes( 17, 0, 0, 0).roll, -1.0, "x=+17（左移）应为 roll=-1.0");
        VERIFY_AXIS(mapper.mapAxes(-17, 0, 0, 0).roll,  1.0, "x=-17（右移）应为 roll=+1.0");
    }

    if (cloudSettings()->drcInvertY()->rawValue().toBool()) {
        qWarning("drcInvertY 开着，跳过 y→pitch 的符号断言（该设置需台架实测确认）");
    } else {
        VERIFY_AXIS(mapper.mapAxes(0,  17, 0, 0).pitch,  1.0, "y=+17（前进）应为 pitch=+1.0");
        VERIFY_AXIS(mapper.mapAxes(0, -17, 0, 0).pitch, -1.0, "y=-17（后退）应为 pitch=-1.0");
    }
}

void DjiDrcControlMapperTest::_testYawAxis()
{
    DjiDrcControlMapper mapper;

    // 量程 w ∈ [-90, 90] °/s，直通不取负（w 正 = 顺时针，与 QGC 的 yaw 正同向）
    VERIFY_AXIS(qAbs(mapper.mapAxes(0, 0, 0,  90).yaw), 1.0, "|w|=90 应到满量程");
    VERIFY_AXIS(qAbs(mapper.mapAxes(0, 0, 0,  45).yaw), 0.5, "|w|=45（半程）应为 0.5");
    VERIFY_AXIS(qAbs(mapper.mapAxes(0, 0, 0, 100).yaw), 1.0, "|w|=100 应夹到 1.0");
    VERIFY_AXIS(mapper.mapAxes(0, 0, 0, 0).yaw, 0.0, "w=0 应为 0");

    if (cloudSettings()->drcInvertW()->rawValue().toBool()) {
        qWarning("drcInvertW 开着，跳过 w→yaw 的符号断言（该设置需台架实测确认）");
    } else {
        VERIFY_AXIS(mapper.mapAxes(0, 0, 0,  90).yaw,  1.0, "w=+90（顺时针）应为 yaw=+1.0");
        VERIFY_AXIS(mapper.mapAxes(0, 0, 0, -90).yaw, -1.0, "w=-90（逆时针）应为 yaw=-1.0");
    }
}

// ---------------------------------------------------------------------------
// 门禁结果 → 上行错误码
// ---------------------------------------------------------------------------

void DjiDrcControlMapperTest::_testJoystickResultCode()
{
    using Result = DjiDrcControlMapper::JoystickResult;

    QCOMPARE(DjiDrcControlMapper::joystickResultCode(Result::Ok), DjiDrcError::kSuccess);
    QCOMPARE(DjiDrcControlMapper::joystickResultCode(Result::Ok), 0);

    // 三种"收到了但没执行"都回 327002。协议里没有更贴切的码，
    // 但这个值在台架上实测过（云端收到的就是它），别改。
    QCOMPARE(DjiDrcError::kObtainControlFailed, 327002);
    QCOMPARE(DjiDrcControlMapper::joystickResultCode(Result::NoVehicle),  DjiDrcError::kObtainControlFailed);
    QCOMPARE(DjiDrcControlMapper::joystickResultCode(Result::NotArmed),   DjiDrcError::kObtainControlFailed);
    QCOMPARE(DjiDrcControlMapper::joystickResultCode(Result::WrongMode),  DjiDrcError::kObtainControlFailed);
}

// ---------------------------------------------------------------------------
// 模式名比对
// ---------------------------------------------------------------------------

void DjiDrcControlMapperTest::_testFlightModeNameMatch()
{
    using namespace DjiFlightModeNames;

    // 英文界面（或该词没被翻译）走字面量相等这条
    QVERIFY(modeIs(QStringLiteral("Position"), "Position"));
    QVERIFY(modeIs(QStringLiteral("Stabilize"), "Stabilize"));

    // 不命中。大小写敏感 —— flightMode() 给的是完整模式名，不是用户输入，
    // 放宽匹配只会让"某个没列进白名单的模式"被误放进来。
    QVERIFY(!modeIs(QStringLiteral("Auto"), "Position"));
    QVERIFY(!modeIs(QStringLiteral("position"), "Position"));
    QVERIFY(!modeIs(QStringLiteral(""), "Position"));

    // 多名字版本
    QVERIFY(modeIsAny(QStringLiteral("Altitude"), {"Position", "Altitude", "Manual"}));
    QVERIFY(!modeIsAny(QStringLiteral("Auto"), {"Position", "Altitude", "Manual"}));

    // 译文那条分支：中文界面下 flightMode() 返回的是译文（"Position" → "定点Position"），
    // 拿英文常量直接比会全不命中 —— 台架上 1726 条 drone_control 就是被这个拒掉的。
    //
    // 这里用同一个 translate() 现算一个译名来验，装没装译文都能跑：
    // 装了验的是真译文，没装就取回原样字面量、退化成上面已经测过的情形。
    const char* const* ctx = contexts();
    for (int i = 0; i < contextCount; ++i) {
        const QString translated = QCoreApplication::translate(ctx[i], "Position");
        QVERIFY2(modeIs(translated, "Position"),
                 qPrintable(QStringLiteral("上下文 %1 下的译名 %2 没被认出来")
                                .arg(QLatin1String(ctx[i]), translated)));
    }
}

// ---------------------------------------------------------------------------
// 吃手动输入的模式白名单（DjiDrcClient::manualControlReady 与门禁都读它）
// ---------------------------------------------------------------------------

void DjiDrcControlMapperTest::_testManualControlModeWhitelist()
{
    auto eats = [](const char* m) { return DjiDrcControlMapper::isManualControlMode(QLatin1String(m)); };

    // 两个固件各来一组：PX4 用 Position/Altitude，ArduPilot 用 Loiter/Altitude Hold。
    QVERIFY2(eats("Position"),       "PX4 Position 必须算吃手动输入 —— 台架验过它接受杆量");
    QVERIFY2(eats("Altitude"),       "PX4 Altitude");
    QVERIFY2(eats("Loiter"),         "ArduPilot Loiter");
    QVERIFY2(eats("Altitude Hold"),  "ArduPilot Altitude Hold");
    QVERIFY2(eats("Stabilize"),      "ArduPilot Stabilize");
    QVERIFY2(eats("Position Hold"),  "ArduPilot PosHold");
    QVERIFY2(eats("Acro"),           "Acro（台架上验过：它同样接受杆量）");

    // **这一条是整个"云端到底控没控住"判据的地基**。
    // Hold 是"刚解锁还没进任何模式"的默认落点，而 PX4 在 Hold 下忽略 MANUAL_CONTROL。
    // 谁把 Hold 加进白名单，manualControlReady 就会在飞机蹲着不动时显示"接受手动"，
    // 而那正是产生台架上 1726 条 `flight mode '等待' ignores manual control` 的场景 ——
    // 界面会反过来骗人，比没有这个判据更糟。
    QVERIFY2(!eats("Hold"), "Hold 绝不能在白名单里：它不吃手动输入，且正是最常见的起点");

    // 自动化模式一律不吃
    QVERIFY(!eats("Mission"));
    QVERIFY(!eats("Auto"));
    QVERIFY(!eats("Return"));
    QVERIFY(!eats("RTL"));
    QVERIFY(!eats("Land"));
    QVERIFY(!eats("Takeoff"));
    QVERIFY(!eats("Guided"));
    QVERIFY2(!eats(""), "空串（还没收到过模式）不能算吃手动输入");
}
