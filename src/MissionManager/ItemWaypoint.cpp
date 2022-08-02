#include "ItemWaypoint.h"
#include "ItemBank.h"
QGC_LOGGING_CATEGORY(ItemWaypointLog, "ItemWaypointLog")

ItemWaypoint::ItemWaypoint(const WaypointInfoSlot &infoSlot,  ItemBank* itemBank)
    : QObject(dynamic_cast<QObject*>(itemBank))
    ,_itemBank(itemBank)
    ,_infoSlotMajor(infoSlot)
{
    _itemsInfoSlot = new QmlObjectListModel(this);
    connect(_itemsInfoSlot, &QmlObjectListModel::dirtyChanged, this, [&](bool dirty){
        qCDebug(ItemWaypointLog) << "_itemsInfoSlot::dirtyChanged" << dirty;
        setDirty(dirty);
    });
    _infoSlotMajor.typeInfoSlot &= 0xfff0; // 低四位为大类编号0：非法，1：主要infoslot,2：补充infoslot,2~15：保留
    _infoSlotMajor.typeInfoSlot |= 1;
    _infoSlotMajor.typeInfoSlot &= 0x0fff; // 清空高四位，用于保存航点占用infoslot总数
    _infoSlotMajor.typeInfoSlot |= (1 << 12);
    _itemsInfoSlot->append(new ItemInfoSlot(_infoSlotMajor, this));
    _itemBank->setNInfoSlot(_itemBank->nInfoslot() + 1);
    emit itemsInfoSlotChanged();
    _indexCurrentInfoSlot = 0;
    emit indexCurrentInfoSlotChanged();
    _itemBank->updateInfoSlotFromWaypointIndex(_infoSlotMajor.idWp + 1);
    _coordinate.setLongitude(_infoSlotMajor.lon);
    _coordinate.setLatitude(_infoSlotMajor.lat);
    _coordinate.setAltitude(_infoSlotMajor.alt);
    emit coordinateChanged();
    emit indexWaypointChanged();
    qDebug(ItemWaypointLog) << "ItemWaypoint::ItemWaypoint() idBank:" << _infoSlotMajor.idBank
                            << "idWp:" << _infoSlotMajor.idWp << "idInfoSlot:" << _infoSlotMajor.idInfoSlot;
}

ItemWaypoint::~ItemWaypoint()
{
//    disconnect(_itemsInfoSlot, nullptr, nullptr, nullptr);
//    disconnect(this, nullptr, nullptr, nullptr);
//    qDebug(ItemWaypointLog) << "~ItemWaypoint()" << _infoSlotMajor.idBank << _infoSlotMajor.idWp << _infoSlotMajor.idInfoSlot;
}

void ItemWaypoint::appendInfoSlot()
{
    WaypointInfoSlot infoSlotMinor = _infoSlotMajor;
    infoSlotMinor.idInfoSlot = _itemsInfoSlot->value<ItemInfoSlot*>(_itemsInfoSlot->count() - 1)->idInfoSlot() + 1;
    infoSlotMinor.typeInfoSlot &= 0xfff0; // 低四位为大类编号0：非法，1：主要infoslot,2：补充infoslot,2~15：保留
    infoSlotMinor.typeInfoSlot |= 2;
    for(int i = 0; i < _itemsInfoSlot->count(); i++) { // 修改当前航点占用infoslot总数
        uint16_t type = _itemsInfoSlot->value<ItemInfoSlot*>(i)->tyInfoSlot();
        type &= 0x0fff; // 清空高四位，用于保存航点占用infoslot总数
        type |= (_itemsInfoSlot->count() << 12);
        _itemsInfoSlot->value<ItemInfoSlot*>(i)->setTyInfoSlot(type);
    }
    _itemsInfoSlot->append(new ItemInfoSlot(infoSlotMinor, this));
    _itemBank->setNInfoSlot(_itemBank->nInfoslot() + 1);
    emit itemsInfoSlotChanged();
    _itemBank->updateInfoSlotFromWaypointIndex(_infoSlotMajor.idWp + 1);
}

void ItemWaypoint::removeInfoSlot(int index)
{
    if (index < 0 || index >= _itemsInfoSlot->count())
        return;
    ItemInfoSlot* itemInfoSlot = qobject_cast<ItemInfoSlot*>(_itemsInfoSlot->removeAt(index));
    _itemBank->setNInfoSlot(_itemBank->nInfoslot() - 1);
    for(int i = index; i < _itemsInfoSlot->count(); i++) {
        uint16_t idInfoSlot = _itemsInfoSlot->value<ItemInfoSlot*>(i)->idInfoSlot();
        uint16_t tyInfoSlot = _itemsInfoSlot->value<ItemInfoSlot*>(i)->tyInfoSlot();
        _itemsInfoSlot->value<ItemInfoSlot*>(i)->setIdInfoSlot(--idInfoSlot);
        tyInfoSlot &= 0x0fff; // 清空高四位，用于保存航点占用infoslot总数
        tyInfoSlot |= (_itemsInfoSlot->count() << 12);
        _itemsInfoSlot->value<ItemInfoSlot*>(i)->setTyInfoSlot(tyInfoSlot);
    }
    _itemBank->updateInfoSlotFromWaypointIndex(itemInfoSlot->idWaypoint() + 1); // FIXME 序号可能不对

    itemInfoSlot->deleteLater();
    emit itemsInfoSlotChanged();
    _indexCurrentInfoSlot = qMin(index, _itemsInfoSlot->count() - 1);
    emit indexCurrentInfoSlotChanged();
}

void ItemWaypoint::updateInfoSlot()
{

}

void ItemWaypoint::_dirtyChanged(bool dirty)
{
    setDirty(dirty);
}

void ItemWaypoint::setIsCurrentWaypoint(bool currentWayPoint)
{
    if (_isCurrentWaypoint == currentWayPoint)
        return;
    _isCurrentWaypoint = currentWayPoint;
    emit isCurrentWaypointChanged();
}

void ItemWaypoint::setIndexCurrentInfoSlot(int index)
{
    _indexCurrentInfoSlot = index;
    emit indexCurrentInfoSlotChanged();
}

void ItemWaypoint::setCoordinate(QGeoCoordinate coordinate)
{
    if (coordinate == _coordinate)
        return;
    _infoSlotMajor.lat = coordinate.latitude();
    _infoSlotMajor.lon = coordinate.longitude();
    coordinate.setAltitude(_infoSlotMajor.alt);
    _coordinate = coordinate;
    emit coordinateChanged();
    setDirty(true);
}

int ItemWaypoint::indexWaypoint() const
{
    if (_itemsInfoSlot->count() > 0)
        return _itemsInfoSlot->value<ItemInfoSlot*>(0)->idWaypoint();
    else
        return -1;
}

void ItemWaypoint::setIndexWaypoint(int index)
{
    ItemInfoSlot* itemInfoSlot = _itemsInfoSlot->value<ItemInfoSlot*>(0);
    if (itemInfoSlot->idWaypoint() == index)
        return;
    for(int i = 0; i < _itemsInfoSlot->count(); i++) {
        _itemsInfoSlot->value<ItemInfoSlot*>(i)->setIdWaypoint(index);
    }
    emit indexWaypointChanged();
}

void ItemWaypoint::setDirty(bool dirty)
{
    qCDebug(ItemWaypointLog) << "ItemWaypoint::setDirty" << _infoSlotMajor.idWp << dirty << _dirty;
    if (dirty == _dirty)
        return;
    _dirty = dirty;
    emit dirtyChanged(dirty);
}
