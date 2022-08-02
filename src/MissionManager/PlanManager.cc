/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "PlanManager.h"
#include "Vehicle.h"
#include "FirmwarePlugin.h"
#include "QGCApplication.h"

QGC_LOGGING_CATEGORY(PlanManagerLog, "PlanManagerLog")

PlanManager::PlanManager(Vehicle* vehicle, MAV_MISSION_TYPE planType)
    : _vehicle                  (vehicle)
    , _planType                 (planType)
    , _ackTimeoutTimer          (nullptr)
    , _expectedAck              (AckNone)
    , _transactionInProgress    (TransactionNone)
    , _resumeMission            (false)
    , _lastMissionRequest       (-1)
    , _missionItemCountToRead   (-1)
    , _currentMissionIndex      (-1)
    , _lastCurrentIndex         (-1)
{
    qRegisterMetaType<TotalBankInfo>();
    _ackTimeoutTimer = new QTimer(this);
    _ackTimeoutTimer->setSingleShot(true);

    connect(_ackTimeoutTimer, &QTimer::timeout, this, &PlanManager::_ackTimeout);
}

PlanManager::~PlanManager()
{

}

void PlanManager::_writeInfoSlotsWorker(void)
{
    _lastMissionRequest = -1;
    emit progressPct(0);
    qCDebug(PlanManagerLog) << QStringLiteral("_writeInfoSlotsWorker _mapBankInfos.keys:") << _mapBankInfosToWrite.keys();
    _bankIndicesToWrite = _mapBankInfosToWrite.keys();
    _retryCount = 0;
    _setTransactionInProgress(TransactionWrite);
    _connectToShenHangVehicle();
    _queryAllBanks();
}

void PlanManager::_handleAckInfoSlot(const ShenHangProtocolMessage &message)
{
    WaypointInfoSlot* infoSlot = new WaypointInfoSlot;
    memcpy(infoSlot, message.payload, sizeof(WaypointInfoSlot));
    qCDebug(PlanManagerLog) << "_handleAckInfoSlot idBank:" << infoSlot->idBank << "idInfoSlot:" << infoSlot->idInfoSlot;

    switch(_transactionInProgress) {
    case TransactionRead:
        if (infoSlot->idBank != _lastBankIdRead || infoSlot->idInfoSlot != _lastInfoSlotIdRead) {
            // TODO 返回错误的infoSlot，弹窗警告，并终止
            return;
        }
        _mapInfoSlotsInBanksToWrite[infoSlot->idBank][infoSlot->idInfoSlot] = infoSlot;
        qCDebug(PlanManagerLog) << "ACK_QUERY_SINGLE_INFO_SLOT idBank:" << infoSlot->idBank << "idInfosl:" << infoSlot->idInfoSlot
                                << QString("%1, %2, %3").arg(infoSlot->lat, 0, 'f', 7).arg(infoSlot->lon,  0, 'f', 7).arg(infoSlot->alt,  0, 'f', 3)
                                << "_bankIndicesToRead" << _bankIndicesToRead;
        _infoSlotIndicesToRead.removeOne(infoSlot->idInfoSlot);
        if (_infoSlotIndicesToRead.isEmpty()) {
            _bankIndicesToRead.removeOne(infoSlot->idBank);
            if (!_bankIndicesToRead.isEmpty()) {
                _lastBankIdRead = _bankIndicesToRead[0];
                _infoSlotIndicesToRead = _mapInfoSlotIndicesToRead[_lastBankIdRead];
            }
            else {
                qCDebug(PlanManagerLog) << "ACK_QUERY_SINGLE_INFO_SLOT READ COMPLETED!!!";
                _finishTransaction(true);
                emit newMissionItemsAvailable(false);
                return;
            }
        }
        _lastInfoSlotIdRead = _infoSlotIndicesToRead[0];
        _querySingleInfoSlot(_lastBankIdRead, _lastInfoSlotIdRead);
        break;
    case TransactionWrite:
        qCDebug(PlanManagerLog) << "ACK_SET_SINGLE_INFO_SLOT idBank:" << infoSlot->idBank
                                << "idInfosl:" << infoSlot->idInfoSlot << "_infoSlotIndicesToWrite" << _infoSlotIndicesToWrite
                                << "waypointInfoSlot->cs" << infoSlot->cs <<  _mapInfoSlotsInBanksToWrite[_lastBankIdWrite][_lastInfoSlotIdWrite]->cs;
        if (infoSlot->idBank != _lastBankIdWrite || infoSlot->idInfoSlot != _lastInfoSlotIdWrite) {
            // TODO 返回错误的infoSlot，弹窗警告，并终止
            return;
        }
        if (infoSlot->cs != _mapInfoSlotsInBanksToWrite[_lastBankIdWrite][_lastInfoSlotIdWrite]->cs) { // 判断返回的infoSlot是否一致
            //  TODO 弹窗警告
            return;
        }
        if (!_checkForExpectedAck(AckCommandBank::ACK_SET_SINGLE_INFO_SLOT)) {
            return;
        }
        _infoSlotIndicesToWrite.removeOne(_lastInfoSlotIdWrite);
        if (_infoSlotIndicesToWrite.isEmpty()) {   // 此航线已完成上传，重构infoSlot表单
            SingleBankInfo* bankInfo = _mapBankInfosToWrite[_lastBankIdWrite];
            _refactorInfoSlots(bankInfo->idBank, bankInfo->nWp, bankInfo->nInfoSlot);
        } else {
            _lastInfoSlotIdWrite = _infoSlotIndicesToWrite[0];
            _setSingleInfoSlot(_mapInfoSlotsInBanksToWrite[_lastBankIdWrite][_lastInfoSlotIdWrite]);
        }
        break;
    default:
        break;
    }
}

