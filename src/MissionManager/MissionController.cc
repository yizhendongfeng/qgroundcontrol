/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "MissionController.h"
#include "MultiVehicleManager.h"
#include "MissionManager.h"
#include "FlightPathSegment.h"
#include "FirmwarePlugin.h"
#include "QGCApplication.h"
#include "SimpleMissionItem.h"
#include "ItemInfoSlot.h"
#include "JsonHelper.h"
#include "ParameterManager.h"
#include "QGroundControlQmlGlobal.h"
#include "SettingsManager.h"
#include "AppSettings.h"
#include "QGCQGeoCoordinate.h"
#include "PlanMasterController.h"
#include "QGCCorePlugin.h"
#include "PlanViewSettings.h"
//#define FINDCOUNT
#define UPDATE_TIMEOUT 5000 ///< How often we check for bounding box changes

QGC_LOGGING_CATEGORY(MissionControllerLog, "MissionControllerLog")

const char* MissionController::_settingsGroup                 = "MissionController";
const char* MissionController::_jsonKeyFileType               = "Mission";
const char* MissionController::_jsonKeyFirmwareType           = "firmwareType";
const char* MissionController::_jsonKeyVehicleType            = "vehicleType";
const char* MissionController::_jsonKeyCruiseSpeed            = "cruiseSpeed";
const char* MissionController::_jsonKeyBankInfo               = "bankInfo";
const char* MissionController::_jsonKeyInfoSlots              = "infoSlots";
const char* MissionController::_jsonKeyPlannedHomePosition    = "plannedHomePosition";
const char* MissionController::_jsonKeyHoverSpeed             = "hoverSpeed";
const char* MissionController::_jsonKeyGlobalPlanAltitudeMode = "globalPlanAltitudeMode";

const char* MissionController::_jsonKeyIdBank         = "idBank";
const char* MissionController::_jsonKeyTypeBank       = "typeBank";
const char* MissionController::_jsonKeyNInfoSlotMax   = "nInfoSlotMax";
const char* MissionController::_jsonKeyIWp            = "iWp";
const char* MissionController::_jsonKeyNWp            = "nWp";
const char* MissionController::_jsonKeyNInfoSlot      = "nInfoSlot";
const char* MissionController::_jsonKeyIdBankSuc      = "idBankSuc";
const char* MissionController::_jsonKeyIdBankIWpSuc   = "idBankIWpSuc";
const char* MissionController::_jsonKeyActBankEnd     = "actBankEnd";
const char* MissionController::_jsonKeyStateBank      = "stateBank";
const char* MissionController::_jsonKeySwitchState    = "switchState";

const char* MissionController::_jsonKeyIdWp           = "idWp";
const char* MissionController::_jsonKeyIdInfoSlot     = "idInfoSlot";
const char* MissionController::_jsonKeyTypeInfoSlot   = "tyInfoSlot";
const char* MissionController::_jsonKeyLon            = "lon";
const char* MissionController::_jsonKeyLat            = "lat";
const char* MissionController::_jsonKeyAlt            = "alt";
const char* MissionController::_jsonKeyRadiusCh       = "radiusCh";
const char* MissionController::_jsonKeyRVGnd          = "rVGnd";
const char* MissionController::_jsonKeyRRocDoc        = "rRocDoc";
const char* MissionController::_jsonKeySwTd           = "swTd";
const char* MissionController::_jsonKeySwVGnd10x      = "swVGnd10x";
const char* MissionController::_jsonKeyRHdgDeg100x    = "rHdgDeg100x";
const char* MissionController::_jsonKeyActDept        = "actDept";
const char* MissionController::_jsonKeyActArrvl       = "actArrvl";
const char* MissionController::_jsonKeySwCondSet      = "swCondSet";
const char* MissionController::_jsonKeyActInFlt       = "actInflt";
const char* MissionController::_jsonKeyHgtHdgCtrlMode = "heHdgCtrlMode";
const char* MissionController::_jsonKeySwAngDeg       = "swAngDeg";
const char* MissionController::_jsonKeySwTurns        = "swTurns";
const char* MissionController::_jsonKeyGdMode         = "gdMode";
const char* MissionController::_jsonKeyMnvrSty        = "mnvrSty";
const char* MissionController::_jsonKeyTransSty       = "transSty";
const char* MissionController::_jsonKeyReserved0      = "reserved0";
const char* MissionController::_jsonKeyReserved1      = "reserved1";

// Deprecated V1 format keys
const char* MissionController::_jsonComplexItemsKey =           "complexItems";
const char* MissionController::_jsonMavAutopilotKey =           "MAV_AUTOPILOT";

const int   MissionController::_missionFileVersion =            2;

MissionController::MissionController(PlanMasterController* masterController, QObject *parent)
    : PlanElementController (masterController, parent)
    , _controllerVehicle    (masterController->controllerVehicle())
    , _managerVehicle       (masterController->managerVehicle())
    , _missionManager       (masterController->managerVehicle()->missionManager())
    , _itemsBank            (new QmlObjectListModel(this))
    , _planViewSettings     (qgcApp()->toolbox()->settingsManager()->planViewSettings())
    , _appSettings          (qgcApp()->toolbox()->settingsManager()->appSettings())
{
    qCDebug(MissionControllerLog) << "MissionController():" << this;
    _resetMissionFlightStatus();

    _updateTimer.setSingleShot(true);

    connect(&_updateTimer,                                  &QTimer::timeout,                           this, &MissionController::_updateTimeout);
    connect(_planViewSettings->takeoffItemNotRequired(),    &Fact::rawValueChanged,                     this, &MissionController::_takeoffItemNotRequiredChanged);
    connect(this,                                           &MissionController::missionDistanceChanged, this, &MissionController::recalcTerrainProfile);

    // The follow is used to compress multiple recalc calls in a row to into a single call.
//    connect(this, &MissionController::_recalcMissionFlightStatusSignal, this, &MissionController::_recalcMissionFlightStatus,   Qt::QueuedConnection);
//    connect(this, &MissionController::_recalcFlightPathSegmentsSignal,  this, &MissionController::_recalcFlightPathSegments,    Qt::QueuedConnection);
//    connect(_missionManager, &MissionManager::totalBankInfoAvailbale, this, &MissionController::_totalBankInfoChangedFromMissionManager, Qt::QueuedConnection);
    qgcApp()->addCompressedSignal(QMetaMethod::fromSignal(&MissionController::_recalcMissionFlightStatusSignal));
    qgcApp()->addCompressedSignal(QMetaMethod::fromSignal(&MissionController::_recalcFlightPathSegmentsSignal));
    qgcApp()->addCompressedSignal(QMetaMethod::fromSignal(&MissionController::recalcTerrainProfile));
}

MissionController::~MissionController()
{
    _deinitAllVisualItems();
}

void MissionController::removeVisualItemBank(int viIndex)
{
    if (viIndex < 0 || viIndex >= _itemsBank->count()) {
        qWarning() << "MissionController::removeVisualItemBank called with bad index - count:index" << _itemsBank->count() << viIndex;
        return;
    }
    int newIndex = 0;

    qDebug() << "removeVisualItemBank viIndex:" << viIndex << "newIndex:" << newIndex;
    ItemBank* item = qobject_cast<ItemBank*>(_itemsBank->removeAt(viIndex));
    item->deleteLater();

    /******************** 修改后续bank中所有infoSlot的idBank ********************/
    for (int i = viIndex; i < _itemsBank->count(); i++) {
        ItemBank* itemBank = _itemsBank->value<ItemBank*>(i);
        itemBank->setIdBank(i);
        for (int j = 0; j < itemBank->itemsWaypoint()->count(); j++) {
             ItemWaypoint* itemWaypoint = itemBank->itemsWaypoint()->value<ItemWaypoint*>(j);
             for (int k = 0; k < itemWaypoint->itemsInfoSlot()->count(); k++) {
                 ItemInfoSlot* itemInfoSlot = itemWaypoint->itemsInfoSlot()->value<ItemInfoSlot*>(k);
                 itemInfoSlot->setIdBank(static_cast<uint16_t>(i));
             }
        }
    }
    _indexCurrentBank = newIndex;
    emit indexCurrentBankChanged();
    setDirty(true);
}

