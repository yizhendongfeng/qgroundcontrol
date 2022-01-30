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
#include "QGCMAVLink.h"
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
    const QList<MissionItem*>& missionItems(void) { return _missionItems; }

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
    ///     Signals sendComplete when done
    void writeMissionItems(const QList<MissionItem*>& missionItems);

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


signals:
    void totalBankInfoAvailbale(const TotalBankInfo& totalBankInfo);  //已获取整体航线信息
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
    void _mavlinkMessageReceived(const mavlink_message_t& message);
    void _shenHangMessageReceived(const ShenHangProtocolMessage& message);
    void _ackTimeout(void);
    void _ackTimeoutShenHang(void);

protected:
    typedef enum {
        AckNone,            ///< State machine is idle
        AckMissionCount,    ///< MISSION_COUNT message expected
        AckMissionItem,     ///< MISSION_ITEM expected
        AckMissionRequest,  ///< MISSION_REQUEST is expected, or MISSION_ACK to end sequence
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

    void _startAckTimeout(AckType_t ack);
    void _startAckTimeoutShenHang(AckCommandBank ack);
    bool _checkForExpectedAck(AckType_t receivedAck);
    bool _checkForExpectedAckShenHang(AckCommandBank receivedAck);
    void _readTransactionComplete(void);
    void _handleMissionCount(const mavlink_message_t& message);
    void _handleMissionItem(const mavlink_message_t& message, bool missionItemInt);
    void _handleMissionRequest(const mavlink_message_t& message, bool missionItemInt);
    void _handleMissionAck(const mavlink_message_t& message);
    void _requestNextMissionItem(void);
    void _clearMissionItems(void);
    void _sendError(ErrorCode_t errorCode, const QString& errorMsg);
    QString _ackTypeToString(AckType_t ackType);
    QString _ackTypeToStringShenHang(AckCommandBank ackType);
    QString _missionResultToString(MAV_MISSION_RESULT result);
    void _finishTransaction(bool success, bool apmGuidedItemWrite = false);
    void _requestList(void);
    void _writeMissionCount(void);
    void _writeMissionItemsWorker(void);
    void _clearAndDeleteMissionItems(void);
    void _clearAndDeleteWriteMissionItems(void);
    QString _lastMissionReqestString(MAV_MISSION_RESULT result);
    void _removeAllWorker(void);
    void _connectToMavlink(void);
    void _disconnectFromMavlink(void);

    /******************** 沈航航线数据处理 ********************/
    void _connectToShenHangVehicle(void);
    void _disconnectFromShenHangVehicle(void);
    void _handleShenHangBankMessage(const ShenHangProtocolMessage& msg);
    void _handleAllBankInfo(const ShenHangProtocolMessage& message);
    void _handleSingleBankInfo(const ShenHangProtocolMessage& message);
    void _handleInfoSlot(const ShenHangProtocolMessage& message);

    QString _planTypeString(void);
    void _queryAllBanks(void);
    void _querySingleBankInfo(uint16_t idBank);
    void _querySingleInfoSlot(uint16_t idBank, uint16_t idInfoSlot);

protected:
    Vehicle*            _vehicle =              nullptr;
    MissionCommandTree* _missionCommandTree =   nullptr;
    MAV_MISSION_TYPE    _planType;

    QTimer*             _ackTimeoutTimer =      nullptr;
    AckType_t           _expectedAck;
    int                 _retryCount;

    TransactionType_t   _transactionInProgress;
    bool                _resumeMission;
    QList<int>          _itemIndicesToWrite;    ///< List of mission items which still need to be written to vehicle
    QList<int>          _itemIndicesToRead;     ///< List of mission items which still need to be requested from vehicle
    int                 _lastMissionRequest;    ///< Index of item last requested by MISSION_REQUEST
    int                 _missionItemCountToRead;///< Count of all mission items to read

    QList<MissionItem*> _missionItems;          ///< Set of mission items on vehicle
    QList<MissionItem*> _writeMissionItems;     ///< Set of mission items currently being written to vehicle
    int                 _currentMissionIndex;
    int                 _lastCurrentIndex;

    /******************** 沈航数据 ********************/
    int                     _retryCountShenHang;
    AckCommandBank          _expectedAckShenHang;
    MISSION_TYPE_SHENHANG   _planTypeShenHang;
    SingleBankInfo          _currentSingleBankInfo = {};
    QList<uint16_t>         _infoSlotIndicesToWrite;
    QList<uint16_t>         _infoSlotIndicesToRead;
    QMap<uint16_t, QList<uint16_t>> _mapInfoSlotIndicesToRead;
    QList<uint16_t>         _bankIndicesToRead;
    QMap<uint16_t, QMap<uint16_t, WaypointInfoSlot*>> _mapWaypointInfoSlot;
    QList<uint16_t>         _bankIndicesToWrite;
    uint16_t                _currentBankIndexToRead;    // 读取infoslot时，在map中的bank索引
    uint16_t                _infoSlotCountToRead;
    uint16_t                _bankCountToWrite = 2;
    uint16_t                _infoSlotCountToWrite;

//    QMap<uint16_t, SingleBankInfo> _mapBankInfo;
    QMap<uint16_t, QMap<uint16_t, WaypointInfoSlot*>> _mapInfoSlots;  // <idBank, <idInfoslot, waypointInfoslot>>>
    uint16_t                _lastBankIdRequested;
    uint16_t                _lastInfoSlotIdRequested;
private:
    void _setTransactionInProgress(TransactionType_t type);
};