void PlanManager::writeInfoSlots(const QMap<uint16_t, SingleBankInfo*>& bankInfos, const QMap<uint16_t, QMap<uint16_t, WaypointInfoSlot*>> &infoSlotsInBanks)
{
    if (_vehicle->isOfflineEditingVehicle() || infoSlotsInBanks.isEmpty()) {
        return;
    }

    if (inProgress()) {
        qCDebug(PlanManagerLog) << QStringLiteral("writeMissionItems %1 called while transaction in progress").arg(_planTypeString());
        return;
    }

//    _clearAndDeleteWriteInfoSlots();
    _mapBankInfosToWrite = bankInfos;
    _mapInfoSlotsInBanksToWrite = infoSlotsInBanks;
    _writeInfoSlotsWorker();
}

void PlanManager::loadFromVehicle(void)
{
    if (_vehicle->isOfflineEditingVehicle()) {
        return;
    }

    qCDebug(PlanManagerLog) << QStringLiteral("loadFromVehicle %1 read sequence").arg(_planTypeString());

    if (inProgress()) {
        qCDebug(PlanManagerLog) << QStringLiteral("loadFromVehicle %1 called while transaction in progress").arg(_planTypeString());
        return;
    }

    _retryCount = 0;
    _setTransactionInProgress(TransactionRead);
    _connectToShenHangVehicle();
    while (!_mapBankInfosRead.isEmpty()) {
        delete _mapBankInfosRead.take(_mapBankInfosRead.keys().first());
    }
    foreach (uint16_t idBank, _mapInfoSlotsInBanksRead.keys()) {
        QMap<uint16_t, WaypointInfoSlot*> mapInfoSlotsIBank = _mapInfoSlotsInBanksRead.take(idBank);
        foreach (uint16_t idInfoSlot, mapInfoSlotsIBank.keys()) {
            delete mapInfoSlotsIBank.take(idInfoSlot);
        }
    }

    _queryAllBanks();
}

void PlanManager::_ackTimeout()
{
    if (_expectedAckShenHang == ACK_NONE) {
        return;
    }

    switch (_expectedAckShenHang) {
    case ACK_NONE:
        qCWarning(PlanManagerLog) << QStringLiteral("_ackTimeout %1 timeout with AckNone").arg(_planTypeString());
        _sendError(InternalError, tr("Internal error occurred during Mission Item communication: _ackTimeOut:_expectedAckShenHang == ACK_NONE"));
        break;
    case ACK_QUERY_ALL_BANK:
        if (_retryCount > _maxRetryCount) {
            _sendError(MaxRetryExceeded, tr("Query all bank info failed, maximum retries exceeded."));
            _finishTransaction(false);
        } else {
            _retryCount++;
            qCDebug(PlanManagerLog) << tr("Retrying %1 query all bank info retry Count").arg(_planTypeString()) << _retryCount;
            _queryAllBanks();
        }
        break;
    case ACK_QUERY_SINGLE_BANK:
        if (_retryCount > _maxRetryCount) {
            _sendError(MaxRetryExceeded, tr("Query single bank failed, maximum retries exceeded."));
            _finishTransaction(false);
        } else {
            _retryCount++;
            qCDebug(PlanManagerLog) << tr("Retrying %1 query single bank retry Count").arg(_planTypeString()) << _retryCount;
            switch (_transactionInProgress) {
            case TransactionRead:
                _querySingleBankInfo(_lastBankIdRead);
                break;
            case TransactionWrite:
                _querySingleBankInfo(_lastBankIdWrite);
                break;
            default:
                break;
            }
        }
        break;
    case ACK_QUERY_SINGLE_INFO_SLOT:
        if (_retryCount > _maxRetryCount) {
            _sendError(MaxRetryExceeded, tr("Query single info slot failed, maximum retries exceeded."));
            _finishTransaction(false);
        } else {
            _retryCount++;
            qCDebug(PlanManagerLog) << QStringLiteral("Retrying %1 MISSION_COUNT retry Count").arg(_planTypeString()) << _retryCount;
            switch (_transactionInProgress) {
            case TransactionRead:
                _querySingleInfoSlot(_lastBankIdRead, _lastInfoSlotIdRead);
                break;
            case TransactionWrite:
                _querySingleInfoSlot(_lastBankIdWrite, _lastInfoSlotIdWrite);
                break;
            default:
                break;
            }

        }
    case ACK_SET_SINGLE_BANK:
        break;
    case ACK_REFACTOR_INFO_SLOT:
        if (_retryCount > _maxRetryCount) {
            _sendError(MaxRetryExceeded, tr("Mission refactor bank failed, maximum retries exceeded."));
            _finishTransaction(false);
        } else {
            _retryCount++;
            qCDebug(PlanManagerLog) << QStringLiteral("Retrying %1 MISSION_COUNT retry Count").arg(_planTypeString()) << _retryCount;
            SingleBankInfo* bankInfo = _mapBankInfosToWrite[_lastBankIdWrite];
            _refactorInfoSlots(bankInfo->idBank, bankInfo->nWp, bankInfo->nInfoSlot);
        }
        break;
    case ACK_BANK_ERROR:
        // MISSION_ACK expected
        if (_retryCount > _maxRetryCount) {
            _sendError(MaxRetryExceeded, tr("Mission remove all, maximum retries exceeded."));
            _finishTransaction(false);
        } else {
            _retryCount++;
            qCDebug(PlanManagerLog) << tr("Retrying %1 MISSION_CLEAR_ALL retry Count").arg(_planTypeString()) << _retryCount;
            _removeAllWorker();
        }
        break;
    case ACK_SET_SINGLE_INFO_SLOT:
        if (_retryCount > _maxRetryCount) {
            _sendError(MaxRetryExceeded, tr("Set single info slot failed, maximum retries exceeded."));
            _finishTransaction(false);
        } else {
            _retryCount++;
            qCDebug(PlanManagerLog) << QStringLiteral("Retrying SET_SINGLE_INFO_SLOT retry Count") << _retryCount
                                    << "_lastBankIdWrite" << _lastInfoSlotIdWrite << "_lastInfoSlotId" << _lastInfoSlotIdWrite;
            _setSingleInfoSlot(_mapInfoSlotsInBanksToWrite[_lastBankIdWrite][_lastInfoSlotIdWrite]);
        }
        break;
    default:
        _sendError(AckTimeoutError, tr("Vehicle did not respond to mission item communication: %1").arg(_ackTypeToString(_expectedAckShenHang)));
        _expectedAckShenHang = ACK_NONE;
        _finishTransaction(false);
    }
}