void MissionController::_resetMissionFlightStatus(void)
{
    _missionFlightStatus.totalDistance =        0.0;
    _missionFlightStatus.maxTelemetryDistance = 0.0;
    _missionFlightStatus.totalTime =            0.0;
    _missionFlightStatus.hoverTime =            0.0;
    _missionFlightStatus.cruiseTime =           0.0;
    _missionFlightStatus.hoverDistance =        0.0;
    _missionFlightStatus.cruiseDistance =       0.0;
    _missionFlightStatus.cruiseSpeed =          _controllerVehicle->defaultCruiseSpeed();
    _missionFlightStatus.hoverSpeed =           _controllerVehicle->defaultHoverSpeed();
    _missionFlightStatus.vehicleSpeed =         _controllerVehicle->multiRotor() || _managerVehicle->vtol() ? _missionFlightStatus.hoverSpeed : _missionFlightStatus.cruiseSpeed;
    _missionFlightStatus.vehicleYaw =           qQNaN();
    _missionFlightStatus.gimbalYaw =            qQNaN();
    _missionFlightStatus.gimbalPitch =          qQNaN();
    _missionFlightStatus.mAhBattery =           0;
    _missionFlightStatus.hoverAmps =            0;
    _missionFlightStatus.cruiseAmps =           0;
    _missionFlightStatus.ampMinutesAvailable =  0;
    _missionFlightStatus.hoverAmpsTotal =       0;
    _missionFlightStatus.cruiseAmpsTotal =      0;
    _missionFlightStatus.batteryChangePoint =   -1;
    _missionFlightStatus.batteriesRequired =    -1;
    _missionFlightStatus.vtolMode =             _missionContainsVTOLTakeoff ? QGCMAVLink::VehicleClassMultiRotor : QGCMAVLink::VehicleClassFixedWing;

    _controllerVehicle->firmwarePlugin()->batteryConsumptionData(_controllerVehicle, _missionFlightStatus.mAhBattery, _missionFlightStatus.hoverAmps, _missionFlightStatus.cruiseAmps);
    if (_missionFlightStatus.mAhBattery != 0) {
        double batteryPercentRemainingAnnounce = qgcApp()->toolbox()->settingsManager()->appSettings()->batteryPercentRemainingAnnounce()->rawValue().toDouble();
        _missionFlightStatus.ampMinutesAvailable = static_cast<double>(_missionFlightStatus.mAhBattery) / 1000.0 * 60.0 * ((100.0 - batteryPercentRemainingAnnounce) / 100.0);
    }

    emit missionDistanceChanged(_missionFlightStatus.totalDistance);
    emit missionTimeChanged();
    emit missionHoverDistanceChanged(_missionFlightStatus.hoverDistance);
    emit missionCruiseDistanceChanged(_missionFlightStatus.cruiseDistance);
    emit missionHoverTimeChanged();
    emit missionCruiseTimeChanged();
    emit missionMaxTelemetryChanged(_missionFlightStatus.maxTelemetryDistance);
    emit batteryChangePointChanged(_missionFlightStatus.batteryChangePoint);
    emit batteriesRequiredChanged(_missionFlightStatus.batteriesRequired);

}

void MissionController::start(bool flyView)
{
    qCDebug(MissionControllerLog) << "start flyView" << flyView;

    _managerVehicleChanged(_managerVehicle);
    connect(_masterController, &PlanMasterController::managerVehicleChanged, this, &MissionController::_managerVehicleChanged);

    PlanElementController::start(flyView);
    _init();
}

void MissionController::_init(void)
{
    qCDebug(MissionControllerLog) << "_init()";

    // We start with an empty mission
    _initAllVisualItems();
}

// Called when new mission items have completed downloading from Vehicle
void MissionController::_newMissionItemsAvailableFromVehicle(bool removeAllRequested)
{
#ifndef FINDCOUNT
    qCDebug(MissionControllerLog) << "_newMissionItemsAvailableFromVehicle flyView:" << _flyView;

    // Fly view always reloads on _loadComplete
    // Plan view only reloads if:
    //  - Load was specifically requested
    //  - There is no current Plan
    if (_flyView || removeAllRequested || _itemsRequested || isEmpty()) {
        // Fly Mode (accept if):
        //      - Always accepts new items from the vehicle so Fly view is kept up to date
        // Edit Mode (accept if):
        //      - Remove all was requested from Fly view (clear mission on flight end)
        //      - A load from vehicle was manually requested
        //      - The initial automatic load from a vehicle completed and the current editor is empty

        _deinitAllVisualItems();
        _updateContainsItems(); // This will clear containsItems which will be set again below. This will re-pop Start Mission confirmation.
        _itemsBank->deleteLater();
        _itemsBank = nullptr;

        const QMap<uint16_t, SingleBankInfo*> mapSingleBankInfo = _missionManager->mapSingleBankInfo();
        const QMap<uint16_t, QMap<uint16_t, WaypointInfoSlot*>> mapInfoSlotsInBanks = _missionManager->mapInfoSlotsInBanks();
        _itemsBank = initLoadedInfoSlotsInBanks(mapSingleBankInfo, mapInfoSlotsInBanks);
        _initAllVisualItems();
        _updateContainsItems();
        emit itemsBankChanged();
        emit newItemsFromVehicle();
    }
    _itemsRequested = false;
#endif
}

void MissionController::loadFromVehicle(void)
{
    if (_masterController->offline()) {
        qCWarning(MissionControllerLog) << "MissionControllerLog::loadFromVehicle called while offline";
    } else if (syncInProgress()) {
        qCWarning(MissionControllerLog) << "MissionControllerLog::loadFromVehicle called while syncInProgress";
    } else {
        _itemsRequested = true;
        _managerVehicle->missionManager()->loadFromVehicle();
    }
}

void MissionController::sendToVehicle(void)
{
    if (_masterController->offline()) {
        qCWarning(MissionControllerLog) << "MissionControllerLog::sendToVehicle called while offline";
    } else if (syncInProgress()) {
        qCWarning(MissionControllerLog) << "MissionControllerLog::sendToVehicle called while syncInProgress";
    } else {
        qCDebug(MissionControllerLog) << "MissionControllerLog::sendToVehicle";
        if (_itemsBank->count() > 0) {
            sendItemsToVehicle(_managerVehicle, _itemsBank);
        }
        setDirty(false);
    }
}

/// Converts from visual items to MissionItems
///     @param missionItemParent QObject parent for newly allocated MissionItems
/// @return true: Mission end action was added to end of list
bool MissionController::_convertToMissionItems(QmlObjectListModel* itemsBank, QMap<uint16_t, SingleBankInfo*>& bankInfos, QMap<uint16_t, QMap<uint16_t, WaypointInfoSlot*>>& infoSlotsInBanks, QObject* missionItemParent)
{
    if (itemsBank->count() == 0) {
        return false;
    }
    bool endActionSet = false;
    for (int i=0; i<itemsBank->count(); i++) {
        ItemBank* itemBank = itemsBank->value<ItemBank*>(i);
        bankInfos[itemBank->idBank()] = new SingleBankInfo(itemBank->bankInfo());
//        qCDebug(MissionControllerLog) << "_convertToMissionItems bankInfos idBank" <<  itemBank->bankInfo().idBank
//                                      << "nWp" << itemBank->bankInfo().nWp << "nInfoSlot" << itemBank->bankInfo().nInfoSlot;
        QList<WaypointInfoSlot*> infoSlotsInBank;
        for (int j = 0; j < itemBank->itemsWaypoint()->count(); j++) {
            ItemWaypoint* itemWaypoint = itemBank->itemsWaypoint()->value<ItemWaypoint*>(j);
            for (int k = 0; k < itemWaypoint->itemsInfoSlot()->count(); k++) {
                ItemInfoSlot* itemInfoSlot = itemWaypoint->itemsInfoSlot()->value<ItemInfoSlot*>(k);
                WaypointInfoSlot infoSlot = itemInfoSlot->infoSlot();
                infoSlotsInBanks[infoSlot.idBank][infoSlot.idInfoSlot] = new WaypointInfoSlot(itemInfoSlot->infoSlot());
                qCDebug(MissionControllerLog) << "_convertToMissionItems infoSlotsInBanks idBank" <<  infoSlot.idBank << "idInfoSlot" << infoSlot.idInfoSlot;
            }
        }
    }
    return endActionSet;
}


