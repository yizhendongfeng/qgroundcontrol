/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include <QObject>
#include <QLoggingCategory>
#include <QTimer>

#include "MissionItem.h"
#include "QGCLoggingCategory.h"
#include "LinkInterface.h"
#include "ShenHangProtocol.h"

class Vehicle;
class MissionCommandTree;

Q_DECLARE_LOGGING_CATEGORY(PlanManagerLog)

/// The PlanManager class is the base class for the Mission, GeoFence and Rally Point managers. All of which use the
/// new mavlink v2 mission protocol.
class PlanManager : public QObject
{
    Q_OBJECT

public:
    PlanManager(Vehicle* vehicle, MAV_MISSION_TYPE planType);
    ~PlanManager();

    bool inProgress(void) const;
    QMap<uint16_t, QMap<uint16_t, WaypointInfoSlot*>>& mapInfoSlotsInBanks(void) { return _mapInfoSlotsInBanksToWrite; }
    QMap<uint16_t, SingleBankInfo*>& mapSingleBankInfo(void) {return _mapBankInfosToWrite;}
    /// Current mission item as reported by MISSION_CURRENT
    int currentIndex(void) const { return _currentMissionIndex; }

    /// Last current mission item reported while in Mission flight mode
    int lastCurrentIndex(void) const { return _lastCurrentIndex; }

    /// Load the mission items from the vehicle
    ///     Signals newMissionItemsAvailable when done
    void loadFromVehicle(void);

    /// Writes the specified set of mission items to the vehicle
    /// IMPORTANT NOTE: PlanManager will take control of the MissionItem objects with the missionItems list. It will free them when done.
    ///     @param missionItems Items to send to vehicle
    ///     @param bankInfos 航线信息
    ///     @param infoSlotsInBanks 每个bank中的infoslot
    ///     Signals sendComplete when done
    void writeInfoSlots(const QMap<uint16_t, SingleBankInfo *> &bankInfos, const QMap<uint16_t, QMap<uint16_t, WaypointInfoSlot *> > &infoSlotsInBanks);

    /// Removes all mission items from vehicle
    ///     Signals removeAllComplete when done
    void removeAll(void);

    /// Error codes returned in error signal
    typedef enum {
        InternalError,
        AckTimeoutError,        ///< Timed out waiting for response from vehicle
        ProtocolError,          ///< Incorrect protocol sequence from vehicle
        RequestRangeError,      ///< Vehicle requested item out of range
        ItemMismatchError,      ///< Vehicle returned item with seq # different than requested
        VehicleAckError,        ///< Vehicle returned error in ack
        MissingRequestsError,   ///< Vehicle did not request all items during write sequence
        MaxRetryExceeded,       ///< Retry failed
        MissionTypeMismatch,    ///< MAV_MISSION_TYPE does not match _planType
    } ErrorCode_t;

    // These values are public so the unit test can set appropriate signal wait times
    // When passively waiting for a mission process, use a longer timeout.
    static const int _ackTimeoutMilliseconds = 1500;
    // When actively retrying to request mission items, use a shorter timeout instead.
    static const int _retryTimeoutMilliseconds = 250;
    static const int _maxRetryCount = 5;

    /******************** 航线相关命令（ty_msg0=130） ********************/
    /**
     * @brief packCommandBankQueryAll 查询整体航路信息（ty_msg0=130,ty_msg1=0）
     * @param msg
     */
    void packCommandBankQueryAll(ShenHangProtocolMessage& msg);

    /**
     * @brief packCommandBankQuerySingle 查询单个bank信息（ty_msg0=130,ty_msg1=1）
     * @param msg
     * @param idBank
     */
    void packCommandBankQuerySingle(ShenHangProtocolMessage& msg, uint16_t idBank);