void PlanManager::_startAckTimeout(AckCommandBank ack)
{
    switch (ack) {
    case ACK_QUERY_ALL_BANK:
        _ackTimeoutTimer->setInterval(_retryTimeoutMilliseconds);
        break;
    case ACK_QUERY_SINGLE_BANK:
        _ackTimeoutTimer->setInterval(_retryTimeoutMilliseconds);
        break;
    case ACK_SET_SINGLE_BANK:
    case ACK_REFACTOR_INFO_SLOT:
    case ACK_BANK_AUTO_SW:
    case ACK_WAYPOINT_AUTO_SW:
        _ackTimeoutTimer->setInterval(_retryTimeoutMilliseconds);
        break;
    case ACK_QUERY_SINGLE_INFO_SLOT:
    case ACK_SET_SINGLE_INFO_SLOT:
        _ackTimeoutTimer->setInterval(_retryTimeoutMilliseconds);
        break;
    case ACK_BANK_ERROR:
    case ACK_NONE:
        break;
    default:
        break;
    }
    _expectedAckShenHang = ack;
    _ackTimeoutTimer->start();
}

/// Checks the received ack against the expected ack. If they match the ack timeout timer will be stopped.
/// @return true: received ack matches expected ack
bool PlanManager::_checkForExpectedAck(AckCommandBank receivedAck)
{
    if (receivedAck == _expectedAckShenHang) {
        _expectedAckShenHang = AckCommandBank::ACK_NONE;
        _ackTimeoutTimer->stop();
        return true;
    } else {
        return false;
    }
}

void PlanManager::_readTransactionComplete(void)
{
    qCDebug(PlanManagerLog) << "_readTransactionComplete read sequence complete";
    
    WeakLinkInterfacePtr weakLink = _vehicle->vehicleLinkManager()->primaryLink();
    if (!weakLink.expired()) {
//        SharedLinkInterfacePtr  sharedLink = weakLink.lock();
//        mavlink_message_t       message;

//        mavlink_msg_mission_ack_pack_chan(qgcApp()->toolbox()->mavlinkProtocol()->getSystemId(),
//                                          qgcApp()->toolbox()->mavlinkProtocol()->getComponentId(),
//                                          sharedLink->mavlinkChannel(),
//                                          &message,
//                                          _vehicle->id(),
//                                          MAV_COMP_ID_AUTOPILOT1,
//                                          MAV_MISSION_ACCEPTED,
//                                          _planType);

//        _vehicle->sendMessageOnLinkThreadSafe(sharedLink.get(), message);
    }

    _finishTransaction(true);
}

void PlanManager::_clearMissionItems(void)
{
    _clearAndDeleteReadInfoSlots();
    _infoSlotIndicesToWrite.clear();
    _infoSlotIndicesToRead.clear();
    _bankIndicesToRead.clear();
    _bankIndicesToWrite.clear();
}

void PlanManager::_shenHangMessageReceived(const ShenHangProtocolMessage& message)
{
    _retryCountShenHang = 0;
    qCDebug(PlanManagerLog) << "_shenHangMessageReceived() tyMsg0:" << message.tyMsg0 << "tyMsg1" << message.tyMsg1;
    if (message.tyMsg0 == MsgId::ACK_COMMAND_BANK)  {
        _handleAckCommandBank(message);
    } else if (message.tyMsg0 == MsgId::WAYPOINT_INFO_SLOT/* && message.tyMsg1 == ACK_QUERY_SINGLE_INFO_SLOT*/) {
        _handleAckInfoSlot(message);
    }
}

void PlanManager::_handleAllBankInfo(const ShenHangProtocolMessage& message)
{
    if (!_checkForExpectedAck(AckCommandBank::ACK_QUERY_ALL_BANK)) {
        return;
    }
    TotalBankInfo totalBankInfo = {};
    memcpy(&totalBankInfo, message.payload, sizeof(totalBankInfo));
    emit totalBankInfoAvailbale(totalBankInfo);
    qCDebug(PlanManagerLog) << "_handleAllBankInfo largeBankInfslCapacity:" <<  totalBankInfo.largeBankInfoSlotCapacity
             << "smallBankInfoslCapacity:" << totalBankInfo.smallBankInfoSlotCapacity
             << "largeBankNumber:" << totalBankInfo.largeBankNumber
             << "smallBankNumber:" << totalBankInfo.smallBankNumber
             << "idTransientBank:" << totalBankInfo.idTransientBank;


    _retryCount = 0;
    switch (_transactionInProgress) {
    case TransactionRead:
        for (uint16_t i = 0; i < totalBankInfo.largeBankNumber; i++) {
            _bankIndicesToRead << i;
        }
        _lastBankIdRead = _bankIndicesToRead[0];
        _querySingleBankInfo(_bankIndicesToRead[0]);
        break;
    case TransactionWrite:
        if (totalBankInfo.largeBankNumber < _mapInfoSlotsInBanksToWrite.count()) {
            // TODO 增加提示
            return;
        }
        _bankIndicesToWrite = _mapBankInfosToWrite.keys();
        _lastBankIdWrite = _bankIndicesToWrite[0];
        _querySingleBankInfo(_bankIndicesToWrite[0]);
        break;
    default:
        break;
    }
}