void MissionController::setLargeBankInfoSlotCapacity(int newLargeBankInfoSlotCapacity)
{
    if (_totalBankInfo.largeBankInfoSlotCapacity == newLargeBankInfoSlotCapacity)
        return;
    _totalBankInfo.largeBankInfoSlotCapacity = newLargeBankInfoSlotCapacity;
    emit largeBankInfoSlotCapacityChanged();
}

void MissionController::sendItemsToVehicle(Vehicle* vehicle, QmlObjectListModel* itemsBank)
{
    if (vehicle) {
        QMap<uint16_t, QMap<uint16_t, WaypointInfoSlot*>> infoSlotsInBanks;
        QMap<uint16_t, SingleBankInfo*> bankInfos;
        _convertToMissionItems(itemsBank, bankInfos, infoSlotsInBanks, vehicle);

        // PlanManager takes control of MissionItems so no need to delete
        vehicle->missionManager()->writeInfoSlots(bankInfos, infoSlotsInBanks);
    }
}

ItemBank* MissionController::insertNewBank()
{
    if (_indexCurrentBank >= 0 && _indexCurrentBank < _itemsBank->count()) {
        ItemBank* itemCurrentBank = _itemsBank->value<ItemBank*>(_indexCurrentBank);
        if (itemCurrentBank->itemCurrentWaypoint())
            itemCurrentBank->itemCurrentWaypoint()->setIsCurrentWaypoint(false);
    }
    SingleBankInfo bankInfo{};
    bankInfo.idBank = static_cast<uint16_t>(_itemsBank->count());
    ItemBank* itemNewBank = new ItemBank(bankInfo, this);
    _itemsBank->append(itemNewBank);
    qCDebug(MissionControllerLog) << "insertNewBank() idBank:" << itemNewBank->idBank();
    _indexCurrentBank = bankInfo.idBank;
    emit itemsBankChanged();
    emit indexCurrentBankChanged();
    return itemNewBank;
}

void MissionController::removeAll(void)
{
#ifndef FINDCOUNT
    qCDebug(MissionControllerLog) << "removeAll()";
    if (_itemsBank) {
        _deinitAllVisualItems();
        _itemsBank->clearAndDeleteContents();
        _itemsBank->deleteLater();

        _itemsBank = new QmlObjectListModel(this);
        _initAllVisualItems();
        setDirty(true);
        _resetMissionFlightStatus();
        _allItemsRemoved();
    }
#endif
}

QmlObjectListModel* MissionController::initLoadedInfoSlotsInBanks(const QMap<uint16_t, SingleBankInfo*>& bankInfos, const QMap<uint16_t, QMap<uint16_t, WaypointInfoSlot*>>& infoSlotsInBanks)
{
#ifndef FINDCOUNT
    qCDebug(MissionControllerLog) << "initLoadedInfoSlotsInBanks() banks" << bankInfos.keys();
    QmlObjectListModel* visualItemsBankTemp = new QmlObjectListModel(this);
    foreach (uint16_t idBank, bankInfos.keys()) {
        ItemBank* itemBank = new ItemBank(*bankInfos[idBank], visualItemsBankTemp);
        itemBank->setInfoSlots(infoSlotsInBanks[idBank]);
        connect(itemBank, &ItemBank::dirtyChanged, this, &MissionController::_itemsBankDirtyChanged);
        visualItemsBankTemp->append(itemBank);
        qCDebug(MissionControllerLog) << "initLoadedInfoSlotsInBanks() visualItemsBankTemp:" << visualItemsBankTemp << "idBank:" << itemBank->idBank() << "_flyView:" << _flyView;
        _indexCurrentBank = bankInfos[idBank]->idBank;
        emit indexCurrentBankChanged();
    }
    return visualItemsBankTemp;
#endif
}

bool MissionController::loadBankInfo(const QJsonObject &json, SingleBankInfo* bankInfo, QMap<uint16_t, WaypointInfoSlot*>& infoSlots, QString &errorString)
{
    if (!bankInfo) {
        bankInfo = new SingleBankInfo;
    }
    // Validate root object keys
    QList<JsonHelper::KeyValidateInfo> keyInfoList = {
        { _jsonKeyIdBank,       QJsonValue::Double, true },
        { _jsonKeyTypeBank,     QJsonValue::Double,  true },
        { _jsonKeyNInfoSlotMax, QJsonValue::Double, true },
        { _jsonKeyIWp,          QJsonValue::Double, true },
        { _jsonKeyNWp,          QJsonValue::Double, true },
        { _jsonKeyNInfoSlot,    QJsonValue::Double, true },
        { _jsonKeyIdBankSuc,    QJsonValue::Double, true },
        { _jsonKeyActBankEnd,   QJsonValue::Double, true },
        { _jsonKeyStateBank,    QJsonValue::Double, true },
        { _jsonKeySwitchState,  QJsonValue::Double, true },
        { _jsonKeyInfoSlots,    QJsonValue::Array, true },
    };
    if (!JsonHelper::validateKeys(json, keyInfoList, errorString)) {
        return false;
    }
    if (json[_jsonKeyIdBank].toInt() >= 0)
        bankInfo->idBank = json[_jsonKeyIdBank].toInt();
    else {
        errorString = QString(_jsonKeyIdBank) + " less than 0";
        return false;
    }

    if (json[_jsonKeyTypeBank].toInt() >= 0)
        bankInfo->typeBank = json[_jsonKeyTypeBank].toInt();
    else {
        errorString = QString(_jsonKeyTypeBank) + " less than 0";
        return false;
    }

    if (json[_jsonKeyNInfoSlotMax].toInt() >= 0)
        bankInfo->nInfoSlotMax = json[_jsonKeyNInfoSlotMax].toInt();
    else {
        errorString = QString(_jsonKeyNInfoSlotMax) + " less than 0";
        return false;
    }

    if (json[_jsonKeyIWp].toInt() >= 0)
        bankInfo->iWp = json[_jsonKeyIWp].toInt();
    else {
        errorString = QString(_jsonKeyTypeInfoSlot) + " less than 0";
        return false;
    }

    if (json[_jsonKeyNWp].toInt() >= 0)
        bankInfo->nWp = json[_jsonKeyNWp].toInt();
    else {
        errorString = QString(_jsonKeyNWp) + " less than 0";
        return false;
    }

    if (json[_jsonKeyNInfoSlot].toInt() >= 0)
        bankInfo->nInfoSlot = json[_jsonKeyNInfoSlot].toInt();
    else {
        errorString = QString(_jsonKeyNInfoSlot) + " less than 0";
        return false;
    }

    if (json[_jsonKeyIdBankSuc].toInt() >= 0)
        bankInfo->idBankSuc = json[_jsonKeyIdBankSuc].toInt();
    else {
        errorString = QString(_jsonKeyIdBankSuc) + " less than 0";
        return false;
    }

    if (json[_jsonKeyActBankEnd].toInt() >= 0)
        bankInfo->actBankEnd = json[_jsonKeyActBankEnd].toInt();
    else {
        errorString = QString(_jsonKeyActBankEnd) + " less than 0";
        return false;
    }

    if (json[_jsonKeyStateBank].toInt() >= 0)
        bankInfo->stateBank = json[_jsonKeyStateBank].toInt();
    else {
        errorString = QString(_jsonKeyStateBank) + " less than 0";
        return false;
    }

    if (json[_jsonKeySwitchState].toInt() >= 0)
        bankInfo->switchState = json[_jsonKeySwitchState].toInt();
    else {
        errorString = QString(_jsonKeySwitchState) + " less than 0";
        return false;
    }
    const QJsonArray jsonArrayInfoSlots = json[_jsonKeyInfoSlots].toArray();
    for (int i=0; i<jsonArrayInfoSlots.count(); i++) {
        // Convert to QJsonObject
        const QJsonValue& itemValue = jsonArrayInfoSlots[i];
        if (!itemValue.isObject()) {
            errorString = tr("Json infoSlot %1 is not an object").arg(i);
            return false;
        }
        const QJsonObject itemObject = itemValue.toObject();
        WaypointInfoSlot* infoSlot = new WaypointInfoSlot{};
        infoSlot->idWp           = itemObject[_jsonKeyIdWp].toInt();
        infoSlot->idBank         = itemObject[_jsonKeyIdBank].toInt();
        infoSlot->idInfoSlot     = itemObject[_jsonKeyIdInfoSlot].toInt();
        infoSlot->typeInfoSlot   = itemObject[_jsonKeyTypeInfoSlot].toInt();
        infoSlot->lon            = itemObject[_jsonKeyLon].toDouble();
        infoSlot->lat            = itemObject[_jsonKeyLat].toDouble();
        infoSlot->alt            = itemObject[_jsonKeyAlt].toDouble();
        infoSlot->radiusCh       = itemObject[_jsonKeyRadiusCh].toDouble();
        infoSlot->rVGnd          = itemObject[_jsonKeyRVGnd].toDouble();
        infoSlot->rRocDoc        = itemObject[_jsonKeyRRocDoc].toDouble();
        infoSlot->swTd           = itemObject[_jsonKeySwTd].toDouble();
        infoSlot->swVGnd10x      = itemObject[_jsonKeySwVGnd10x].toDouble();
        infoSlot->rHdgDeg100x    = itemObject[_jsonKeyRHdgDeg100x].toDouble();

        infoSlot->actDept        = itemObject[_jsonKeyActDept].toInt();
        infoSlot->actArrvl       = itemObject[_jsonKeyActArrvl].toInt();
        infoSlot->swCondSet      = itemObject[_jsonKeySwCondSet].toInt();
        infoSlot->actInflt       = itemObject[_jsonKeyActInFlt].toInt();
        infoSlot->hgtHdgCtrlMode = itemObject[_jsonKeyHgtHdgCtrlMode].toInt();
        infoSlot->swAngDeg       = itemObject[_jsonKeySwAngDeg].toInt();
        infoSlot->swTurns        = itemObject[_jsonKeySwTurns].toInt();
        infoSlot->gdMode         = itemObject[_jsonKeyGdMode].toInt();
        infoSlot->mnvrSty        = itemObject[_jsonKeyMnvrSty].toInt();
        infoSlot->transSty       = itemObject[_jsonKeyTransSty].toInt();
        infoSlot->reserved0      = itemObject[_jsonKeyReserved0].toInt();
        infoSlot->reserved1      = itemObject[_jsonKeyReserved1].toInt();
        infoSlots[infoSlot->idInfoSlot] = infoSlot;
    }
    return true;
}


