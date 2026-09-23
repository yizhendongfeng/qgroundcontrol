/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "DjiDrcControlMapper.h"

#include <QtCore/QDebug>
// QGeoCoordinate 在 QtPositioning 里，不在 QtCore —— 写成 QtCore/... 会 C1083
#include <QtPositioning/QGeoCoordinate>
#include <QtCore/QStringList>
#include <QtCore/qmath.h>

#include "SettingsManager.h"
#include "CloudServerSettings.h"
#include "DjiFlightModeNames.h"
#include "Vehicle.h"
#include "QGCCameraManager.h"
#include "MavlinkCameraControl.h"
#include "GimbalController.h"
#include "Gimbal.h"

namespace {

// ---------- drone_control 的量程与方向 ----------
//
// 量程来自官方 cloud-sdk 的校验注解（DroneControlRequest.java）：
//   x ∈ [-17, 17] m/s   y ∈ [-17, 17] m/s   h ∈ [-4, 5] m/s   w ∈ [-90, 90] °/s
// h 的上限是 5、下限是 4 —— 上升比下降快，这是协议本身的规定，不是笔误。
constexpr double kMaxHorizSpeed   = 17.0;
constexpr double kMaxClimbSpeed   = 5.0;
constexpr double kMaxDescendSpeed = 4.0;
constexpr double kMaxYawRate      = 90.0;

// 方向约定按官方网页端 demo 的注释（Cloud-API-Demo-Web/src/types/drc.ts）：
//   x 正 = A 键（左移）   y 正 = W 键（前进）   h 正 = 上升   w 正 = 顺时针
// 翻成 QGC 的摇杆轴（roll 正=右、pitch 正=前、thrust 正=上、yaw 正=顺时针）：
// 只有 x→roll 这一路要取负，其余直通。
//
// 社区实现里对 x 的正方向有说"左"也有说"右"的，判据只能是台架实测 ——
// 设置项 drcInvertX/Y/W 就是给实测准备的：挂上飞机打单轴指令，看往哪边动。

// 云台俯仰限位（DJI 常见机型 -90°~+30°）。camera_aim / camera_look_at 算出来的
// 角度会夹在这个区间里，免得把云台怼到限位上。
constexpr double kMinGimbalPitch = -90.0;
constexpr double kMaxGimbalPitch = 30.0;

double clampAxis(double v) { return qBound(-1.0, v, 1.0); }

// 推力零点 —— **不是 0**。
//
// PX4 把 MANUAL_CONTROL.z 读作 [0,1000]，500 才是悬停。接收侧 v1.16
// mavlink_receiver.cpp 做的是 throttle = (z/1000)*2 - 1；ManualControlSetpoint.msg
// 的原话是 "QGC sends throttle/z in range [0,1000] - [0,1]"；配套提交 80ea3a0 把该
// 字段从 [-1000,1000] 改成了 [0,1000]。
//
// QGC 全栈都按 [0,1] 送：Joystick.cc 的手柄路径把油门折算进 [0,1]，屏幕虚拟摇杆的
// 左杆本身就是 [0,1]、居中 0.5。所以 [0,1] 是这条链路的约定，按 [-1,1] 送出的 z 会被
// 飞控读成"中立即全速下降"。
constexpr double kThrottleNeutral  = 0.5;
constexpr double kThrottleHalfSpan = 0.5;

/// 升降速度 h（m/s）→ QGC 的 thrust 轴 [0,1]，0.5 = 悬停。
/// 升 5 降 4 的不对称量程是协议规定，见上面的 kMaxClimbSpeed / kMaxDescendSpeed。
double climbRateToThrust(double h)
{
    return kThrottleNeutral
         + kThrottleHalfSpan * clampAxis(h >= 0 ? h / kMaxClimbSpeed : h / kMaxDescendSpeed);
}

} // namespace

DjiDrcControlMapper::DjiDrcControlMapper(QObject* parent) :
    QObject(parent)
{
}

bool DjiDrcControlMapper::isManualControlMode(const QString& mode)
{
    // ArduPilot Copter: Stabilize / Altitude Hold / Loiter / Position Hold / Brake / Sport / Drift / Acro
    // PX4: Manual / Stabilized / Acro / Rattitude / Altitude / Position
    //
    // **不含 Hold**：PX4 在 Hold 下忽略 MANUAL_CONTROL，而它正是"刚解锁还没进入
    // 任何模式"的默认落点 —— 换句话说，云端刚拿到控制权、一条杆量都还没进去时，
    // 飞机多半就蹲在这里。DjiDrcClient 的 manualControlReady 直接读这张表：
    // 为 false 时工具栏显红字、面板出警告行。
    return DjiFlightModeNames::modeIsAny(mode, {
        "Stabilize", "Altitude Hold", "Loiter", "Position Hold", "Brake", "Sport", "Drift",
        "Manual", "Stabilized", "Rattitude", "Altitude", "Position", "Acro",
    });
}