void PlanManager::_handleAckCommandBank(const ShenHangProtocolMessage& message)
{
    SingleBankInfo* singleBankInfo = nullptr;
    SingleBankCheckInfo refactorInfoSlot = {};
    ErrorBankInfo errorBankInfo = {};
    QList<uint16_t> infoSlotIndicesInBank;
    SetSingleBank setSingleBank = {};
    bool checkPass = false;
    switch (message.tyMsg1)
    {
    case ACK_QUERY_ALL_BANK:
        _handleAllBankInfo(message);
        break;
    case ACK_QUERY_SINGLE_BANK:
        singleBankInfo = new SingleBankInfo;
        memcpy(singleBankInfo, message.payload, sizeof(*singleBankInfo));
        qCDebug(PlanManagerLog)  << "ACK_QUERY_SINGLE_BANK idBank" << singleBankInfo->idBank
                                 << "nWp" << singleBankInfo->nWp << "nInfoSlot" << singleBankInfo->nInfoSlot;
        if (!_checkForExpectedAck(AckCommandBank::ACK_QUERY_SINGLE_BANK)) {
            return;
        }
        switch (_transactionInProgress) {
        case TransactionNone:
            break;
        case TransactionRead:
            _mapBankInfosToWrite[singleBankInfo->idBank] = singleBankInfo;
            for (uint16_t i = 0; i < singleBankInfo->nInfoSlot; i++) {
                infoSlotIndicesInBank << i;
            }
            _mapInfoSlotIndicesToRead[singleBankInfo->idBank] = infoSlotIndicesInBank;
            _bankIndicesToRead.removeOne(singleBankInfo->idBank);
            if (_bankIndicesToRead.count() > 0)
                _querySingleBankInfo(_bankIndicesToRead[0]);
            else {  // 开始读infoslot
                qCDebug(PlanManagerLog) << "!!! READ ACK_QUERY_SINGLE_BANK Read Completed!!! start to read insfo slot!!!";
                _bankIndicesToRead.clear();     // 重新填充，用于逐个读取每个bank的infoslot
                _bankIndicesToRead = _mapInfoSlotIndicesToRead.keys();
                _lastBankIdRead = _bankIndicesToRead[0];
                _infoSlotIndicesToRead.clear();
                _infoSlotIndicesToRead = _mapInfoSlotIndicesToRead[_lastBankIdRead];
                _lastInfoSlotIdRead = _mapInfoSlotIndicesToRead[_lastBankIdRead][0];
                if (_infoSlotIndicesToRead.count() > 0) // 有infoslot就读，没有就读下一个bank信息
                    _querySingleInfoSlot(_lastBankIdRead, _lastInfoSlotIdRead);
            }
            break;
        case TransactionWrite:
            // u8变量，对应航线状态,bit0 0未装载，1装载；bit1 0未校验，1已校验；bit2 0未锁定，1已锁定；bit3 0空闲，1占用；bit4：7 保留
            if ((singleBankInfo->stateBank & 0x08) >> 3 == 1) { // bit3 0空闲，1占用；占用中的bank不可更新
                qCDebug(PlanManagerLog) << "current bank is occupied id:" << singleBankInfo->idBank;
                // TODO 弹出提示窗口，并终止任务传输
//                return;
            } else if ((singleBankInfo->stateBank & 0x04) >> 2 == 1) { // bit2 0未锁定，1已锁定；锁定中的bank需先解除锁定后再更新
                qCDebug(PlanManagerLog) << "current bank is locked id:" << singleBankInfo->idBank;
                // TODO 弹出提示窗口，并终止任务传输
//                return;
            }
            _bankIndicesToWrite.removeOne(singleBankInfo->idBank);
            if (_bankIndicesToWrite.count() > 0)
                _querySingleBankInfo(_bankIndicesToWrite[0]);
            else {  // 开始上传infoslot
                qCDebug(PlanManagerLog) << "!!! ACK_QUERY_SINGLE_BANK Write Completed!!! start to write insfo slot!!!";
                _bankIndicesToWrite = _mapBankInfosToWrite.keys();// 重新填充，用于逐个读取每个bank的infoslot
                _lastBankIdWrite = _bankIndicesToWrite[0];
                _infoSlotIndicesToWrite = _mapInfoSlotsInBanksToWrite[_lastBankIdWrite].keys();
                _lastInfoSlotIdWrite = _infoSlotIndicesToWrite[0];
                _setSingleInfoSlot(_mapInfoSlotsInBanksToWrite[_lastBankIdWrite][_lastInfoSlotIdWrite]);
            }
            break;
        case TransactionRemoveAll:
            break;
        }
        break;
    case ACK_SET_SINGLE_BANK:
        singleBankInfo = new SingleBankInfo{};
        memcpy(singleBankInfo, message.payload, sizeof(*singleBankInfo));
        qCDebug(PlanManagerLog) << "ACK_SET_SINGLE_BANK idBank" << singleBankInfo->idBank;
        if (!_checkForExpectedAck(AckCommandBank::ACK_SET_SINGLE_BANK)) {
            return;
        }
        if(_transactionInProgress != TransactionWrite)
            return;
        _bankIndicesToWrite.removeOne(singleBankInfo->idBank);
        if (_bankRefactored) {   // 已经重构过，开始写入下一个航线
            _bankRefactored = false;
            if (_bankIndicesToWrite.isEmpty()) { // 所有infoSlot都已上传
                _finishTransaction(true);
                return;
            } else {            // 开始写入下一个航线
                _lastBankIdWrite = _bankIndicesToWrite[0];
                _infoSlotIndicesToWrite = _mapInfoSlotsInBanksToWrite[_lastBankIdWrite].keys();// 重新填充，用于逐个读取每个bank的infoslot
                _lastInfoSlotIdWrite = _infoSlotIndicesToWrite[0];

                WaypointInfoSlot* infoSlot = _mapInfoSlotsInBanksToWrite[_lastBankIdWrite][_lastInfoSlotIdWrite];
                _setSingleInfoSlot(infoSlot);
            }
        }
        if(singleBankInfo) {
            delete singleBankInfo;
            singleBankInfo = nullptr;
        }
        break;
    case ACK_WAYPOINT_AUTO_SW:
        break;
    case ACK_BANK_AUTO_SW:
        break;
    case ACK_REFACTOR_INFO_SLOT:
        if (_transactionInProgress != TransactionWrite) // 只有上传才有重构
            return;
        memcpy(&refactorInfoSlot, message.payload, sizeof(refactorInfoSlot));
        singleBankInfo = _mapBankInfosToWrite[_lastBankIdWrite];
        // WARNING refactorInfoSlot有个bank的crc32校验码，对应的是否为singleBankInfo的校验
        if (refactorInfoSlot.idBank != singleBankInfo->idBank || refactorInfoSlot.nWp != singleBankInfo->nWp
                || refactorInfoSlot.nInfoSlot != singleBankInfo->nInfoSlot) {
            checkPass = false;
        } else
            checkPass = true;
        _bankRefactored = true;

        setSingleBank.idBank = singleBankInfo->idBank;
        setSingleBank.idBankSuc = singleBankInfo->idBankSuc;
        setSingleBank.idBankIWpSuc = singleBankInfo->idBankIwpSuc;
        setSingleBank.actBankEnd = singleBankInfo->actBankEnd;
        setSingleBank.flagBankVerified = checkPass;
        setSingleBank.flagBankLock = (singleBankInfo->stateBank & 0x04) >> 3;    // bit2 0未锁定；1已锁定
        _setSingleBankInfo(setSingleBank);
        break;
    case ACK_QUERY_SINGLE_INFO_SLOT:
        qCDebug(PlanManagerLog) << "ACK_QUERY_SINGLE_INFO_SLOT";
        break;
    case ACK_SET_SINGLE_INFO_SLOT:
        qCDebug(PlanManagerLog) << "ACK_SET_SINGLE_INFO_SLOT";
        break;
    case ACK_BANK_ERROR:
        qCDebug(PlanManagerLog) << "ACK_BANK_ERROR";
        memcpy(&errorBankInfo, message.payload, sizeof(errorBankInfo));
        break;
    default:
        break;
    }
}

