#include <JsonHelper.h>
#include "ItemBank.h"
#include "ItemWaypoint.h"

const char* _jsonKeyIdBank          = "idBank";
const char* _jsonKeyTypeBank        = "typeBank";
const char* _jsonKeyNInfoSlotMax    = "nInfoSlotMax";
const char* _jsonKeyIWp             = "iWp";
const char* _jsonKeyNWp             = "nWp";
const char* _jsonKeyNInfoSlot       = "nInfoSlot";
const char* _jsonKeyIdBankSuc       = "idBankSuc";
const char* _jsonKeyIdBankIWpSuc    = "idBankIWpSuc";
const char* _jsonKeyActBankEnd      = "actBankEnd";
const char* _jsonKeyStateBank       = "stateBank";
const char* _jsonKeySwitchState     = "switchState";
const char* _jsonKeyInfoSlots       = "infoSlots";

QGC_LOGGING_CATEGORY(ItemBankLog, "ItemBankLog")

ItemBank::ItemBank(const SingleBankInfo& bankInfo, QObject *parent)
    : QObject(parent), _bankInfo(bankInfo)
{
    qCDebug(ItemBankLog) << "ItemBank() idBank:" << _bankInfo.idBank << "nWp:" << _bankInfo.nWp << "nInfoSlot:" << _bankInfo.nInfoSlot << "itemBank:" << this;
    _itemsWaypoint = new QmlObjectListModel(this);
    connect(_itemsWaypoint, &QmlObjectListModel::dirtyChanged, this, [&](bool dirty) {
        setDirty(dirty);
        qCDebug(ItemBankLog) << "_itemsWaypoint::dirtyChanged" << dirty;
    });
    emit nWpChanged();
    emit nInfoSlotChanged();
}

ItemBank::~ItemBank()
{
//    disconnect(this, nullptr, nullptr, nullptr);
//    qCDebug(ItemBankLog) << "~ItemBank()" << _bankInfo.idBank << _bankInfo.nWp << _bankInfo.nInfoSlot << "itemBank:" << this << "parent:" << parent();
}

void ItemBank::setDirty(bool dirty)
{
    if (dirty != _dirty) {
        if (_itemsWaypoint && !dirty) {
            _itemsWaypoint->setDirty(dirty);
        }
        _dirty = dirty;
        emit dirtyChanged(dirty);
        qCDebug(ItemBankLog) << "ItemBank::setDirty" << idBank() << dirty;
    }
}

void ItemBank::setCurrentWaypoint(int index)
{
    if (_itemsWaypoint->count() == 0 || _itemsWaypoint->count() <= index/* || _indexCurrentWaypoint == index*/)
        return;
    _indexCurrentWaypoint = index;
    emit indexCurrentWaypointChanged();
    setItemCurrentWaypoint(index);
}

void ItemBank::setIdBank(int index)
{
    if (_bankInfo.idBank == index)
        return;
    _bankInfo.idBank = index;
    emit idBankChanged();
}

void ItemBank::setInfoSlots(QMap<uint16_t, WaypointInfoSlot*> infoSlots)
{
    // 清空bankInfo中的nInfoSlot，重新计算。因为infoSlot分布在不同的航点中，在添加或删除infoslot时会自动增加或减小个数。
    _bankInfo.nInfoSlot = 0;
    foreach(uint16_t idInfoSlot, infoSlots.keys()) {
        WaypointInfoSlot* infoSlot = infoSlots[idInfoSlot];
        switch (infoSlots[idInfoSlot]->typeInfoSlot & 0x0f) {
        case TypeInfoSlot::MAJOR:
            insertWaypointFromInfoSlot(infoSlot);
            break;
        case TypeInfoSlot::MINOR:
            if (_itemsWaypoint->count() > 0) {
                ItemWaypoint* itemWaypoint = _itemsWaypoint->value<ItemWaypoint*>(_itemsWaypoint->count() - 1);
                ItemInfoSlot* itemInfoSlot = new ItemInfoSlot(*infoSlots[idInfoSlot], itemWaypoint);
                itemWaypoint->itemsInfoSlot()->append(itemInfoSlot);
            }
            break;
        default:
            break;
        }
    }
    emit nWpChanged();
    emit waypointPathChanged();
    emit itemsWaypointChanged();
    emit nInfoSlotChanged();
}

void ItemBank::setNInfoSlot(int count)
{
    if (count == _bankInfo.nInfoSlot)
        return;
    _bankInfo.nInfoSlot = count < 0 ? 0 : count;
    emit nInfoSlotChanged();
}

void ItemBank::updateInfoSlotFromWaypointIndex(int index)
{
    uint16_t indexLastInfoSlot = 0;
    uint16_t indexNumber = 0;
    if (index >= _itemsWaypoint->count() || _itemsWaypoint->count() == 0)
        return;
    if (index > 0) {
        ItemWaypoint* itemWaypoint = _itemsWaypoint->value<ItemWaypoint*>(index - 1);
        ItemInfoSlot* itemInfoSlot = itemWaypoint->itemsInfoSlot()->value<ItemInfoSlot*>(itemWaypoint->itemsInfoSlot()->count() - 1);
        indexLastInfoSlot = itemInfoSlot->idInfoSlot();
        indexNumber = itemInfoSlot->idWaypoint() + 1;
    }
    for (int i = index; i < _itemsWaypoint->count(); i++) {
        ItemWaypoint* itemWaypoint = _itemsWaypoint->value<ItemWaypoint*>(i);
        for (int j = 0; j < itemWaypoint->itemsInfoSlot()->count(); j++) {
            itemWaypoint->itemsInfoSlot()->value<ItemInfoSlot*>(j)->setIdInfoSlot(++indexLastInfoSlot);
            itemWaypoint->setIndexWaypoint(indexNumber);
        }
        indexNumber++;
    }
}