void DjiDrcControlMapper::setVehicle(Vehicle* vehicle)
{
    if (_vehicle == vehicle) {
        return;
    }
    if (_vehicle) {
        disconnect(_vehicle, nullptr, this, nullptr);
    }
    _vehicle = vehicle;

    if (_vehicle) {
        // 飞行终止的 ACK 不等（急停优先保证低时延），但结果要落日志 ——
        // 飞控不支持这条指令时，只有这里能看出来。
        connect(_vehicle, &Vehicle::mavCommandResult, this,
                [](int, int, int command, int ackResult, int failureCode) {
                    if (command == MAV_CMD_DO_FLIGHTTERMINATION) {
                        qWarning() << "[DjiDrc] flight termination ack:" << ackResult
                                   << "failureCode:" << failureCode;
                    }
                });
    }
}

// ---------------------------------------------------------------------------
// 飞行控制
// ---------------------------------------------------------------------------

DjiDrcControlMapper::Axes DjiDrcControlMapper::mapAxes(double x, double y, double h, double w) const
{
    // 不能用 const 指针：DEFINE_SETTINGFACT 生成的访问器不是 const 成员函数
    CloudServerSettings* settings = SettingsManager::instance()->cloudServerSettings();
    if (settings->drcInvertX()->rawValue().toBool()) { x = -x; }
    if (settings->drcInvertY()->rawValue().toBool()) { y = -y; }
    if (settings->drcInvertW()->rawValue().toBool()) { w = -w; }

    Axes axes;
    // x 正 = 左移，QGC 的 roll 正 = 右，所以取负
    axes.roll   = clampAxis(-x / kMaxHorizSpeed);
    axes.pitch  = clampAxis(y / kMaxHorizSpeed);
    axes.yaw    = clampAxis(w / kMaxYawRate);
    axes.thrust = climbRateToThrust(h);
    return axes;
}

DjiDrcControlMapper::JoystickResult DjiDrcControlMapper::applyDroneControl(double x, double y, double h, double w)
{
    if (!_vehicle) {
        _lastError = QStringLiteral("no active vehicle");
        return JoystickResult::NoVehicle;
    }

    if (!_vehicle->armed()) {
        _lastError = QStringLiteral("vehicle not armed");
        return JoystickResult::NotArmed;
    }

    const QString mode = _vehicle->flightMode();
    if (!isManualControlMode(mode)) {
        // 见 isManualControlMode 的注释：这种模式下发了也不会动
        _lastError = QStringLiteral("flight mode '%1' ignores manual control").arg(mode);
        return JoystickResult::WrongMode;
    }

    _lastAxes     = mapAxes(x, y, h, w);
    _haveLastAxes = true;
    sendRaw(_lastAxes);
    return JoystickResult::Ok;
}

void DjiDrcControlMapper::sendRaw(const Axes& axes)
{
    _vehicle->sendJoystickDataThreadSafe(static_cast<float>(axes.roll),
                                        static_cast<float>(axes.pitch),
                                        static_cast<float>(axes.yaw),
                                        static_cast<float>(axes.thrust),
                                        0);
}

void DjiDrcControlMapper::resendLastAxes()
{
    if (!_vehicle) {
        return;
    }
    if (!_haveLastAxes) {
        // 没有"上一次"可发就发中立值，而不是什么都不发：这条流的作用是让 PX4
        // 始终看得到手动输入源，断流本身就有后果。
        // 注意**不能**把它记成"上一次" —— 中立是"没有指令"的占位，不是一条指令。
        sendRaw(Axes{});
        return;
    }
    sendRaw(_lastAxes);
}

void DjiDrcControlMapper::zeroSticks()
{
    if (!_vehicle) {
        return;
    }
    // 推力的"归零"（松杆）是 kThrottleNeutral，不是 0.0 —— roll/pitch/yaw 归零到 0 是
    // 对的，推力照抄就会发成"全速下降"。而这条路径正是失权 / 死锁 / 退出 DRC 走的收尾，
    // 每一步都在把飞机往下压。详见 kThrottleNeutral 的注释。
    sendRaw(Axes{});
    // 清掉"上一次杆量"：归零之后就不该再有任何东西把旧指令重发出去。
    // 漏掉这一步的话，25 Hz 保活流会在 40ms 内把刚归零的那一杆原样发回去，
    // 死锁保护就形同虚设。
    _haveLastAxes = false;
    _lastAxes     = Axes{};
}