    /**
     * @brief packCommandBankSetSingle 设置单个bank（ty_msg0=130,ty_msg1=2）
     * @param msg 传出参数
     * @param idBank 对应需要设置的bank编号
     * @param idBankSuc 对应接续航线bank编号
     * @param idBankIwpSuc 对应接续航线中航点编号
     * @param actBankEnd 设置航线完成动作，暂未定义，保留
     * @param flagBankVerified 设置航线校验位，0：校验未通过，1：校验通过
     * @param flagBankLock 设置航线锁定位，0：解除锁定，1：设置锁定
     */
    void packCommandBankSetSingle(ShenHangProtocolMessage& msg, uint16_t idBank, uint16_t idBankSuc, uint16_t idBankIwpSuc, uint16_t actBankEnd, uint8_t flagBankVerified, uint8_t flagBankLock);

    /**
     * @brief packRefactorInfoSlot 重构infoslot表单（ty_msg0=130,ty_msg1=3）
     * @param msg 传出参数
     * @param idBank 对应需要重构的bank编号
     * @param nWp 对应bank内的航路点数目
     * @param nInfoSlot 对应bank内的infoslot数目
     */
    void packRefactorInfoSlot(ShenHangProtocolMessage& msg, uint16_t idBank, uint16_t nWp, uint16_t nInfoSlot);

    /**
     * @brief packQuerySingleInfoSlot 查询单个infoslot（ty_msg0=130,ty_msg1=4）
     * @param msg 传出参数
     * @param idBank 对应要查询的infoslot所在的bank编号
     * @param idInfoSlot 对应需要查询的infoslot编号
     */
    void packQuerySingleInfoSlot(ShenHangProtocolMessage& msg, uint16_t idBank, uint16_t idInfoSlot);

    /**
     * @brief packSetSingleInfoSlot 打包发送航路点信息
     * @param msg 传出参数
     * @param infoSlot
     */
    void packSetSingleInfoSlot(ShenHangProtocolMessage& msg, WaypointInfoSlot *infoSlot);

    void packSetSingleBank(ShenHangProtocolMessage& msg, SetSingleBank setSingleBank);

signals:
    void totalBankInfoAvailbale     (const TotalBankInfo& totalBankInfo);  //已获取整体航线信息
    void newMissionItemsAvailable   (bool removeAllRequested);
    void inProgressChanged          (bool inProgress);
    void error                      (int errorCode, const QString& errorMsg);
    void currentIndexChanged        (int currentIndex);
    void lastCurrentIndexChanged    (int lastCurrentIndex);
    void progressPct                (double progressPercentPct);
    void removeAllComplete          (bool error);
    void sendComplete               (bool error);
    void resumeMissionReady         (void);
    void resumeMissionUploadFail    (void);

private slots:
    void _shenHangMessageReceived(const ShenHangProtocolMessage& message);
    void _ackTimeout(void);

protected:
    typedef enum {
        AckNone,            ///< State machine is idle
        AckMissionCount,    ///< MISSION_COUNT message expected
        AckMissionItem,     ///< MISSION_ITEM expected
        AckMissionQuery,    ///< MISSION_Query is expected, or MISSION_ACK to end sequence
        AckMissionClearAll, ///< MISSION_CLEAR_ALL sent, MISSION_ACK is expected
        AckGuidedItem,      ///< MISSION_ACK expected in response to ArduPilot guided mode single item send
    } AckType_t;

    enum MISSION_TYPE_SHENHANG
    {
       SHENHANG_MISSION_TYPE_MISSION=0, /* Items are mission commands for main mission. | */
       SHENHANG_MISSION_TYPE_FENCE=1, /* Specifies GeoFence area(s). Items are MAV_CMD_NAV_FENCE_ GeoFence items. | */
       SHENHANG_MISSION_TYPE_RALLY=2, /* Specifies the rally points for the vehicle. Rally points are alternative RTL points. Items are MAV_CMD_NAV_RALLY_POINT rally point items. | */
       SHENHANG_MISSION_TYPE_ALL=255, /* Only used in MISSION_CLEAR_ALL to clear all mission types. | */
       SHENHANG_MISSION_TYPE_ENUM_END=256, /*  | */
    };
    typedef enum {
        TransactionNone,
        TransactionRead,
        TransactionWrite,
        TransactionRemoveAll
    } TransactionType_t;

