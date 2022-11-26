#pragma once

#include <QObject>
#include <QGeoCoordinate>
#include "QmlObjectListModel.h"
#include "ShenHangVehicleData.h"
#include "ItemWaypoint.h"
//class ItemWaypoint;
Q_DECLARE_LOGGING_CATEGORY(ItemBankLog)
class ItemBank : public QObject
{
    Q_OBJECT
public:
    explicit ItemBank(const SingleBankInfo& bankInfo, QObject *parent = nullptr);
    ~ItemBank() override;

    Q_PROPERTY(bool dirty READ dirty WRITE setDirty NOTIFY dirtyChanged)
    Q_PROPERTY(int idBank READ idBank WRITE setIdBank NOTIFY idBankChanged)
    Q_PROPERTY(int tyBank READ tyBank)
    Q_PROPERTY(int nInfoslotMax READ nInfoslotMax)
    Q_PROPERTY(int iWp READ iWp)
    Q_PROPERTY(int nWp READ nWp NOTIFY nWpChanged)
    Q_PROPERTY(int nInfoslot READ nInfoslot WRITE setNInfoSlot NOTIFY nInfoSlotChanged)
    Q_PROPERTY(int idBankSuc READ idBankSuc)
    Q_PROPERTY(int idBankIwpSuc READ idBankIwpSuc)
    Q_PROPERTY(int actBankEnd READ actBankEnd)
    Q_PROPERTY(int stateBank READ stateBank)
    Q_PROPERTY(int switchState READ switchState)
    Q_PROPERTY(QVariantList waypointPath READ waypointPath NOTIFY waypointPathChanged)
    Q_PROPERTY(QmlObjectListModel* itemsWaypoint READ itemsWaypoint NOTIFY itemsWaypointChanged)
    Q_PROPERTY(int indexCurrentWaypoint READ indexCurrentWaypoint WRITE setCurrentWaypoint NOTIFY indexCurrentWaypointChanged)
    Q_PROPERTY(ItemWaypoint* itemCurrentWaypoint READ itemCurrentWaypoint NOTIFY itemCurrentWaypointChanged)

    bool dirty() const {return _dirty;}
    int idBank() const {return _bankInfo.idBank;}
    int tyBank() const {return _bankInfo.typeBank;}
    int nInfoslotMax() const {return _bankInfo.nInfoSlotMax;}
    int iWp() const {return _bankInfo.iWp;}
    int nWp() const {return _bankInfo.nWp;}
    int nInfoslot() const {return _bankInfo.nInfoSlot;}
    int idBankSuc() const {return _bankInfo.idBankSuc;}
    int idBankIwpSuc() const {return _bankInfo.idBankIwpSuc;}
    int actBankEnd() const {return _bankInfo.actBankEnd;}
    int stateBank() const {return _bankInfo.stateBank;}
    int switchState() const {return _bankInfo.switchState;}
    QVariantList waypointPath(){return _waypointPath;}
    QmlObjectListModel* itemsWaypoint() {return _itemsWaypoint;}
    int indexCurrentWaypoint() const {return _indexCurrentWaypoint;}
    SingleBankInfo bankInfo() const {return _bankInfo;}
    void setDirty(bool dirty);
    void setCurrentWaypoint(int index);
    void setIdBank(int index);
    void setInfoSlots(QMap<uint16_t, WaypointInfoSlot*> infoSlots);
    void setNInfoSlot(int count);

    /**
     * @brief updateInfoSlotFromWaypointIndex 从index航点更新航线中的infoslot的id信息
     */
    void updateInfoSlotFromWaypointIndex(int index);

    ItemWaypoint* itemCurrentWaypoint() const {return _itemCurrentWaypoint;}

    /**
     * @brief insertWaypoint    鼠标点击地图时，向当前航点后插入新的航点
     * @param coordinate        坐标
     * @param index             要插入的索引
     */
    Q_INVOKABLE void insertWaypoint(QGeoCoordinate coordinate, int index);

    /**
     * @brief removeWaypoint 删除序号为index的航点，默认选中后一航点
     * @param index
     */
    Q_INVOKABLE void removeWaypoint(int index);

    Q_INVOKABLE void setItemCurrentWaypoint(int index);

    /**
     * @brief inserWaypointFromInfoSlot 使用infoSlot插入新航点
     * @param infoSlot
     * @return 返回创建的航点
     */
    ItemWaypoint* insertWaypointFromInfoSlot(WaypointInfoSlot* infoSlot);

signals:
    void idBankChanged();
    void nWpChanged();
    void waypointPathChanged();
    void itemsWaypointChanged();
    void indexCurrentWaypointChanged();
    void itemCurrentWaypointChanged();
    void dirtyChanged(bool dirty);
    void nInfoSlotChanged();

private:
    bool _dirty = false;
    SingleBankInfo  _bankInfo;
    QVariantList    _waypointPath;
    QmlObjectListModel* _itemsWaypoint = nullptr; // 航点的可视元素
    int _indexCurrentWaypoint = -1;                      // 当前航点的序号
    ItemWaypoint* _itemCurrentWaypoint = nullptr;
};