int DjiDrcControlMapper::joystickResultCode(JoystickResult result)
{
    switch (result) {
    case JoystickResult::Ok:
        return DjiDrcError::kSuccess;
    case JoystickResult::NoVehicle:
    case JoystickResult::NotArmed:
    case JoystickResult::WrongMode:
        // 协议里没有更贴切的码："没拿到控制" 这个语义最接近
        return DjiDrcError::kObtainControlFailed;
    }
    return DjiDrcError::kDrcAbnormal;
}

bool DjiDrcControlMapper::emergencyStop()
{
    if (!_vehicle) {
        _lastError = QStringLiteral("no active vehicle");
        return false;
    }

    // MAV_CMD_DO_FLIGHTTERMINATION（param1=1 终止飞行）。ArduPilot 支持；
    // PX4 是否支持需实测（ACK 结果会打到日志里）。这是不可逆的空中停桨，
    // 调用方必须先做二次确认 —— 见 DjiDrcClient::handleEmergencyStop。
    qWarning() << "[DjiDrc] EMERGENCY STOP: sending MAV_CMD_DO_FLIGHTTERMINATION";
    _vehicle->sendMavCommand(_vehicle->defaultComponentId(), MAV_CMD_DO_FLIGHTTERMINATION, true, 1.0f);
    return true;
}

// ---------------------------------------------------------------------------
// 负载控制
// ---------------------------------------------------------------------------

MavlinkCameraControl* DjiDrcControlMapper::currentCamera() const
{
    if (!_vehicle || !_vehicle->cameraManager()) {
        return nullptr;
    }
    return _vehicle->cameraManager()->currentCameraInstance();
}

int DjiDrcControlMapper::cameraPhotoTake()
{
    MavlinkCameraControl* camera = currentCamera();
    if (!camera) {
        _lastError = QStringLiteral("no camera");
        return DjiDrcError::kObtainControlFailed;
    }
    if (!camera->takePhoto()) {
        _lastError = QStringLiteral("takePhoto rejected");
        return DjiDrcError::kTakePhotoFailed;
    }
    return DjiDrcError::kSuccess;
}

int DjiDrcControlMapper::cameraPhotoStop()
{
    MavlinkCameraControl* camera = currentCamera();
    if (!camera) {
        _lastError = QStringLiteral("no camera");
        return DjiDrcError::kObtainControlFailed;
    }
    if (!camera->stopTakePhoto()) {
        _lastError = QStringLiteral("stopTakePhoto rejected");
        return DjiDrcError::kTakePhotoFailed;
    }
    return DjiDrcError::kSuccess;
}

int DjiDrcControlMapper::cameraRecordingStart()
{
    MavlinkCameraControl* camera = currentCamera();
    if (!camera) {
        _lastError = QStringLiteral("no camera");
        return DjiDrcError::kObtainControlFailed;
    }
    if (!camera->startVideoRecording()) {
        _lastError = QStringLiteral("startVideoRecording rejected");
        return DjiDrcError::kStartRecordingFailed;
    }
    return DjiDrcError::kSuccess;
}

int DjiDrcControlMapper::cameraRecordingStop()
{
    MavlinkCameraControl* camera = currentCamera();
    if (!camera) {
        _lastError = QStringLiteral("no camera");
        return DjiDrcError::kObtainControlFailed;
    }
    if (!camera->stopVideoRecording()) {
        _lastError = QStringLiteral("stopVideoRecording rejected");
        return DjiDrcError::kStopRecordingFailed;
    }
    return DjiDrcError::kSuccess;
}

int DjiDrcControlMapper::cameraModeSwitch(int cameraMode)
{
    MavlinkCameraControl* camera = currentCamera();
    if (!camera) {
        _lastError = QStringLiteral("no camera");
        return DjiDrcError::kObtainControlFailed;
    }

    // CameraModeEnum：0=拍照 1=录像（2=低光智能 3=全景，本机无对应能力）
    switch (cameraMode) {
    case 0:
        camera->setCameraModePhoto();
        return DjiDrcError::kSuccess;
    case 1:
        camera->setCameraModeVideo();
        return DjiDrcError::kSuccess;
    default:
        _lastError = QStringLiteral("unsupported camera mode %1").arg(cameraMode);
        return DjiDrcError::kSwitchCameraModeFailed;
    }
}

int DjiDrcControlMapper::cameraFocalLengthSet(double zoomFactor)
{
    MavlinkCameraControl* camera = currentCamera();
    if (!camera) {
        _lastError = QStringLiteral("no camera");
        return DjiDrcError::kObtainControlFailed;
    }
    if (!camera->hasZoom()) {
        _lastError = QStringLiteral("camera has no zoom");
        return DjiDrcError::kWrongLensType;
    }

    // DJI 给的是"变焦倍数"（协议规定 2~200），QGC 的 zoomLevel 是 0~100 的
    // 归一化档位（MAV_CMD_SET_CAMERA_ZOOM 的 ZOOM_TYPE_RANGE）。
    // 这里按协议的 [2,200] 线性映射。如果本机镜头的实际倍数范围不是 2~200，
    // 映射出来的"倍数"就对不上 —— 台架上按实际镜头核对一次。
    const double factor = qBound(2.0, zoomFactor, 200.0);
    const double level  = (factor - 2.0) / 198.0 * 100.0;
    camera->setZoomLevel(level);
    return DjiDrcError::kSuccess;
}

