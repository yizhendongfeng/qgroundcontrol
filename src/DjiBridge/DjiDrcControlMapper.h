/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>

class Vehicle;
class MavlinkCameraControl;

/// @file
/// @brief DJI 上云 API —— DRC 指令 / 负载控制指令 → QGC 动作的落地层
///
/// 这是**唯一**把云端报文变成飞机动作的地方：DjiDrcClient 只管协议（收报文、解字段、
/// 应答、心跳、超时），"怎么动"全在这里。这样拆是因为动手的地方必须能一眼看全 ——
/// 尤其是 drone_emergency_stop 这种不可逆的。
///
/// 量程与方向约定集中在 .cc 顶部那组常量里（依据官方 SDK 的校验注解 + 官方网页端
/// demo 的注释）。实测发现方向不对时改那里的常量，别改 DjiDrcClient。
///
/// 负载指令（相机/云台）的返回值遵循 DJI 的 ControlErrorCodeEnum：0 成功，非 0 是
/// 32700x 系列错误码（拍照失败 / 云台到限位 / 镜头类型不对…）。

/// DJI 上行应答里的错误码。取值来自官方 cloud-sdk 的
/// cloudapi/control/ControlErrorCodeEnum 与 DrcStatusErrorEnum。
namespace DjiDrcError {
constexpr int kSuccess                = 0;
constexpr int kObtainControlFailed     = 327002; ///< 无控制权 / 未取得控制权
constexpr int kAimFailed               = 327005;
constexpr int kTakePhotoFailed         = 327006;
constexpr int kStartRecordingFailed    = 327007;
constexpr int kStopRecordingFailed     = 327008;
constexpr int kSwitchCameraModeFailed  = 327009;
constexpr int kZoomFailed              = 327010;
constexpr int kGimbalReachLimit        = 327014;
constexpr int kWrongLensType           = 327015;
constexpr int kDrcAbnormal             = 514300; ///< 通用 DRC 异常
constexpr int kDrcHeartbeatTimeout     = 514301;
constexpr int kDrcCertificateAbnormal  = 514302;
constexpr int kDrcLinkLost             = 514303;
constexpr int kDrcLinkRefused          = 514304;
} // namespace DjiDrcError

class DjiDrcControlMapper : public QObject
{
    Q_OBJECT

public:
    explicit DjiDrcControlMapper(QObject* parent = nullptr);

    /// 当前受控飞机（由 DjiBridgeServer::activeVehicleChanged 转发过来）
    void setVehicle(Vehicle* vehicle);
    Vehicle* vehicle() const { return _vehicle; }

    // ---------- 飞行控制 ----------

    /// 摇杆指令被拒绝的原因。Ok 以外的取值说明"收到了但没执行"，
    /// DjiDrcClient 会据此回非 0 的 result —— 不能让云端以为自己在控制飞机。
    enum class JoystickResult {
        Ok,
        NoVehicle,  ///< 没有受控飞机
        NotArmed,   ///< 未解锁
        WrongMode,  ///< 当前飞行模式不吃手动输入，见 .cc 的 kManualControlModes
    };

    /// 归一化后的杆量：roll/pitch/yaw ∈ [-1,1]，**thrust ∈ [0,1]、0.5 = 悬停**。
    /// thrust 不是 [-1,1] —— 四路值域并不相同，原因见 .cc 的 kThrottleNeutral。
    struct Axes {
        double roll   = 0.0;
        double pitch  = 0.0;
        double yaw    = 0.0;
        double thrust = 0.5;
    };

    /// 原始杆量 → QGC 摇杆轴。**只做映射**：发报文、查门禁都不在这里。
    /// 抽成纯函数是为了推力零点能单独验 —— 它已经被判错过一次方向。
    Axes mapAxes(double x, double y, double h, double w) const;

    /// drone_control：x/y 水平速度（m/s）、h 升降速度（m/s）、w 偏航角速度（°/s）
    JoystickResult applyDroneControl(double x, double y, double h, double w);