void PlanManager::_handleSingleBankInfo(const ShenHangProtocolMessage& message)
{
    SingleBankInfo singleBankInfo = {};
    memcpy(&singleBankInfo, message.payload, sizeof (singleBankInfo));

    if (_bankIndicesToRead.contains(_currentSingleBankInfo.idBank)) {
        _bankIndicesToRead.removeOne(_currentSingleBankInfo.idBank);
        for (uint16_t i = 0; i < singleBankInfo.nInfoSlot; i++) {
            _infoSlotIndicesToRead << i;
        }
        _infoSlotCountToRead = _infoSlotIndicesToRead.count();
        _querySingleInfoSlot(singleBankInfo.idBank, _infoSlotIndicesToRead[0]);
    }
    _checkForExpectedAck(AckCommandBank::ACK_QUERY_SINGLE_BANK);
}

//void PlanManager::_handleInfoSlot(const ShenHangProtocolMessage& message)
//{
//    WaypointInfoSlot* waypointInfoSlot = new WaypointInfoSlot;
//    memcpy(waypointInfoSlot, message.payload, sizeof(*waypointInfoSlot));
//    uint16_t idBank = waypointInfoSlot->idBank;
//    uint16_t seq = waypointInfoSlot->idInfosl;
//    if (_infoSlotIndicesToRead.contains(waypointInfoSlot->idInfosl)) {
//        _infoSlotIndicesToRead.removeOne(waypointInfoSlot->idInfosl);
//        _mapInfoSlots[idBank][seq] = waypointInfoSlot;
//    } else {
//        return;
//    }
//    emit progressPct((double)seq / (double)_infoSlotCountToRead);
//    _retryCountShenHang = 0;
//    if (_infoSlotIndicesToRead.count() > 0) {
//        _lastInfoSlotIdRequested = _infoSlotIndicesToRead[0];
//        _querySingleInfoSlot(_lastBankIdRequested, _infoSlotIndicesToRead[0]);
//    }
//    else {  // 已经读完当前航线中的航点，开始读下一个航线
//        _lastBankIdRequested = ++idBank;
//        _infoSlotCountToRead = 0;
//        // TODO: 怎么判断航线（bank）已经读完
//        _retryCount = 0;
//        _lastBankIdRequested = _bankIndicesToRead[0];
//        _querySingleBankInfo(_bankIndicesToRead[0]);// 测试假设有两条航线直接读第2条航线
//    }
//    _checkForExpectedAck(AckCommandBank::ACK_QUERY_SINGLE_INFO_SLOT);
//}

void PlanManager::_sendError(ErrorCode_t errorCode, const QString& errorMsg)
{
    qCDebug(PlanManagerLog) << QStringLiteral("Sending error - _planTypeString(%1) errorCode(%2) errorMsg(%4)").arg(_planTypeString()).arg(errorCode).arg(errorMsg);

    emit error(errorCode, errorMsg);
}

QString PlanManager::_ackTypeToString(AckCommandBank ackType)
{
    switch (ackType) {
    case ACK_NONE:
        return QString("No Ack");
    case ACK_QUERY_ALL_BANK:
        return QString("ACK_QUERY_ALL");
    case ACK_QUERY_SINGLE_BANK:
        return QString("ACK_QUERY_SINGLE_BANK");
    case ACK_SET_SINGLE_BANK:
        return QString("ACK_SET_SINGLE_BANK");
    case ACK_REFACTOR_INFO_SLOT:
        return QString("ACK_REFACTOR_INFO_SLOT");
    case ACK_QUERY_SINGLE_INFO_SLOT:
        return QString("ACK_QUERY_SINGLE_INFO_SLOT");
    case ACK_BANK_AUTO_SW:
        return QString("ACK_BANK_AUTO_SW");
    case ACK_WAYPOINT_AUTO_SW:
        return QString("ACK_WAYPOINT_AUTO_SW");
    case ACK_BANK_ERROR:
        return QString("ACK_BANK_ERROR");
    default:
        return QString("");
    }
    return QString("");
}

