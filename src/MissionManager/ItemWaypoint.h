#pragma once

#include <QObject>
#include "ShenHangVehicleData.h"
#include "QmlObjectListModel.h"
#include "ItemInfoSlot.h"
Q_DECLARE_LOGGING_CATEGORY(ItemWaypointLog)
class ItemBank;

class ItemWaypoint : public QObject
{
    Q_OBJECT
public:
    explicit ItemWaypoint(const WaypointInfoSlot& infoSlot, ItemBank* itemBank = nullptr);
    ~ItemWaypoint() override;

    Q_PROPERTY(bool isCurrentWaypoint READ isCurrentWaypoint WRITE setIsCurrentWaypoint NOTIFY isCurrentWaypointChanged)
    Q_PROPERTY(int indexCurrentInfoSlot READ indexCurrentInfoSlot WRITE setIndexCurrentInfoSlot NOTIFY indexCurrentInfoSlotChanged)
    Q_PROPERTY(int indexWaypoint READ indexWaypoint WRITE setIndexWaypoint NOTIFY indexWaypointChanged)
    Q_PROPERTY(int indexBank READ indexBank)
    Q_PROPERTY(QmlObjectListModel* itemsInfoSlot READ itemsInfoSlot NOTIFY itemsInfoSlotChanged)
    Q_PROPERTY(QString mapVisualQML READ mapVisualQML  CONSTANT)                                           ///< QMl code for map visuals
    Q_PROPERTY(QGeoCoordinate coordinate READ coordinate WRITE setCoordinate NOTIFY coordinateChanged)                           ///< Does not include altitude
    Q_PROPERTY(bool dirty READ dirty WRITE setDirty NOTIFY dirtyChanged)

    bool isCurrentWaypoint() const {return _isCurrentWaypoint;}
    bool indexCurrentInfoSlot() const {return _indexCurrentInfoSlot;}
    QmlObjectListModel* itemsInfoSlot() {return _itemsInfoSlot;}
    void setIsCurrentWaypoint(bool currentWayPoint);
    void setIndexCurrentInfoSlot(int index);
    const QString mapVisualQML(void) const { return QStringLiteral("SimpleItemMapVisual.qml"); }
    const QGeoCoordinate& coordinate() const {return _coordinate;}
    const bool dirty() const {return _dirty;}

    void setCoordinate(QGeoCoordinate coordinate);
    int indexWaypoint() const;
    int indexBank() const {return _infoSlotMajor.idBank;}
    void setIndexWaypoint(int index);
    void setDirty(bool dirty);
    /**
     * @brief appendInfoSlot 点击添加infoslot按钮，在当前航点中插入新的infoslot
     * @return
     */
    Q_INVOKABLE void appendInfoSlot();

    /**
     * @brief removeInfoSlot 删除当前索引的infoslot
     * @param index
     */
    Q_INVOKABLE void removeInfoSlot(int index);

    Q_INVOKABLE void updateInfoSlot();


signals:
    void isCurrentWaypointChanged();
    void indexCurrentInfoSlotChanged();
    void itemsInfoSlotChanged();
    void coordinateChanged();
    void indexWaypointChanged();
    void dirtyChanged(bool dirty);
public slots:
    void _dirtyChanged(bool dirty);
private:
    ItemBank* _itemBank = nullptr;
    bool _isCurrentWaypoint = false;
    bool _dirty = false;
    int _indexCurrentInfoSlot = -1;                 // 当前选中的infoslot序号
    WaypointInfoSlot _infoSlotMajor{};
    QmlObjectListModel* _itemsInfoSlot = nullptr;   // 当前应显示航点的infoslot
    QGeoCoordinate _coordinate;
};