bool MissionController::_loadJsonMissionFile(const QJsonObject& json, QMap<uint16_t, SingleBankInfo *>& bankInfos, QMap<uint16_t, QMap<uint16_t, WaypointInfoSlot*>> &infoSlots, QString& errorString)
{
    // Validate root object keys
    QList<JsonHelper::KeyValidateInfo> rootKeyInfoList = {
        { _jsonKeyPlannedHomePosition,      QJsonValue::Array,  true },
        { _jsonKeyBankInfo,                 QJsonValue::Array, true },
        { _jsonKeyFirmwareType,             QJsonValue::Double, true },
        { _jsonKeyVehicleType,              QJsonValue::Double, false },
        { _jsonKeyCruiseSpeed,              QJsonValue::Double, false },
        { _jsonKeyHoverSpeed,               QJsonValue::Double, false },
        { _jsonKeyGlobalPlanAltitudeMode,   QJsonValue::Double, false },
    };
    if (!JsonHelper::validateKeys(json, rootKeyInfoList, errorString)) {
        return false;
    }

    setGlobalAltitudeMode(QGroundControlQmlGlobal::AltitudeModeNone);   // Mixed mode

    AppSettings* appSettings = qgcApp()->toolbox()->settingsManager()->appSettings();

    // Get the firmware/vehicle type from the plan file
    MAV_AUTOPILOT   planFileFirmwareType =  static_cast<MAV_AUTOPILOT>(json[_jsonKeyFirmwareType].toInt());
    MAV_TYPE        planFileVehicleType =   static_cast<MAV_TYPE>     (QGCMAVLink::vehicleClassToMavType(appSettings->offlineEditingVehicleClass()->rawValue().toInt()));
    if (json.contains(_jsonKeyVehicleType)) {
        planFileVehicleType = static_cast<MAV_TYPE>(json[_jsonKeyVehicleType].toInt());
    }

    // Update firmware/vehicle offline settings if we aren't connect to a vehicle
    if (_masterController->offline()) {
        appSettings->offlineEditingFirmwareClass()->setRawValue(QGCMAVLink::firmwareClass(static_cast<MAV_AUTOPILOT>(json[_jsonKeyFirmwareType].toInt())));
        if (json.contains(_jsonKeyVehicleType)) {
            appSettings->offlineEditingVehicleClass()->setRawValue(QGCMAVLink::vehicleClass(planFileVehicleType));
        }
    }

    // The controller vehicle always tracks the Plan file firmware/vehicle types so update it
    _controllerVehicle->stopTrackingFirmwareVehicleTypeChanges();
    _controllerVehicle->_offlineFirmwareTypeSettingChanged(planFileFirmwareType);
    _controllerVehicle->_offlineVehicleTypeSettingChanged(planFileVehicleType);

    if (json.contains(_jsonKeyCruiseSpeed)) {
        appSettings->offlineEditingCruiseSpeed()->setRawValue(json[_jsonKeyCruiseSpeed].toDouble());
    }
    if (json.contains(_jsonKeyHoverSpeed)) {
        appSettings->offlineEditingHoverSpeed()->setRawValue(json[_jsonKeyHoverSpeed].toDouble());
    }
    if (json.contains(_jsonKeyGlobalPlanAltitudeMode)) {
        setGlobalAltitudeMode(json[_jsonKeyGlobalPlanAltitudeMode].toVariant().value<QGroundControlQmlGlobal::AltitudeMode>());
    }

    QGeoCoordinate homeCoordinate;
    if (!JsonHelper::loadGeoCoordinate(json[_jsonKeyPlannedHomePosition], true /* altitudeRequired */, homeCoordinate, errorString)) {
        return false;
    }
    QJsonArray jsonBanks = json[_jsonKeyBankInfo].toArray();
    for (int i = 0; i < jsonBanks.count(); i++) {
        QJsonObject jsonBank = jsonBanks[i].toObject();
        SingleBankInfo* bankInfo = new SingleBankInfo;
        QMap<uint16_t, WaypointInfoSlot*> mapInfoSlotsInBank;
        if (!loadBankInfo(jsonBank, bankInfo, mapInfoSlotsInBank, errorString)) {
            if(bankInfo) {
                delete bankInfo;
                bankInfo = nullptr;
            }
            return false;
        } else {
            bankInfos[bankInfo->idBank] = bankInfo;
            infoSlots[bankInfo->idBank] = mapInfoSlotsInBank;
        }
    }
    return true;
}

bool MissionController::_loadItemsFromJson(const QJsonObject& json, QMap<uint16_t, SingleBankInfo*> bankInfos, QMap<uint16_t, QMap<uint16_t, WaypointInfoSlot*>> &infoSlots, QString& errorString)
{
    // V1 file format has no file type key and version key is string. Convert to new format.
    if (!json.contains(JsonHelper::jsonFileTypeKey)) {
        json[JsonHelper::jsonFileTypeKey] = _jsonKeyFileType;
    }

    int fileVersion;
    JsonHelper::validateExternalQGCJsonFile(json,
                                            _jsonKeyFileType,    // expected file type
                                            1,                     // minimum supported version
                                            2,                     // maximum supported version
                                            fileVersion,
                                            errorString);
    return _loadJsonMissionFile(json, bankInfos, infoSlots, errorString);
}

void MissionController::_initLoadedVisualItems(QmlObjectListModel* loadedVisualItems)
{
    qDebug(MissionControllerLog) << "_initLoadedVisualItems" << loadedVisualItems << _itemsBank;
    if (_itemsBank) {
        _deinitAllVisualItems();
        _itemsBank->deleteLater();
    }
    _itemsBank = loadedVisualItems;
    _initAllVisualItems();

    if (_itemsBank->count() > 0) {
        _firstItemAdded();
    } else {
        _allItemsRemoved();
    }
}