QString PlanManager::_missionResultToString(MAV_MISSION_RESULT result)
{
    QString error;

    switch (result) {
    case MAV_MISSION_ACCEPTED:
        error = tr("Mission accepted.");
        break;
    case MAV_MISSION_ERROR:
        error = tr("Unspecified error.");
        break;
    case MAV_MISSION_UNSUPPORTED_FRAME:
        error = tr("Coordinate frame is not supported.");
        break;
    case MAV_MISSION_UNSUPPORTED:
        error = tr("Command is not supported.");
        break;
    case MAV_MISSION_NO_SPACE:
        error = tr("Mission item exceeds storage space.");
        break;
    case MAV_MISSION_INVALID:
        error = tr("One of the parameters has an invalid value.");
        break;
    case MAV_MISSION_INVALID_PARAM1:
        error = tr("Param 1 invalid value.");
        break;
    case MAV_MISSION_INVALID_PARAM2:
        error = tr("Param 2 invalid value.");
        break;
    case MAV_MISSION_INVALID_PARAM3:
        error = tr("Param 3 invalid value.");
        break;
    case MAV_MISSION_INVALID_PARAM4:
        error = tr("Param 4 invalid value.");
        break;
    case MAV_MISSION_INVALID_PARAM5_X:
        error = tr("Param 5 invalid value.");
        break;
    case MAV_MISSION_INVALID_PARAM6_Y:
        error = tr("Param 6 invalid value.");
        break;
    case MAV_MISSION_INVALID_PARAM7:
        error = tr("Param 7 invalid value.");
        break;
    case MAV_MISSION_INVALID_SEQUENCE:
        error = tr("Received mission item out of sequence.");
        break;
    case MAV_MISSION_DENIED:
        error = tr("Not accepting any mission commands.");
        break;
    default:
        qWarning(PlanManagerLog) << QStringLiteral("Fell off end of switch statement %1 %2").arg(_planTypeString()).arg(result);
        error = tr("Unknown error: %1.").arg(result);
        break;
    }

    QString lastRequestString/* = _lastMissionReqestString(result)*/;
    if (!lastRequestString.isEmpty()) {
        error += QStringLiteral(" ") + lastRequestString;
    }

    return error;
}

void PlanManager::_finishTransaction(bool success, bool apmGuidedItemWrite)
{
    qCDebug(PlanManagerLog) << "_finishTransaction success:" << success;
    emit progressPct(1);
    _disconnectFromShenHangVehicle();

    _infoSlotIndicesToRead.clear();
    _bankIndicesToRead.clear();
    _bankIndicesToWrite.clear();
    _infoSlotIndicesToWrite.clear();
    // First thing we do is clear the transaction. This way inProgesss is off when we signal transaction complete.
    TransactionType_t currentTransactionType = _transactionInProgress;
    _setTransactionInProgress(TransactionNone);

    switch (currentTransactionType) {
    case TransactionRead:
        if (!success) {
            // Read from vehicle failed, clear partial list
            _clearAndDeleteReadInfoSlots();
        }
        emit newMissionItemsAvailable(false);
        break;
    case TransactionWrite:
        if (success) {
            // Write succeeded, update internal list to be current
            if (_planType == MAV_MISSION_TYPE_MISSION) {
                _currentMissionIndex = -1;
                _lastCurrentIndex = -1;
                emit currentIndexChanged(-1);
                emit lastCurrentIndexChanged(-1);
            }
        }
        _clearAndDeleteWriteInfoSlots();
        emit sendComplete(!success /* error */);
        break;
    case TransactionRemoveAll:
        emit removeAllComplete(!success /* error */);
        break;
    default:
        break;
    }

    if (_resumeMission) {
        _resumeMission = false;
        if (success) {
            emit resumeMissionReady();
        } else {
            emit resumeMissionUploadFail();
        }
    }
}

bool PlanManager::inProgress(void) const
{
    return _transactionInProgress != TransactionNone;
}

void PlanManager::_removeAllWorker(void)
{
    qCDebug(PlanManagerLog) << "_removeAllWorker";

    emit progressPct(0);

    _connectToShenHangVehicle();

    WeakLinkInterfacePtr weakLink = _vehicle->vehicleLinkManager()->primaryLink();
    if (!weakLink.expired()) {
//        mavlink_message_t       message;
//        SharedLinkInterfacePtr  sharedLink = weakLink.lock();

//        mavlink_msg_mission_clear_all_pack_chan(qgcApp()->toolbox()->mavlinkProtocol()->getSystemId(),
//                                                qgcApp()->toolbox()->mavlinkProtocol()->getComponentId(),
//                                                sharedLink->mavlinkChannel(),
//                                                &message,
//                                                _vehicle->id(),
//                                                MAV_COMP_ID_AUTOPILOT1,
//                                                _planType);
//        _vehicle->sendMessageOnLinkThreadSafe(sharedLink.get(), message);
    }
//    _startAckTimeout(AckMissionClearAll);
}

void PlanManager::removeAll(void)
{
    if (inProgress()) {
        return;
    }
    qCDebug(PlanManagerLog) << QStringLiteral("removeAll %1").arg(_planTypeString());
    _clearAndDeleteReadInfoSlots();
    if (_planType == MAV_MISSION_TYPE_MISSION) {
        _currentMissionIndex = -1;
        _lastCurrentIndex = -1;
        emit currentIndexChanged(-1);
        emit lastCurrentIndexChanged(-1);
    }
    _retryCount = 0;
    _setTransactionInProgress(TransactionRemoveAll);
    _removeAllWorker();
}

void PlanManager::packCommandBankQueryAll(ShenHangProtocolMessage &msg)
{
    msg.tyMsg0 = COMMAND_BANK;
    msg.tyMsg1 = QUERY_ALL_BANK;
    msg.idSource = qgcApp()->toolbox()->shenHangProtocol()->getSystemId();
    msg.idTarget = _vehicle->id();
    memset(msg.payload, 0, sizeof(msg.payload));
}

void PlanManager::packCommandBankQuerySingle(ShenHangProtocolMessage &msg, uint16_t idBank)
{
    msg.tyMsg0 = COMMAND_BANK;
    msg.tyMsg1 = QUERY_SINGLE_BANK;
    msg.idSource = qgcApp()->toolbox()->shenHangProtocol()->getSystemId();
    msg.idTarget = _vehicle->id();
    memset(msg.payload, 0, sizeof(msg.payload));
    memcpy(msg.payload, &idBank, 2);
}