    void _startAckTimeout(AckCommandBank ack);
    bool _checkForExpectedAck(AckCommandBank receivedAck);
    void _readTransactionComplete(void);
    void _clearMissionItems(void);
    void _sendError(ErrorCode_t errorCode, const QString& errorMsg);
    QString _ackTypeToString(AckCommandBank ackType);
    QString _missionResultToString(MAV_MISSION_RESULT result);
    void _finishTransaction(bool success, bool apmGuidedItemWrite = false);
    void _writeInfoSlotsWorker(void);
    void _clearAndDeleteReadInfoSlots(void);
    void _clearAndDeleteWriteInfoSlots(void);
    void _removeAllWorker(void);

    /******************** 沈航航线数据处理 ********************/
    void _connectToShenHangVehicle(void);
    void _disconnectFromShenHangVehicle(void);
    void _handleShenHangBankMessage(const ShenHangProtocolMessage& msg);
    void _handleAllBankInfo(const ShenHangProtocolMessage& message);
    void _handleAckCommandBank(const ShenHangProtocolMessage& message);
    void _handleAckInfoSlot(const ShenHangProtocolMessage& message);
    void _handleSingleBankInfo(const ShenHangProtocolMessage& message);
//    void _handleInfoSlot(const ShenHangProtocolMessage& message);


    QString _planTypeString(void);
    void _queryAllBanks(void);
    void _querySingleBankInfo(uint16_t idBank);
    void _setSingleBankInfo(SetSingleBank setSingleBank);
    void _refactorInfoSlots(uint16_t idBank, uint16_t nWp, uint16_t nInfoSlot);
    void _querySingleInfoSlot(uint16_t idBank, uint16_t idInfoSlot);
    void _setSingleInfoSlot(WaypointInfoSlot* infoSlot);
protected:
    Vehicle*            _vehicle =              nullptr;
    MAV_MISSION_TYPE    _planType;

    QTimer*             _ackTimeoutTimer =      nullptr;
    AckType_t           _expectedAck;
    int                 _retryCount = 0;

    TransactionType_t   _transactionInProgress;
    bool                _resumeMission;
    int                 _lastMissionRequest;    ///< Index of item last requested by MISSION_REQUEST
    int                 _missionItemCountToRead;///< Count of all mission items to read

    int                 _currentMissionIndex;
    int                 _lastCurrentIndex;

    /******************** 沈航数据 ********************/
    int                     _retryCountShenHang;
    AckCommandBank          _expectedAckShenHang;
    MISSION_TYPE_SHENHANG   _planTypeShenHang;
    SingleBankInfo          _currentSingleBankInfo = {};

    uint16_t                _lastBankIdRead = 0;
    uint16_t                _lastInfoSlotIdRead = 0;
    uint16_t                _infoSlotCountToRead;
    QList<uint16_t>         _bankIndicesToRead;
    QList<uint16_t>         _infoSlotIndicesToRead;
    QMap<uint16_t, QList<uint16_t>> _mapInfoSlotIndicesToRead;
    QMap<uint16_t, SingleBankInfo*> _mapBankInfosRead;
    QMap<uint16_t, QMap<uint16_t, WaypointInfoSlot*>> _mapInfoSlotsInBanksRead; // <idBank, <idInfoSlot, WaypointInfoSlot*>> 已读取的infoSlot

    uint16_t                _lastBankIdWrite = 0;
    uint16_t                _lastInfoSlotIdWrite = 0;
    uint16_t                _infoSlotCountToWrite;
    QList<uint16_t>         _bankIndicesToWrite;
    QList<uint16_t>         _infoSlotIndicesToWrite;
    QMap<uint16_t, SingleBankInfo*> _mapBankInfosToWrite;
    QMap<uint16_t, QMap<uint16_t, WaypointInfoSlot*>> _mapInfoSlotsInBanksToWrite; // <idBank, <idInfoSlot, WaypointInfoSlot*>>
    bool                    _bankRefactored = false;    // bank是否已经重构过；true: 重构过之后再设置bank，等接收到bank的信息才算完成一个航线的传输; false: 接收到的bank信息为解锁

private:
    void _setTransactionInProgress(TransactionType_t type);
};
