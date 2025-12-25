/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/


/// @file
///     @author Don Gagne <don@thegagnes.com>

#pragma once

#include <QtCore/QObject>
#include <QtCore/QLoggingCategory>
#include <QJsonObject>
#include <QtMqtt/QMqttTopicName>

class LinkInterface;
class Vehicle;
class QmlObjectListModel;
class QTimer;
class QMqttClient;
class GCU;
class VideoSettings;
class VideoManager;

Q_DECLARE_LOGGING_CATEGORY(MultiVehicleManagerLog)

class MultiVehicleManager : public QObject
{
    Q_OBJECT
    Q_MOC_INCLUDE("QmlObjectListModel.h")
    Q_MOC_INCLUDE("LinkInterface.h")
    Q_MOC_INCLUDE("Vehicle.h")
    Q_PROPERTY(bool                 activeVehicleAvailable          READ _getActiveVehicleAvailable                                         NOTIFY activeVehicleAvailableChanged)
    Q_PROPERTY(bool                 parameterReadyVehicleAvailable  READ _getParameterReadyVehicleAvailable                                 NOTIFY parameterReadyVehicleAvailableChanged)
    Q_PROPERTY(Vehicle              *activeVehicle                  READ activeVehicle                      WRITE setActiveVehicle          NOTIFY activeVehicleChanged)
    Q_PROPERTY(QmlObjectListModel   *vehicles                       READ vehicles                                                           CONSTANT)
    Q_PROPERTY(QmlObjectListModel   *selectedVehicles               READ selectedVehicles                                                   CONSTANT)
    Q_PROPERTY(Vehicle              *offlineEditingVehicle          READ offlineEditingVehicle                                              CONSTANT)
    Q_PROPERTY(bool                 mqttConnected                   READ mqttConnected                                            CONSTANT NOTIFY mqttConnectedChanged)

public:
    explicit MultiVehicleManager(QObject *parent = nullptr);
    ~MultiVehicleManager();

    static MultiVehicleManager *instance();
    static void registerQmlTypes();

    void init();
    void updateDevicesInCloudServer();
    /**
     * @brief sendMqttReply   向mqtt服务器发送应答消息
     * @param topicPrefix sys/thing，sys:任何设备都通用的上云功能，比如设备注册，设备生命周期状态更新等功能，
     *                              thing：支撑实现各设备物模型定义的功能的Topic，主要围绕Property，Service，Event展开
     * @param topicSuffix state_reply, set_reply等
     * @param tid         事务（Transaction）的 UUID：表征一次简单的消息通信,如：增/删/改/查，云台控制等
     * @param bid         业务（Business）的 UUID：有些功能不是一次通信就能完成的，包含持续一段时间内的所有交互。
     *                    业务通常由多个原子事务组成，且持续时间较长;例如点播/下载/回放；解决业务多并发和重复请求的问题，
     *                    便于所有模块的状态机管理。
     * @param method      物模型文件中的service的tidentifier
     * @param result      用于表示ack消息中的事件结果（是否成功）
     */
    void sendMqttReply(const QString& topicPrefix, const QString& topicSuffix, const QString& tid, const QString& bid, const QString& method, const int& result);
    Q_INVOKABLE Vehicle *getVehicleById(int vehicleId) const;
    Q_INVOKABLE void      selectVehicle(int vehicleId);
    Q_INVOKABLE void    deselectVehicle(int vehicleId);
    Q_INVOKABLE void    deselectAllVehicles();
    Q_INVOKABLE void    connectToMqttHost();
    QmlObjectListModel *vehicles() const { return _vehicles; }
    QmlObjectListModel *selectedVehicles() const { return _selectedVehicles; }
    Vehicle *offlineEditingVehicle() const { return _offlineEditingVehicle; }
    bool mqttConnected() const { return _mqttConnected; }
    Vehicle *activeVehicle() const { return _activeVehicle; }
    void setActiveVehicle(Vehicle *vehicle);

signals:
    void vehicleAdded(Vehicle *vehicle);
    void vehicleRemoved(Vehicle *vehicle);
    void activeVehicleAvailableChanged(bool activeVehicleAvailable);
    void parameterReadyVehicleAvailableChanged(bool parameterReadyVehicleAvailable);
    void activeVehicleChanged(Vehicle *activeVehicle);
    void mqttConnectedChanged(bool mqttConnected);

private slots:
    void _deleteVehiclePhase1(Vehicle *vehicle); /// This slot is connected to the Vehicle::allLinksDestroyed signal such that the Vehicle is deleted and all other right things happen when the Vehicle goes away.
    void _deleteVehiclePhase2(Vehicle *vehicle);
    void _setActiveVehiclePhase2(Vehicle *vehicle);
    void _vehicleParametersReadyChanged(bool parametersReady);
    void _sendGCSHeartbeat();
    void _vehicleHeartbeatInfo(LinkInterface *link, int vehicleId, int componentId, int vehicleFirmwareType, int vehicleType);
    void _requestProtocolVersion(unsigned version) const; /// This slot is connected to the Vehicle::requestProtocolVersion signal such that the vehicle manager tries to switch MAVLink to v2 if all vehicles support it
    /**
     * @brief _sendOsdToServer发送属性信息，重复间隔0.5s
     */
    void _sendOsdToServer();
    /**
     * @brief _sendStateLiveCapacityToServer 发送直播属性，只有变化时才发送
     */
    void _sendStateLiveCapacityToServer();
    void _receiveMqttFromServer(const QByteArray &message, const QMqttTopicName &topic = QMqttTopicName());
private:
    bool _vehicleExists(int vehicleId);
    bool _vehicleSelected(int vehicleId);
    void _setActiveVehicle(Vehicle *vehicle);
    bool _getActiveVehicleAvailable() const { return _activeVehicleAvailable; }
    void _setActiveVehicleAvailable(bool activeVehicleAvailable);
    bool _getParameterReadyVehicleAvailable() const { return _parameterReadyVehicleAvailable; }
    void _setParameterReadyVehicleAvailable(bool parametersReady);

    QTimer *_gcsHeartbeatTimer = nullptr;           ///< Timer to emit heartbeats
    QmlObjectListModel *_vehicles = nullptr;
    QmlObjectListModel *_selectedVehicles = nullptr;
    Vehicle *_offlineEditingVehicle = nullptr;      ///< Disconnected vechicle used for offline editing
    bool _mqttConnected = false;
    bool _activeVehicleAvailable = false;           ///< true: An active vehicle is available
    bool _parameterReadyVehicleAvailable = false;   ///< true: An active vehicle with ready parameters is available
    Vehicle *_activeVehicle = nullptr;              ///< Currently active vehicle from a ui perspective
    QList<int> _ignoreVehicleIds;                   ///< List of vehicle id for which we ignore further communication
    bool _initialized = false;

    static constexpr int kGCSHeartbeatRateMSecs = 1000;  ///< Heartbeat rate

    /**********  Mqtt  **********/
    QMqttClient *_mqttClient = nullptr;
    QTimer * _timerSendOsd = nullptr;               // 发送信息到服务器
    // QJsonObject jsonDevices;                        // 设备信息
    // QJsonObject jsonGcs;                            // 地面站信息
    // QJsonObject jsonDrone;                          // 无人机信息
    VideoSettings* _videoSettings = nullptr;
    VideoManager*  _videoManager  = nullptr;

};