void PlanManager::packCommandBankSetSingle(ShenHangProtocolMessage &msg, uint16_t idBank, uint16_t idBankSuc, uint16_t idBankIwpSuc, uint16_t actBankEnd, uint8_t flagBankVerified, uint8_t flagBankLock)
{
    msg.tyMsg0 = COMMAND_BANK;
    msg.tyMsg1 = SET_SINGLE_BANK;
    msg.idSource = qgcApp()->toolbox()->shenHangProtocol()->getSystemId();
    msg.idTarget = _vehicle->id();
    memset(msg.payload, 0, sizeof(msg.payload));
    memcpy(msg.payload, &idBank, 2);
    memcpy(msg.payload + 2, &idBankSuc, 2);
    memcpy(msg.payload + 4, &idBankIwpSuc, 2);
    memcpy(msg.payload + 6, &actBankEnd, 2);
    msg.payload[8] = flagBankVerified;
    msg.payload[9] = flagBankLock;
}

void PlanManager::packRefactorInfoSlot(ShenHangProtocolMessage &msg, uint16_t idBank, uint16_t nWp, uint16_t nInfoSlot)
{
    msg.tyMsg0 = COMMAND_BANK;
    msg.tyMsg1 = REFACTOR_INFO_SLOT;
    msg.idSource = qgcApp()->toolbox()->shenHangProtocol()->getSystemId();
    msg.idTarget = _vehicle->id();
    memset(msg.payload, 0, sizeof(msg.payload));
    memcpy(msg.payload, &idBank, 2);
    memcpy(msg.payload + 2, &nWp, 2);
    memcpy(msg.payload + 4, &nInfoSlot, 2);
}

void PlanManager::packQuerySingleInfoSlot(ShenHangProtocolMessage &msg, uint16_t idBank, uint16_t idInfoSlot)
{
    msg.tyMsg0 = COMMAND_BANK;
    msg.tyMsg1 = QUERY_SINGLE_INFO_SLOT;
    msg.idSource = qgcApp()->toolbox()->shenHangProtocol()->getSystemId();
    msg.idTarget = _vehicle->id();
    memset(msg.payload, 0, sizeof(msg.payload));
    memcpy(msg.payload, &idBank, 2);
    memcpy(msg.payload + 2, &idInfoSlot, 2);
}

void PlanManager::packSetSingleInfoSlot(ShenHangProtocolMessage &msg, WaypointInfoSlot* infoSlot)
{
    msg.tyMsg0 = WAYPOINT_INFO_SLOT;
    msg.idSource = qgcApp()->toolbox()->shenHangProtocol()->getSystemId();
    msg.idTarget = _vehicle->id();
    memcpy(msg.payload, infoSlot, sizeof(*infoSlot));
}

void PlanManager::packSetSingleBank(ShenHangProtocolMessage &msg, SetSingleBank setSingleBank)
{
    msg.tyMsg0 = COMMAND_BANK;
    msg.tyMsg1 = SET_SINGLE_BANK;
    msg.idSource = qgcApp()->toolbox()->shenHangProtocol()->getSystemId();
    msg.idTarget = _vehicle->id();
    memcpy(msg.payload, &setSingleBank, sizeof(setSingleBank));
}

void PlanManager::_clearAndDeleteReadInfoSlots(void)
{
    foreach (uint16_t idBank, _mapInfoSlotsInBanksRead.keys()) {
        // Using deleteLater here causes too much transient memory to stack up
        foreach (uint16_t idInfoSlot, _mapInfoSlotsInBanksRead[idBank].keys()) {
            delete _mapInfoSlotsInBanksRead[idBank].take(idInfoSlot);
        }
        _mapInfoSlotsInBanksRead.take(idBank);
    }
    foreach (uint16_t idBank, _mapBankInfosRead.keys()) {
        delete _mapBankInfosRead.take(idBank);
    }
}


void PlanManager::_clearAndDeleteWriteInfoSlots(void)
{
    foreach (uint16_t idBank, _mapInfoSlotsInBanksToWrite.keys()) {
        // Using deleteLater here causes too much transient memory to stack up
        foreach (uint16_t idInfoSlot, _mapInfoSlotsInBanksToWrite[idBank].keys()) {
            delete _mapInfoSlotsInBanksToWrite[idBank].take(idInfoSlot);
        }
        _mapInfoSlotsInBanksToWrite.take(idBank);
    }
    foreach (uint16_t idBank, _mapBankInfosToWrite.keys()) {
        delete _mapBankInfosToWrite.take(idBank);
    }
}

void PlanManager::_connectToShenHangVehicle()
{
    QMetaObject::Connection connection = connect(_vehicle, &Vehicle::shenHangMessageReceived, this, &PlanManager::_shenHangMessageReceived);
    qCDebug(PlanManagerLog) << "PlanManager::_connectToShenHangVehicle()" << _vehicle->id() << _vehicle << "connection:" << connection;
}

void PlanManager::_disconnectFromShenHangVehicle()
{
    disconnect(_vehicle, &Vehicle::shenHangMessageReceived, this, &PlanManager::_shenHangMessageReceived);
}

void PlanManager::_handleShenHangBankMessage(const ShenHangProtocolMessage& msg)
{
    TotalBankInfo totalBankInfo = {};
    SingleBankInfo singleBankInfo = {};
    SingleBankCheckInfo refactorInfoSlot = {};
    struct WaypointInfoSlot waypointInfoSlot = {};
    ErrorBankInfo errorBankInfo = {};
    switch (msg.tyMsg1)
    {
    case ACK_QUERY_ALL_BANK:
        memcpy(&totalBankInfo, msg.payload, sizeof(totalBankInfo));
        break;
    case ACK_QUERY_SINGLE_BANK:
    case ACK_SET_SINGLE_BANK:
    case ACK_BANK_AUTO_SW:
    case ACK_WAYPOINT_AUTO_SW:
        memcpy(&singleBankInfo, msg.payload, sizeof(singleBankInfo));
        break;
    case REFACTOR_INFO_SLOT:
        memcpy(&refactorInfoSlot, msg.payload, sizeof(refactorInfoSlot));
        break;
    case ACK_QUERY_SINGLE_INFO_SLOT:
        memcpy(&waypointInfoSlot, msg.payload, sizeof(waypointInfoSlot));
//        waypoints
        break;
    case ACK_BANK_ERROR:
        memcpy(&errorBankInfo, msg.payload, sizeof(errorBankInfo));
        break;
    default:
        break;
    }
}