int DjiDrcControlMapper::gimbalReset(int resetMode)
{
    if (!_vehicle || !_vehicle->gimbalController()) {
        _lastError = QStringLiteral("no gimbal controller");
        return DjiDrcError::kObtainControlFailed;
    }
    GimbalController* controller = _vehicle->gimbalController();

    // GimbalResetModeEnum：0 回中 1 朝下 2 回中并回正 3 俯仰朝下。
    // MAVLink 没有"云台复位"这个原语，只能拿回中 / 指定俯仰去近似：
    //   0、2 → 回中（centerGimbal 走 DO_GIMBAL_MANAGER_PITCHYAW 的 NaN 回中语义）
    //   1、3 → 俯仰打到 -90（朝下），偏航保持不动
    const bool toDown = (resetMode == 1 || resetMode == 3);
    if (toDown) {
        Gimbal* gimbal = controller->activeGimbal();
        const double yaw = gimbal ? gimbal->absoluteYaw()->rawValue().toDouble() : 0.0;
        controller->sendPitchAbsoluteYaw(kMinGimbalPitch, yaw);
    } else {
        controller->centerGimbal();
    }
    return DjiDrcError::kSuccess;
}

int DjiDrcControlMapper::cameraAim(double nx, double ny)
{
    if (!_vehicle || !_vehicle->gimbalController()) {
        _lastError = QStringLiteral("no gimbal controller");
        return DjiDrcError::kObtainControlFailed;
    }
    Gimbal* gimbal = _vehicle->gimbalController()->activeGimbal();
    if (!gimbal) {
        _lastError = QStringLiteral("no active gimbal");
        return DjiDrcError::kAimFailed;
    }

    // x、y 是归一化的画面坐标（左上角为原点），把该点转到画面中心。
    // 像素偏移 → 角度的换算要用镜头的视场角，而 QGC 拿不到镜头 FOV ——
    // 用设置项 drcCameraHFov 给的水平视场角按 16:9 估垂直视场角。
    // 不同镜头（广角/变焦/红外）FOV 差很多，实测后按镜头调这个设置项。
    const double hFov = SettingsManager::instance()->cloudServerSettings()->drcCameraHFov()->rawValue().toDouble();
    const double vFov = hFov * 9.0 / 16.0;

    const double currentPitch = gimbal->absolutePitch()->rawValue().toDouble();
    const double currentYaw   = gimbal->absoluteYaw()->rawValue().toDouble();

    // 点在中心右侧（nx>0.5）→ 云台要往右转；点在中心下方（ny>0.5）→ 俯仰往下压
    const double yaw   = currentYaw + (nx - 0.5) * hFov;
    const double pitch = qBound(kMinGimbalPitch, currentPitch - (ny - 0.5) * vFov, kMaxGimbalPitch);

    _vehicle->gimbalController()->sendPitchAbsoluteYaw(pitch, yaw);
    return DjiDrcError::kSuccess;
}

int DjiDrcControlMapper::cameraLookAt(double latitude, double longitude, double height)
{
    if (!_vehicle || !_vehicle->gimbalController()) {
        _lastError = QStringLiteral("no gimbal controller");
        return DjiDrcError::kObtainControlFailed;
    }

    const QGeoCoordinate here(_vehicle->coordinate());
    const QGeoCoordinate target(latitude, longitude);
    if (!here.isValid() || !target.isValid()) {
        _lastError = QStringLiteral("invalid coordinate");
        return DjiDrcError::kAimFailed;
    }

    // height 与飞机高度都用"相对起飞点高度"（DJI 协议里 height 就是这么定的），
    // 两边同基准，差值才有意义。
    const double horizontal = here.distanceTo(target);
    const double dz         = _vehicle->altitudeRelative()->rawValue().toDouble() - height;

    double pitch = 0.0;
    if (horizontal > 0.5) {
        // 目标在下方时 dz>0，云台俯仰应为负
        pitch = -qRadiansToDegrees(std::atan2(dz, horizontal));
    }
    const double yaw = here.azimuthTo(target); // 0~360，正北为 0，顺时针为正

    _vehicle->gimbalController()->sendPitchAbsoluteYaw(qBound(kMinGimbalPitch, pitch, kMaxGimbalPitch), yaw);
    return DjiDrcError::kSuccess;
}