    /// 最近一次**被接受**的那组轴值。QML 显示必须用它而不是原始值 ——
    /// 映射里有取反和 drcInvertX/Y/W，直接显示原始值会出现"把手指一边、飞机飞另一边"。
    Axes lastAxes() const { return _lastAxes; }
    bool haveLastAxes() const { return _haveLastAxes; }

    /// 重发上一次被接受的杆量（25 Hz 保活流用）。
    ///
    /// **刻意不走门禁**：`applyDroneControl` 里那道模式门决定的是"这条指令该不该被
    /// 接受"（它对应 DJI 的错误码），不能拿来决定保活流是否流动 —— 否则航线上传
    /// 切进 Auto 模式时流会断，PX4 判断"没有手动输入源"，就再也回不到接受手动控制
    /// 的模式了，而恰恰是回到那种模式才需要这条流。
    void resendLastAxes();

    /// 摇杆归零。失联保护（deadman）与退出 DRC 时调用 —— 不归零的话最后一条
    /// 速度指令会一直留着。同时清掉"上一次杆量"，免得残余的保活 tick 把它复活。
    void zeroSticks();

    /// 判定结果 → DJI 错误码，供上行应答使用
    static int joystickResultCode(JoystickResult result);

    // ---------- 飞行模式策略 ----------
    //
    // 这个判定原先藏在 .cc 的匿名 namespace 里，现在提出来是因为 DjiDrcClient
    // 也要用：它拿这个告诉操作员"云端拿得到控制权，但飞机现在这条模式一条杆量
    // 都进不去"（工具栏红字 + 面板警告行）。

    /// 该模式是否接受 MANUAL_CONTROL。
    ///
    /// **注意表里是英文原名，不是可以直接比对的字面量** —— flightMode() 返回的是
    /// 译文，比对必须走 DjiFlightModeNames（原因见那里的注释）。
    ///
    /// 不在表里的模式（Auto/Mission/RTL/Land/Guided…）飞控会直接忽略 MANUAL_CONTROL，
    /// 云端却以为自己在控制飞机 —— 与其静默无效，不如回一个错误码。
    /// 表里 PX4 与 ArduPilot 两套模式名都列上（QGC 的 flightMode() 返回字符串，两套用词不同）。
    static bool isManualControlMode(const QString& mode);

    /// drone_emergency_stop：映射为 MAV_CMD_DO_FLIGHTTERMINATION（不可逆）
    /// @return true 表示指令已下发（不代表飞控执行成功）
    bool emergencyStop();

    // ---------- 负载控制（返回值见文件头说明） ----------

    int cameraPhotoTake();
    int cameraPhotoStop();
    int cameraRecordingStart();
    int cameraRecordingStop();
    /// @param cameraMode 0=拍照 1=录像（CameraModeEnum），其余返回错误码
    int cameraModeSwitch(int cameraMode);
    /// @param zoomFactor 变焦倍数，协议规定 2~200
    int cameraFocalLengthSet(double zoomFactor);
    /// @param resetMode GimbalResetModeEnum：0 回中 1 朝下 2 回中并回正 3 俯仰朝下
    int gimbalReset(int resetMode);
    /// 双击画面某点使其居中
    /// @param nx,ny 归一化的画面坐标 [0,1]，左上角为原点
    int cameraAim(double nx, double ny);
    /// 把指定经纬高转到画面中心
    int cameraLookAt(double latitude, double longitude, double height);

    /// 最近一次失败原因（仅日志与 UI 展示用）
    QString lastError() const { return _lastError; }

private:
    MavlinkCameraControl* currentCamera() const;

    /// 只发报文，不碰"上一次杆量"。归零与中立重发都走它 —— 那两种情况**不能**
    /// 被记成一条指令，否则保活流会一直重发一个本不该存在的"上一次"。
    void sendRaw(const Axes& axes);

    Vehicle* _vehicle = nullptr;
    mutable QString _lastError;

    /// 上一次被接受的轴值。25 Hz 保活流靠它重发，"归零"时一并清掉。
    /// 放在这里而不是 DjiDrcClient，是为了让"归零"和"重发"共用同一份状态 ——
    /// 分成两处的话，归零点漏掉一处就会出现"明明归零了却还在动"。
    Axes _lastAxes;
    bool _haveLastAxes = false;
};