QString PlanManager::_planTypeString(void)
{
    switch (_planType) {
    case MAV_MISSION_TYPE_MISSION:
        return QStringLiteral("T:Mission");
    case MAV_MISSION_TYPE_FENCE:
        return QStringLiteral("T:GeoFence");
    case MAV_MISSION_TYPE_RALLY:
        return QStringLiteral("T:Rally");
    default:
        qWarning() << "Unknown plan type" << _planType;
        return QStringLiteral("T:Unknown");
    }
}

void PlanManager::_queryAllBanks()
{
    qCDebug(PlanManagerLog) << "_queryAllBanks _retryCount" << _retryCount;
    _clearMissionItems();
    WeakLinkInterfacePtr weakLink = _vehicle->vehicleLinkManager()->primaryLink();
    if (!weakLink.expired()) {
        SharedLinkInterfacePtr  sharedLink = weakLink.lock();
        ShenHangProtocolMessage msg = {};
        _vehicle->PackCommandBankQueryAll(msg);
        _vehicle->sendShenHangMessageOnLinkThreadSafe(sharedLink.get(), msg);
    }        
    _startAckTimeout(ACK_QUERY_ALL_BANK);
}

void PlanManager::_querySingleBankInfo(uint16_t idBank)
{
    qCDebug(PlanManagerLog) << "_querySingleBankInfo idBank" << idBank;
    WeakLinkInterfacePtr weakLink = _vehicle->vehicleLinkManager()->primaryLink();
    if (!weakLink.expired()) {
        SharedLinkInterfacePtr  sharedLink = weakLink.lock();
        ShenHangProtocolMessage msg = {};
        _vehicle->PackCommandBankQuerySingle(msg, idBank);
        _vehicle->sendShenHangMessageOnLinkThreadSafe(sharedLink.get(), msg);
    }
    _startAckTimeout(ACK_QUERY_SINGLE_BANK);
}

void PlanManager::_setSingleBankInfo(SetSingleBank setSingleBank)
{
    qCDebug(PlanManagerLog) << "_setSingleBankInfo idBank" << setSingleBank.idBank;
    WeakLinkInterfacePtr weakLink = _vehicle->vehicleLinkManager()->primaryLink();
    if (!weakLink.expired()) {
        SharedLinkInterfacePtr  sharedLink = weakLink.lock();
        ShenHangProtocolMessage msg = {};
        packSetSingleBank(msg, setSingleBank);
        _vehicle->sendShenHangMessageOnLinkThreadSafe(sharedLink.get(), msg);
    }
    _startAckTimeout(ACK_SET_SINGLE_BANK);
}

void PlanManager::_refactorInfoSlots(uint16_t idBank, uint16_t nWp, uint16_t nInfoSlot)
{
    qCDebug(PlanManagerLog) << "_refactorInfoSlots" << idBank << nWp << nInfoSlot;
    WeakLinkInterfacePtr weakLink = _vehicle->vehicleLinkManager()->primaryLink();
    if (!weakLink.expired()) {
        SharedLinkInterfacePtr  sharedLink = weakLink.lock();
        ShenHangProtocolMessage msg = {};
        _vehicle->PackRefactorInfoSlot(msg, idBank, nWp, nInfoSlot);
        _vehicle->sendShenHangMessageOnLinkThreadSafe(sharedLink.get(), msg);
    }
    _startAckTimeout(ACK_REFACTOR_INFO_SLOT);
}

void PlanManager::_querySingleInfoSlot(uint16_t idBank, uint16_t idInfoSlot)
{
    qCDebug(PlanManagerLog) << "_querySingleInfoSlot idBank" << idBank << "idInfoSlot" << idInfoSlot;
    WeakLinkInterfacePtr weakLink = _vehicle->vehicleLinkManager()->primaryLink();
    if (!weakLink.expired()) {
        SharedLinkInterfacePtr  sharedLink = weakLink.lock();
        ShenHangProtocolMessage msg = {};
        _vehicle->PackQuerySingleInfoSlot(msg, idBank, idInfoSlot);
        _vehicle->sendShenHangMessageOnLinkThreadSafe(sharedLink.get(), msg);
    }
    _startAckTimeout(ACK_QUERY_SINGLE_INFO_SLOT);
}

void PlanManager::_setSingleInfoSlot(WaypointInfoSlot *infoSlot)
{
    qCDebug(PlanManagerLog) << "_setSingleInfoSlot idBank" << infoSlot->idBank << "idInfoSlot" << infoSlot->idInfoSlot;
    WeakLinkInterfacePtr weakLink = _vehicle->vehicleLinkManager()->primaryLink();
    if (!weakLink.expired()) {
        SharedLinkInterfacePtr  sharedLink = weakLink.lock();
        ShenHangProtocolMessage msg = {};
        packSetSingleInfoSlot(msg, infoSlot);
        _vehicle->sendShenHangMessageOnLinkThreadSafe(sharedLink.get(), msg);
    }
    _startAckTimeout(ACK_SET_SINGLE_INFO_SLOT);
}

void PlanManager::_setTransactionInProgress(TransactionType_t type)
{
    if (_transactionInProgress  != type) {
        qCDebug(PlanManagerLog) << "_setTransactionInProgress" << _planTypeString() << type;
        _transactionInProgress = type;
        emit inProgressChanged(inProgress());
    }
}