bool MissionController::load(const QJsonObject& json, QString& errorString)
{
    qCDebug(MissionControllerLog) << "load()";
    QString errorStr;
    QString errorMessage = tr("Mission: %1");

    QMap<uint16_t, SingleBankInfo*> bankInfos;
    QMap<uint16_t, QMap<uint16_t, WaypointInfoSlot*>> infoSlots;
    if (!_loadJsonMissionFile(json, bankInfos, infoSlots, errorStr)) {
        errorString = errorMessage.arg(errorStr);
        return false;
    }
    _itemsBank = initLoadedInfoSlotsInBanks(bankInfos, infoSlots);
    _initAllVisualItems();
    _updateContainsItems();
    emit itemsBankChanged();
    emit newItemsFromVehicle();
    return true;
}

bool MissionController::loadJsonFile(QFile& file, QString& errorString)
{
    qCDebug(MissionControllerLog) << "loadJsonFile" << file.fileName();
    QString         errorStr;
    QString         errorMessage = tr("Mission: %1");
    QJsonDocument   jsonDoc;
    QByteArray      bytes = file.readAll();

    if (!JsonHelper::isJsonFile(bytes, jsonDoc, errorStr)) {
        errorString = errorMessage.arg(errorStr);
        return false;
    }

    QJsonObject json = jsonDoc.object();
    QMap<uint16_t, SingleBankInfo*> mapBankInfos;
    QMap<uint16_t, QMap<uint16_t, WaypointInfoSlot*>> mapInfoSlotsInBanks;
    if (!_loadItemsFromJson(json, mapBankInfos, mapInfoSlotsInBanks, errorStr)) {
        errorString = errorMessage.arg(errorStr);
        return false;
    }
    _itemsBank = initLoadedInfoSlotsInBanks(mapBankInfos, mapInfoSlotsInBanks);
    _initAllVisualItems();
    _updateContainsItems();
    emit itemsBankChanged();
    emit newItemsFromVehicle();
    return true;
}

void MissionController::save(QJsonObject& json)
{
    json[JsonHelper::jsonVersionKey] = _missionFileVersion;

    // Mission settings
    QJsonValue coordinateValue;

    JsonHelper::saveGeoCoordinate(QGeoCoordinate(), true /* writeAltitude */, coordinateValue);
    json[_jsonKeyPlannedHomePosition]    = coordinateValue;
    json[_jsonKeyFirmwareType]           = _controllerVehicle->firmwareType();
    json[_jsonKeyVehicleType]            = _controllerVehicle->vehicleType();
    json[_jsonKeyCruiseSpeed]            = _controllerVehicle->defaultCruiseSpeed();
    json[_jsonKeyHoverSpeed]             = _controllerVehicle->defaultHoverSpeed();
    json[_jsonKeyGlobalPlanAltitudeMode] = _globalAltMode;

    // Save the visual items

    QJsonArray rgJsonBankInfos;
    for (int i=0; i< _itemsBank->count(); i++) {
        QJsonArray rgJsonInfoSlots;
        ItemBank* itemBank = qobject_cast<ItemBank*>(_itemsBank->get(i));
        QmlObjectListModel* itemsWaypoint = itemBank->itemsWaypoint();
        for (int j = 0; j < itemsWaypoint->count(); j++) {
            ItemWaypoint* itemWaypoint = itemsWaypoint->value<ItemWaypoint*>(j);//qobject_cast<ItemWaypoint*>(itemsWaypoint->get(i));
            QmlObjectListModel* itemsInfoSlot = itemWaypoint->itemsInfoSlot();
            for (int k = 0; k < itemsInfoSlot->count(); k++) {
                QJsonObject objectInfoSlot;
                ItemInfoSlot* itemInfoSlot = itemsInfoSlot->value<ItemInfoSlot*>(k);
                objectInfoSlot[_jsonKeyIdBank]         = itemInfoSlot->infoSlot().idBank;
                objectInfoSlot[_jsonKeyIdWp]           = itemInfoSlot->infoSlot().idWp;
                objectInfoSlot[_jsonKeyIdInfoSlot]     = itemInfoSlot->infoSlot().idInfoSlot;
                objectInfoSlot[_jsonKeyTypeInfoSlot]   = itemInfoSlot->infoSlot().typeInfoSlot;
                objectInfoSlot[_jsonKeyLon]            = itemInfoSlot->infoSlot().lon;
                objectInfoSlot[_jsonKeyLat]            = itemInfoSlot->infoSlot().lat;
                objectInfoSlot[_jsonKeyAlt]            = itemInfoSlot->infoSlot().alt;
                objectInfoSlot[_jsonKeyRadiusCh]       = itemInfoSlot->infoSlot().radiusCh;
                objectInfoSlot[_jsonKeyRVGnd]          = itemInfoSlot->infoSlot().rVGnd;
                objectInfoSlot[_jsonKeyRRocDoc]        = itemInfoSlot->infoSlot().rRocDoc;
                objectInfoSlot[_jsonKeySwTd]           = itemInfoSlot->infoSlot().swTd;
                objectInfoSlot[_jsonKeySwVGnd10x]      = itemInfoSlot->infoSlot().swVGnd10x;
                objectInfoSlot[_jsonKeyRHdgDeg100x]    = itemInfoSlot->infoSlot().rHdgDeg100x;
                objectInfoSlot[_jsonKeyActDept]        = itemInfoSlot->infoSlot().actDept;
                objectInfoSlot[_jsonKeyActArrvl]       = itemInfoSlot->infoSlot().actArrvl;
                objectInfoSlot[_jsonKeySwCondSet]      = itemInfoSlot->infoSlot().swCondSet;
                objectInfoSlot[_jsonKeyActInFlt]       = itemInfoSlot->infoSlot().actInflt;
                objectInfoSlot[_jsonKeyHgtHdgCtrlMode] = itemInfoSlot->infoSlot().hgtHdgCtrlMode;
                objectInfoSlot[_jsonKeySwAngDeg]       = itemInfoSlot->infoSlot().swAngDeg;
                objectInfoSlot[_jsonKeySwTurns]        = itemInfoSlot->infoSlot().swTurns;
                objectInfoSlot[_jsonKeyGdMode]         = itemInfoSlot->infoSlot().gdMode;
                objectInfoSlot[_jsonKeyMnvrSty]        = itemInfoSlot->infoSlot().mnvrSty;
                objectInfoSlot[_jsonKeyTransSty]       = itemInfoSlot->infoSlot().transSty;
                objectInfoSlot[_jsonKeyReserved0]      = itemInfoSlot->infoSlot().reserved0;
                objectInfoSlot[_jsonKeyReserved1]      = itemInfoSlot->infoSlot().reserved1;
                rgJsonInfoSlots.append(objectInfoSlot);
                qCDebug(MissionControllerLog) << "i:" << i << "j:" << j << "k:" << k
                                              << "idBank:" << itemInfoSlot->infoSlot().idBank
                                              << "idWp:" << itemInfoSlot->infoSlot().idWp
                                              << "idInfoSlot:" << itemInfoSlot->infoSlot().idInfoSlot;
            }
        }
        QJsonObject jsonItemBank;
        jsonItemBank[_jsonKeyIdBank] = itemBank->bankInfo().idBank;
        jsonItemBank[_jsonKeyTypeBank] = itemBank->bankInfo().typeBank;
        jsonItemBank[_jsonKeyNInfoSlotMax] = itemBank->bankInfo().nInfoSlotMax;
        jsonItemBank[_jsonKeyIWp] = itemBank->bankInfo().iWp;
        jsonItemBank[_jsonKeyNWp] = itemBank->bankInfo().nWp;
        jsonItemBank[_jsonKeyNInfoSlot] = itemBank->bankInfo().nInfoSlot;
        jsonItemBank[_jsonKeyIdBankSuc] = itemBank->bankInfo().idBankSuc;
        jsonItemBank[_jsonKeyIdBankIWpSuc] = itemBank->bankInfo().idBankIwpSuc;
        jsonItemBank[_jsonKeyActBankEnd] = itemBank->bankInfo().actBankEnd;
        jsonItemBank[_jsonKeyStateBank] = itemBank->bankInfo().stateBank;
        jsonItemBank[_jsonKeySwitchState] = itemBank->bankInfo().switchState;
        jsonItemBank[_jsonKeyInfoSlots] = rgJsonInfoSlots;
        rgJsonBankInfos.append(jsonItemBank);
    }
    json[_jsonKeyBankInfo] = rgJsonBankInfos;
}