ItemWaypoint* ItemBank::insertWaypointFromInfoSlot(WaypointInfoSlot* infoSlot)
{
    if (_itemCurrentWaypoint)
        _itemCurrentWaypoint->setIsCurrentWaypoint(false);
    _itemCurrentWaypoint = new ItemWaypoint(*infoSlot, this);
    connect(_itemCurrentWaypoint, &ItemWaypoint::coordinateChanged, this, [this](){
        ItemWaypoint* wayPointSender = static_cast<ItemWaypoint*>(sender());
        int index = wayPointSender->indexWaypoint();
        _waypointPath[index].setValue(QVariant::fromValue(wayPointSender->coordinate()));
        emit waypointPathChanged();
    });
    _itemsWaypoint->insert(infoSlot->idWp, _itemCurrentWaypoint);
    emit itemsWaypointChanged();
    _waypointPath.insert(infoSlot->idWp, QVariant::fromValue(_itemCurrentWaypoint->coordinate()));
    _bankInfo.nWp = static_cast<uint16_t>(_itemsWaypoint->count());
    _indexCurrentWaypoint = infoSlot->idWp;
    emit nWpChanged();
    emit indexCurrentWaypointChanged();
    emit itemCurrentWaypointChanged();
    emit waypointPathChanged();
    updateInfoSlotFromWaypointIndex(_indexCurrentWaypoint + 1);
    return _itemCurrentWaypoint;
}

void ItemBank::insertWaypoint(QGeoCoordinate coordinate, int index)
{
    WaypointInfoSlot* waypointInfoSlot = new WaypointInfoSlot{};
    waypointInfoSlot->lat = coordinate.latitude();
    waypointInfoSlot->lon = coordinate.longitude();
    waypointInfoSlot->alt = static_cast<float>(coordinate.altitude());
    waypointInfoSlot->idBank = _bankInfo.idBank;
    uint16_t idInfoSlot = 0; //_itemsWaypoint->count() == 0 ? 0 : _itemsWaypoint.at(_indexCurrentWaypoint)->infoSlots().last()->idInfosl + 1;
    if (_itemsWaypoint->count() > 0) {
        QmlObjectListModel* listModel = _itemsWaypoint->value<ItemWaypoint*>(_indexCurrentWaypoint)->itemsInfoSlot();
        idInfoSlot = listModel->value<ItemInfoSlot*>(listModel->count() - 1)->idInfoSlot() + 1;
    }
    waypointInfoSlot->idInfoSlot = idInfoSlot;
    waypointInfoSlot->idWp = index < 0 ? 0 : index;//++_indexCurrentWaypoint;
    waypointInfoSlot->typeInfoSlot = (1 << 12) | 1;
    _itemCurrentWaypoint = insertWaypointFromInfoSlot(waypointInfoSlot);
    if (_itemCurrentWaypoint)
        _itemCurrentWaypoint->setIsCurrentWaypoint(true);
}

void ItemBank::removeWaypoint(int index)
{
    if (index >= _itemsWaypoint->count() || index < 0)
        return;
    _itemsWaypoint->removeAt(index)->deleteLater();
    if (_bankInfo.nWp > 0)
        _bankInfo.nWp--;
    _waypointPath.removeAt(index);
    emit waypointPathChanged();
    if (_itemsWaypoint->count() > 0) {
        if (index < _itemsWaypoint->count()) {   // 需要更新后续的航点
            updateInfoSlotFromWaypointIndex(index);
            _itemCurrentWaypoint = _itemsWaypoint->value<ItemWaypoint*>(index);
            if (_itemCurrentWaypoint)
                _itemCurrentWaypoint->setIsCurrentWaypoint(true);
            _indexCurrentWaypoint = index;
        } else if (index == _itemsWaypoint->count()) {  // 删除的是最后一个航点
            _itemCurrentWaypoint = _itemsWaypoint->value<ItemWaypoint*>(index - 1);
            if (_itemCurrentWaypoint)
                _itemCurrentWaypoint->setIsCurrentWaypoint(true);
            _indexCurrentWaypoint = index - 1;
        }
    } else {    // 航点已全部删完
        _itemCurrentWaypoint = nullptr;
        _indexCurrentWaypoint = -1;
    }
    emit itemCurrentWaypointChanged();
    emit indexCurrentWaypointChanged();
}

void ItemBank::setItemCurrentWaypoint(int index)
{
    if (index >= _itemsWaypoint->count() || index < 0)
        return;
    _indexCurrentWaypoint = index;
    emit indexCurrentWaypointChanged();
    if (_itemCurrentWaypoint)
        _itemCurrentWaypoint->setIsCurrentWaypoint(false);
    _itemCurrentWaypoint = _itemsWaypoint->value<ItemWaypoint*>(index);
    emit itemCurrentWaypointChanged();
    _itemCurrentWaypoint->setIsCurrentWaypoint(true);
}