FlightPathSegment* MissionController::_createFlightPathSegmentWorker(VisualItemPair& pair)
{
    // The takeoff goes straight up from ground to alt and then over to specified position at same alt. Which means
    // that coord 1 altitude is the same as coord altitude.
    bool                takeoffStraightUp   = pair.second->isTakeoffItem() && !_controllerVehicle->fixedWing();

    QGeoCoordinate      coord1              = pair.first->exitCoordinate();
    QGeoCoordinate      coord2              = pair.second->coordinate();
    double              coord2AMSLAlt       = pair.second->amslEntryAlt();
    double              coord1AMSLAlt       = takeoffStraightUp ? coord2AMSLAlt : pair.first->amslExitAlt();

    FlightPathSegment::SegmentType segmentType = FlightPathSegment::SegmentTypeGeneric;
    if (pair.second->isTakeoffItem()) {
        segmentType = FlightPathSegment::SegmentTypeTakeoff;
    } else if (pair.second->isLandCommand()) {
        segmentType = FlightPathSegment::SegmentTypeLand;
    }

    FlightPathSegment* segment = new FlightPathSegment(segmentType, coord1, coord1AMSLAlt, coord2, coord2AMSLAlt, !_flyView /* queryTerrainData */,  this);

    if (takeoffStraightUp) {
//        connect(pair.second, &VisualMissionItem::amslEntryAltChanged, segment, &FlightPathSegment::setCoord1AMSLAlt);
    } else {
//        connect(pair.first, &VisualMissionItem::amslExitAltChanged, segment, &FlightPathSegment::setCoord1AMSLAlt);
    }
//    connect(pair.first,  &VisualMissionItem::exitCoordinateChanged, segment,    &FlightPathSegment::setCoordinate1);
//    connect(pair.second, &VisualMissionItem::coordinateChanged,     segment,    &FlightPathSegment::setCoordinate2);
//    connect(pair.second, &VisualMissionItem::amslEntryAltChanged,   segment,    &FlightPathSegment::setCoord2AMSLAlt);

//    connect(pair.second, &VisualMissionItem::coordinateChanged,         this,       &MissionController::_recalcMissionFlightStatusSignal, Qt::QueuedConnection);

    connect(segment,    &FlightPathSegment::totalDistanceChanged,       this,       &MissionController::recalcTerrainProfile,             Qt::QueuedConnection);
    connect(segment,    &FlightPathSegment::coord1AMSLAltChanged,       this,       &MissionController::_recalcMissionFlightStatusSignal, Qt::QueuedConnection);
    connect(segment,    &FlightPathSegment::coord2AMSLAltChanged,       this,       &MissionController::_recalcMissionFlightStatusSignal, Qt::QueuedConnection);
    connect(segment,    &FlightPathSegment::amslTerrainHeightsChanged,  this,       &MissionController::recalcTerrainProfile,             Qt::QueuedConnection);
    connect(segment,    &FlightPathSegment::terrainCollisionChanged,    this,       &MissionController::recalcTerrainProfile,             Qt::QueuedConnection);

    return segment;
}

FlightPathSegment* MissionController::_addFlightPathSegment(FlightPathSegmentHashTable& prevItemPairHashTable, VisualItemPair& pair)
{
    FlightPathSegment* segment = nullptr;

    if (prevItemPairHashTable.contains(pair)) {
        // Pair already exists and connected, just re-use
        _flightPathSegmentHashTable[pair] = segment = prevItemPairHashTable.take(pair);
    } else {
        segment = _createFlightPathSegmentWorker(pair);
        _flightPathSegmentHashTable[pair] = segment;
    }

    _simpleFlightPathSegments.append(segment);

    return segment;
}

void MissionController::_updateBatteryInfo(int waypointIndex)
{
    if (_missionFlightStatus.mAhBattery != 0) {
        _missionFlightStatus.hoverAmpsTotal = (_missionFlightStatus.hoverTime / 60.0) * _missionFlightStatus.hoverAmps;
        _missionFlightStatus.cruiseAmpsTotal = (_missionFlightStatus.cruiseTime / 60.0) * _missionFlightStatus.cruiseAmps;
        _missionFlightStatus.batteriesRequired = ceil((_missionFlightStatus.hoverAmpsTotal + _missionFlightStatus.cruiseAmpsTotal) / _missionFlightStatus.ampMinutesAvailable);
        // FIXME: Battery change point code pretty much doesn't work. The reason is that is treats complex items as a black box. It needs to be able to look
        // inside complex items in order to determine a swap point that is interior to a complex item. Current the swap point display in PlanToolbar is
        // disabled to do this problem.
        if (waypointIndex != -1 && _missionFlightStatus.batteriesRequired == 2 && _missionFlightStatus.batteryChangePoint == -1) {
            _missionFlightStatus.batteryChangePoint = waypointIndex - 1;
        }
    }
}

void MissionController::_addHoverTime(double hoverTime, double hoverDistance, int waypointIndex)
{
    _missionFlightStatus.totalTime += hoverTime;
    _missionFlightStatus.hoverTime += hoverTime;
    _missionFlightStatus.hoverDistance += hoverDistance;
    _missionFlightStatus.totalDistance += hoverDistance;
    _updateBatteryInfo(waypointIndex);
}

void MissionController::_addCruiseTime(double cruiseTime, double cruiseDistance, int waypointIndex)
{
    _missionFlightStatus.totalTime += cruiseTime;
    _missionFlightStatus.cruiseTime += cruiseTime;
    _missionFlightStatus.cruiseDistance += cruiseDistance;
    _missionFlightStatus.totalDistance += cruiseDistance;
    _updateBatteryInfo(waypointIndex);
}

/// Adds the specified time to the appropriate hover or cruise time values.
///     @param vtolInHover true: vtol is currrent in hover mode
///     @param hoverTime    Amount of time tp add to hover
///     @param cruiseTime   Amount of time to add to cruise
///     @param extraTime    Amount of additional time to add to hover/cruise
///     @param seqNum       Sequence number of waypoint for these values, -1 for no waypoint associated
void MissionController::_addTimeDistance(bool vtolInHover, double hoverTime, double cruiseTime, double extraTime, double distance, int seqNum)
{
    if (_controllerVehicle->vtol()) {
        if (vtolInHover) {
            _addHoverTime(hoverTime, distance, seqNum);
            _addHoverTime(extraTime, 0, -1);
        } else {
            _addCruiseTime(cruiseTime, distance, seqNum);
            _addCruiseTime(extraTime, 0, -1);
        }
    } else {
        if (_controllerVehicle->multiRotor()) {
            _addHoverTime(hoverTime, distance, seqNum);
            _addHoverTime(extraTime, 0, -1);
        } else {
            _addCruiseTime(cruiseTime, distance, seqNum);
            _addCruiseTime(extraTime, 0, -1);
        }
    }
}

/// Initializes a new set of mission items
void MissionController::_initAllVisualItems(void)
{
    qCDebug(MissionControllerLog) << "_initAllVisualItems()";
    if (!_itemsBank)
        return;
    for (int i=0; i<_itemsBank->count(); i++) {
//        ItemBank* item = qobject_cast<ItemBank*>(_itemsBank->get(i));
//        _initVisualItem(item);
    }

    connect(_itemsBank, &QmlObjectListModel::dirtyChanged, this, &MissionController::_itemsBankDirtyChanged);
    connect(_itemsBank, &QmlObjectListModel::countChanged, this, &MissionController::_updateContainsItems);

    emit visualItemsChanged();
    emit containsItemsChanged(containsItems());
    emit plannedHomePositionChanged(plannedHomePosition());

    if (!_flyView) {
    }

    setDirty(false);
}

void MissionController::_deinitAllVisualItems(void)
{
    if (!_itemsBank)
        return;
    disconnect(_itemsBank, &QmlObjectListModel::dirtyChanged, this, &MissionController::dirtyChanged);
    disconnect(_itemsBank, &QmlObjectListModel::countChanged, this, &MissionController::_updateContainsItems);
    qDebug() << "_deinitAllVisualItems" << _itemsBank << _itemsBank->objectName() << _itemsBank->count();
    for (int i = 0; i < _itemsBank->count(); i++) {
        ItemBank* itemBank = _itemsBank->value<ItemBank*>(i);
        disconnect(itemBank->itemsWaypoint(), nullptr, nullptr, nullptr);
        for (int j = 0; j < itemBank->itemsWaypoint()->count(); j++) {
            ItemWaypoint* itemWaypoint = itemBank->itemsWaypoint()->value<ItemWaypoint*>(j);
            disconnect(itemWaypoint->itemsInfoSlot(), nullptr, nullptr, nullptr);
            for (int k = 0; k < itemWaypoint->itemsInfoSlot()->count(); k++) {
                ItemInfoSlot* itemInfoSlot = itemWaypoint->itemsInfoSlot()->value<ItemInfoSlot*>(k);
                disconnect(itemInfoSlot, nullptr, nullptr, nullptr);
            }
            disconnect(itemWaypoint, nullptr, nullptr, nullptr);
        }
        disconnect(itemBank, nullptr, nullptr, nullptr);
    }
}

void MissionController::_itemCommandChanged(void)
{
    emit _recalcFlightPathSegmentsSignal();
}

void MissionController::_managerVehicleChanged(Vehicle* managerVehicle)
{
    if (_managerVehicle) {
        _missionManager->disconnect(this);
        _managerVehicle->disconnect(this);
        _managerVehicle = nullptr;
        _missionManager = nullptr;
    }

    _managerVehicle = managerVehicle;
    if (!_managerVehicle) {
        qWarning() << "MissionController::managerVehicleChanged managerVehicle=NULL";
        return;
    }

    _missionManager = _managerVehicle->missionManager();
    connect(_missionManager, &MissionManager::newMissionItemsAvailable, this, &MissionController::_newMissionItemsAvailableFromVehicle);
    connect(_missionManager, &MissionManager::sendComplete,             this, &MissionController::_managerSendComplete);
    connect(_missionManager, &MissionManager::removeAllComplete,        this, &MissionController::_managerRemoveAllComplete);
    connect(_missionManager, &MissionManager::inProgressChanged,        this, &MissionController::_inProgressChanged);
    connect(_missionManager, &MissionManager::progressPct,              this, &MissionController::_progressPctChanged);
    connect(_missionManager, &MissionManager::currentIndexChanged,      this, &MissionController::_currentMissionIndexChanged);
    connect(_missionManager, &MissionManager::lastCurrentIndexChanged,  this, &MissionController::resumeMissionIndexChanged);
    connect(_missionManager, &MissionManager::resumeMissionReady,       this, &MissionController::resumeMissionReady);
    connect(_missionManager, &MissionManager::resumeMissionUploadFail,  this, &MissionController::resumeMissionUploadFail);
    connect(_missionManager, &MissionManager::totalBankInfoAvailbale, this, &MissionController::_totalBankInfoChangedFromMissionManager, Qt::QueuedConnection);
    connect(_managerVehicle, &Vehicle::defaultCruiseSpeedChanged,       this, &MissionController::_recalcMissionFlightStatusSignal, Qt::QueuedConnection);
    connect(_managerVehicle, &Vehicle::defaultHoverSpeedChanged,        this, &MissionController::_recalcMissionFlightStatusSignal, Qt::QueuedConnection);
    connect(_managerVehicle, &Vehicle::vehicleTypeChanged,              this, &MissionController::complexMissionItemNamesChanged);

    emit complexMissionItemNamesChanged();
    emit resumeMissionIndexChanged();
}

void MissionController::_inProgressChanged(bool inProgress)
{
    emit syncInProgressChanged(inProgress);
}

double MissionController::_normalizeLat(double lat)
{
    // Normalize latitude to range: 0 to 180, S to N
    return lat + 90.0;
}

double MissionController::_normalizeLon(double lon)
{
    // Normalize longitude to range: 0 to 360, W to E
    return lon  + 180.0;
}

void MissionController::_centerHomePositionOnMissionItems(QmlObjectListModel *visualItems)
{
    qCDebug(MissionControllerLog) << "_centerHomePositionOnMissionItems";

    if (visualItems->count() > 1) {
//        double north = 0.0;
//        double south = 0.0;
//        double east  = 0.0;
//        double west  = 0.0;
//        bool firstCoordSet = false;

//        for (int i=1; i<visualItems->count(); i++) {
//            VisualMissionItem* item = qobject_cast<VisualMissionItem*>(visualItems->get(i));
//            if (item->specifiesCoordinate()) {
//                if (firstCoordSet) {
//                    double lat = _normalizeLat(item->coordinate().latitude());
//                    double lon = _normalizeLon(item->coordinate().longitude());
//                    north = fmax(north, lat);
//                    south = fmin(south, lat);
//                    east  = fmax(east, lon);
//                    west  = fmin(west, lon);
//                } else {
//                    firstCoordSet = true;
//                    north = _normalizeLat(item->coordinate().latitude());
//                    south = north;
//                    east  = _normalizeLon(item->coordinate().longitude());
//                    west  = east;
//                }
//            }
//        }

//        if (firstCoordSet) {
//            _settingsItem->setInitialHomePositionFromUser(QGeoCoordinate((south + ((north - south) / 2)) - 90.0, (west + ((east - west) / 2)) - 180.0, 0.0));
//        }
    }
}

int MissionController::currentMissionIndex(void) const
{
    if (!_flyView) {
        return -1;
    } else {
        int currentIndex = _missionManager->currentIndex();
        if (!_controllerVehicle->firmwarePlugin()->sendHomePositionToVehicle()) {
            currentIndex++;
        }
        return currentIndex;
    }
}

void MissionController::_currentMissionIndexChanged(int sequenceNumber)
{
    if (_flyView) {
        if (!_controllerVehicle->firmwarePlugin()->sendHomePositionToVehicle()) {
            sequenceNumber++;
        }
        emit currentMissionIndexChanged(currentMissionIndex());
    }
}

bool MissionController::syncInProgress(void) const
{
    return _missionManager->inProgress();
}

bool MissionController::dirty(void) const
{
    return _itemsBank ? _itemsBank->dirty() : false;
}


void MissionController::setDirty(bool dirty)
{
#ifndef FINDCOUNT
    qDebug() << "**************** MissionController::setDirty" << dirty;
    if (_itemsBank) {
        _itemsBank->setDirty(dirty);
    }
#endif
}

void MissionController::_updateContainsItems(void)
{
    emit containsItemsChanged(containsItems());
}

bool MissionController::containsItems(void) const
{
    return _itemsBank ? _itemsBank->count() > 0 : false;
}

void MissionController::removeAllFromVehicle(void)
{
    if (_masterController->offline()) {
        qCWarning(MissionControllerLog) << "MissionControllerLog::removeAllFromVehicle called while offline";
    } else if (syncInProgress()) {
        qCWarning(MissionControllerLog) << "MissionControllerLog::removeAllFromVehicle called while syncInProgress";
    } else {
        _itemsRequested = true;
        _missionManager->removeAll();
    }
}

QStringList MissionController::complexMissionItemNames(void) const
{
    QStringList complexItems;

//    complexItems.append(SurveyComplexItem::name);
//    complexItems.append(CorridorScanComplexItem::name);
//    if (_controllerVehicle->multiRotor() || _controllerVehicle->vtol()) {
//        complexItems.append(StructureScanComplexItem::name);
//    }

    // Note: The landing pattern items are not added here since they have there own button which adds them

    return qgcApp()->toolbox()->corePlugin()->complexMissionItemNames(_controllerVehicle, complexItems);
}

void MissionController::resumeMission(int resumeIndex)
{
    if (!_controllerVehicle->firmwarePlugin()->sendHomePositionToVehicle()) {
        resumeIndex--;
    }
//    _missionManager->generateResumeMission(resumeIndex);
}

QGeoCoordinate MissionController::plannedHomePosition(void) const
{
//    if (_settingsItem) {
//        return _settingsItem->coordinate();
//    } else {
        return QGeoCoordinate();
//    }
}

void MissionController::applyDefaultMissionAltitude(void)
{
    double defaultAltitude = _appSettings->defaultMissionItemAltitude()->rawValue().toDouble();
    Q_UNUSED(defaultAltitude);
}

void MissionController::_progressPctChanged(double progressPct)
{
    if (!QGC::fuzzyCompare(progressPct, _progressPct)) {
        _progressPct = progressPct;
        emit progressPctChanged(progressPct);
    }
}

void MissionController::_itemsBankDirtyChanged(bool dirty)
{
    qDebug() << "MissionController::_itemsBankDirtyChanged" << dirty;
    emit dirtyChanged(dirty);
}

bool MissionController::showPlanFromManagerVehicle(void)
{
    qCDebug(MissionControllerLog) << "showPlanFromManagerVehicle _flyView" << _flyView << "MissionController:" << this << "parent:" << parent();
    if (_masterController->offline()) {
        qCWarning(MissionControllerLog) << "MissionController::showPlanFromManagerVehicle called while offline";
        return true;    // stops further propagation of showPlanFromManagerVehicle due to error
    } else {
        if (!_managerVehicle->initialPlanRequestComplete()) {
            // The vehicle hasn't completed initial load, we can just wait for newMissionItemsAvailable to be signalled automatically
            qCDebug(MissionControllerLog) << "showPlanFromManagerVehicle: !initialPlanRequestComplete, wait for signal";
            return true;
        } else if (syncInProgress()) {
            // If the sync is already in progress, newMissionItemsAvailable will be signalled automatically when it is done. So no need to do anything.
            qCDebug(MissionControllerLog) << "showPlanFromManagerVehicle: syncInProgress wait for signal";
            return true;
        } else {
            // Fake a _newMissionItemsAvailable with the current items
            qCDebug(MissionControllerLog) << "showPlanFromManagerVehicle: sync complete simulate signal";
            _itemsRequested = true;
            _newMissionItemsAvailableFromVehicle(false /* removeAllRequested */);
            return false;
        }
    }
}

void MissionController::_managerSendComplete(bool error)
{
    // Fly view always reloads on send complete
    qCDebug(MissionControllerLog) << "_managerSendComplete error:" << error;
    if (!error && _flyView) {
        showPlanFromManagerVehicle();
    }
}

void MissionController::_managerRemoveAllComplete(bool error)
{
    qCDebug(MissionControllerLog) << "_managerRemoveAllComplete error:" << error;
    if (!error) {
        // Remove all from vehicle so we always update
        showPlanFromManagerVehicle();
    }
}

bool MissionController::_isROIBeginItem(SimpleMissionItem* simpleItem)
{
    return simpleItem->mavCommand() == MAV_CMD_DO_SET_ROI_LOCATION ||
            simpleItem->mavCommand() == MAV_CMD_DO_SET_ROI_WPNEXT_OFFSET ||
            (simpleItem->mavCommand() == MAV_CMD_DO_SET_ROI &&
             static_cast<int>(simpleItem->missionItem().param1()) == MAV_ROI_LOCATION);
}

bool MissionController::_isROICancelItem(SimpleMissionItem* simpleItem)
{
    return simpleItem->mavCommand() == MAV_CMD_DO_SET_ROI_NONE ||
            (simpleItem->mavCommand() == MAV_CMD_DO_SET_ROI &&
             static_cast<int>(simpleItem->missionItem().param1()) == MAV_ROI_NONE);
}

void MissionController::_updateTimeout()
{
    QGeoCoordinate firstCoordinate;
    QGeoCoordinate takeoffCoordinate;
    QGCGeoBoundingCube boundingCube;
    double north = 0.0;
    double south = 180.0;
    double east  = 0.0;
    double west  = 360.0;
    double minAlt = QGCGeoBoundingCube::MaxAlt;
    double maxAlt = QGCGeoBoundingCube::MinAlt;
    for (int i = 1; i < _itemsBank->count(); i++) {
        ItemBank* itemBank = _itemsBank->value<ItemBank*>(i);
        for (int j = 0; j < itemBank->itemsWaypoint()->count(); j++) {
            ItemWaypoint* itemWaypoint = itemBank->itemsWaypoint()->value<ItemWaypoint*>(j);
            if(itemWaypoint->coordinate().isValid()) {
                double lat = itemWaypoint->coordinate().latitude()  + 90.0;
                double lon = itemWaypoint->coordinate().longitude() + 180.0;
                double alt = itemWaypoint->coordinate().altitude();
                north  = fmax(north, lat);
                south  = fmin(south, lat);
                east   = fmax(east,  lon);
                west   = fmin(west,  lon);
                minAlt = fmin(minAlt, alt);
                maxAlt = fmax(maxAlt, alt);
            }
        }
    }

    //-- Build bounding "cube"
    boundingCube = QGCGeoBoundingCube(
                QGeoCoordinate(north - 90.0, west - 180.0, minAlt),
                QGeoCoordinate(south - 90.0, east - 180.0, maxAlt));
    if(_travelBoundingCube != boundingCube || _takeoffCoordinate != takeoffCoordinate) {
        _takeoffCoordinate  = takeoffCoordinate;
        _travelBoundingCube = boundingCube;
        emit missionBoundingCubeChanged();
        qCDebug(MissionControllerLog) << "Bounding cube:" << _travelBoundingCube.pointNW << _travelBoundingCube.pointSE;
    }
}

void MissionController::_complexBoundingBoxChanged()
{
    _updateTimer.start(UPDATE_TIMEOUT);
}

bool MissionController::isEmpty(void) const
{
    return _itemsBank->count() < 1;
}

void MissionController::_takeoffItemNotRequiredChanged(void)
{
    // Force a recalc of allowed bits
}

void MissionController::_totalBankInfoChangedFromMissionManager(const TotalBankInfo& totalBankInfo)
{
    _totalBankInfo = totalBankInfo;
    emit largeBankInfoSlotCapacityChanged();
    emit smallBankInfoSlotCapacityChanged();
    emit largeBankNumberChanged();
    emit smallBankNumberChanged();
    emit idTransientBankChanged();
}

QString MissionController::surveyComplexItemName(void) const
{
    return ""; //SurveyComplexItem::name;
}

void MissionController::_allItemsRemoved(void)
{
    // When there are no mission items we track changes to firmware/vehicle type. This allows a vehicle connection
    // to adjust these items.
    _controllerVehicle->trackFirmwareVehicleTypeChanges();
}

void MissionController::_firstItemAdded(void)
{
    // As soon as the first item is added we lock the firmware/vehicle type to current values. So if you then connect a vehicle
    // it will not affect these values.
    _controllerVehicle->stopTrackingFirmwareVehicleTypeChanges();
}

MissionController::SendToVehiclePreCheckState MissionController::sendToVehiclePreCheck(void)
{
    if (_managerVehicle->isOfflineEditingVehicle()) {
        return SendToVehiclePreCheckStateNoActiveVehicle;
    }
    if (_managerVehicle->armed() && _managerVehicle->flightMode() == _managerVehicle->missionFlightMode()) {
        return SendToVehiclePreCheckStateActiveMission;
    }
    if (_controllerVehicle->firmwareType() != _managerVehicle->firmwareType() || QGCMAVLink::vehicleClass(_controllerVehicle->vehicleType()) != QGCMAVLink::vehicleClass(_managerVehicle->vehicleType())) {
        return SendToVehiclePreCheckStateFirwmareVehicleMismatch;
    }
    return SendToVehiclePreCheckStateOk;
}

QGroundControlQmlGlobal::AltitudeMode MissionController::globalAltitudeMode(void)
{
    return _globalAltMode;
}

QGroundControlQmlGlobal::AltitudeMode MissionController::globalAltitudeModeDefault(void)
{
    if (_globalAltMode == QGroundControlQmlGlobal::AltitudeModeNone) {
        return QGroundControlQmlGlobal::AltitudeModeRelative;
    } else {
        return _globalAltMode;
    }
}

void MissionController::setGlobalAltitudeMode(QGroundControlQmlGlobal::AltitudeMode altMode)
{
    if (_globalAltMode != altMode) {
        _globalAltMode = altMode;
        emit globalAltitudeModeChanged();
    }
}
