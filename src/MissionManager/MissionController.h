/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include "PlanElementController.h"
#include "QmlObjectListModel.h"
#include "Vehicle.h"
#include "QGCLoggingCategory.h"
#include "QGCGeoBoundingCube.h"
#include "QGroundControlQmlGlobal.h"
#include "ItemBank.h"

#include <QHash>

class FlightPathSegment;
class VisualMissionItem;
class MissionItem;
class AppSettings;
class MissionManager;
class SimpleMissionItem;
class ComplexMissionItem;
class MissionSettingsItem;
class ItemInfoSlot;
class QDomDocument;
class PlanViewSettings;

Q_DECLARE_LOGGING_CATEGORY(MissionControllerLog)

typedef QPair<VisualMissionItem*,VisualMissionItem*> VisualItemPair;
typedef QHash<VisualItemPair, FlightPathSegment*> FlightPathSegmentHashTable;

class MissionController : public PlanElementController
{
    Q_OBJECT

public:
    MissionController(PlanMasterController* masterController, QObject* parent = nullptr);
    ~MissionController();
    enum ReadyForSaveState {
        ReadyForSave,
        NotReadyForSaveTerrain,
        NotReadyForSaveData,
    };
    Q_ENUM(ReadyForSaveState)
    typedef struct {
        double                      maxTelemetryDistance;
        double                      totalDistance;
        double                      totalTime;
        double                      hoverDistance;
        double                      hoverTime;
        double                      cruiseDistance;
        double                      cruiseTime;
        int                         mAhBattery;             ///< 0 for not available
        double                      hoverAmps;              ///< Amp consumption during hover
        double                      cruiseAmps;             ///< Amp consumption during cruise
        double                      ampMinutesAvailable;    ///< Amp minutes available from single battery
        double                      hoverAmpsTotal;         ///< Total hover amps used
        double                      cruiseAmpsTotal;        ///< Total cruise amps used
        int                         batteryChangePoint;     ///< -1 for not supported, 0 for not needed
        int                         batteriesRequired;      ///< -1 for not supported
        double                      vehicleYaw;
        double                      gimbalYaw;              ///< NaN signals yaw was never changed
        double                      gimbalPitch;            ///< NaN signals pitch was never changed
        // The following values are the state prior to executing this item
        QGCMAVLink::VehicleClass_t  vtolMode;               ///< Either VehicleClassFixedWing, VehicleClassMultiRotor, VehicleClassGeneric (mode unknown)
        double                      cruiseSpeed;
        double                      hoverSpeed;
        double                      vehicleSpeed;           ///< Either cruise or hover speed based on vehicle type and vtol state
    } MissionFlightStatus_t;

//    Q_PROPERTY(QmlObjectListModel*  visualItems                     READ visualItems                    NOTIFY visualItemsChanged)
    Q_PROPERTY(QmlObjectListModel*  itemsBank                       READ itemsBank                      NOTIFY itemsBankChanged)
    Q_PROPERTY(QmlObjectListModel*  simpleFlightPathSegments        READ simpleFlightPathSegments       CONSTANT)                               ///< Used by Plan view only for interactive editing
    Q_PROPERTY(QmlObjectListModel*  directionArrows                 READ directionArrows                CONSTANT)
    Q_PROPERTY(QStringList          complexMissionItemNames         READ complexMissionItemNames        NOTIFY complexMissionItemNamesChanged)
    Q_PROPERTY(QGeoCoordinate       plannedHomePosition             READ plannedHomePosition            NOTIFY plannedHomePositionChanged)      ///< Includes AMSL altitude
    Q_PROPERTY(QGeoCoordinate       previousCoordinate              MEMBER _previousCoordinate          NOTIFY previousCoordinateChanged)
    Q_PROPERTY(FlightPathSegment*   splitSegment                    MEMBER _splitSegment                NOTIFY splitSegmentChanged)             ///< Segment which show show + split ui element
    Q_PROPERTY(int                  indexCurrentBank                READ indexCurrentBank               NOTIFY indexCurrentBankChanged)
    Q_PROPERTY(double               progressPct                     READ progressPct                    NOTIFY progressPctChanged)
    Q_PROPERTY(int                  missionItemCount                READ missionItemCount               NOTIFY missionItemCountChanged)         ///< True mission item command count (only valid in Fly View)
    Q_PROPERTY(int                  currentMissionIndex             READ currentMissionIndex            NOTIFY currentMissionIndexChanged)
    Q_PROPERTY(int                  resumeMissionIndex              READ resumeMissionIndex             NOTIFY resumeMissionIndexChanged)       ///< Returns the item index two which a mission should be resumed. -1 indicates resume mission not available.
    Q_PROPERTY(int                  currentPlanViewSeqNum           READ currentPlanViewSeqNum          NOTIFY currentPlanViewSeqNumChanged)
    Q_PROPERTY(int                  currentPlanViewVIIndex          READ currentPlanViewVIIndex         NOTIFY currentPlanViewVIIndexChanged)
    Q_PROPERTY(VisualMissionItem*   currentPlanViewItem             READ currentPlanViewItem            NOTIFY currentPlanViewItemChanged)
    Q_PROPERTY(double               missionDistance                 READ missionDistance                NOTIFY missionDistanceChanged)
    Q_PROPERTY(double               missionTime                     READ missionTime                    NOTIFY missionTimeChanged)
    Q_PROPERTY(double               missionHoverDistance            READ missionHoverDistance           NOTIFY missionHoverDistanceChanged)
    Q_PROPERTY(double               missionCruiseDistance           READ missionCruiseDistance          NOTIFY missionCruiseDistanceChanged)
    Q_PROPERTY(double               missionHoverTime                READ missionHoverTime               NOTIFY missionHoverTimeChanged)
    Q_PROPERTY(double               missionCruiseTime               READ missionCruiseTime              NOTIFY missionCruiseTimeChanged)
    Q_PROPERTY(double               missionMaxTelemetry             READ missionMaxTelemetry            NOTIFY missionMaxTelemetryChanged)
    Q_PROPERTY(int                  batteryChangePoint              READ batteryChangePoint             NOTIFY batteryChangePointChanged)
    Q_PROPERTY(int                  batteriesRequired               READ batteriesRequired              NOTIFY batteriesRequiredChanged)
    Q_PROPERTY(QGCGeoBoundingCube*  travelBoundingCube              READ travelBoundingCube             NOTIFY missionBoundingCubeChanged)
    Q_PROPERTY(QString              surveyComplexItemName           READ surveyComplexItemName          CONSTANT)
    Q_PROPERTY(bool                 onlyInsertTakeoffValid          MEMBER _onlyInsertTakeoffValid      NOTIFY onlyInsertTakeoffValidChanged)
    Q_PROPERTY(bool                 isInsertTakeoffValid            MEMBER _isInsertTakeoffValid        NOTIFY isInsertTakeoffValidChanged)
    Q_PROPERTY(bool                 isInsertLandValid               MEMBER _isInsertLandValid           NOTIFY isInsertLandValidChanged)
    Q_PROPERTY(bool                 isROIActive                     MEMBER _isROIActive                 NOTIFY isROIActiveChanged)
    Q_PROPERTY(bool                 isROIBeginCurrentItem           MEMBER _isROIBeginCurrentItem       NOTIFY isROIBeginCurrentItemChanged)
    Q_PROPERTY(bool                 flyThroughCommandsAllowed       MEMBER _flyThroughCommandsAllowed   NOTIFY flyThroughCommandsAllowedChanged)
    Q_PROPERTY(double               minAMSLAltitude                 MEMBER _minAMSLAltitude             NOTIFY minAMSLAltitudeChanged)          ///< Minimum altitude associated with this mission. Used to calculate percentages for terrain status.
    Q_PROPERTY(double               maxAMSLAltitude                 MEMBER _maxAMSLAltitude             NOTIFY maxAMSLAltitudeChanged)          ///< Maximum altitude associated with this mission. Used to calculate percentages for terrain status.

    Q_PROPERTY(QGroundControlQmlGlobal::AltitudeMode globalAltitudeMode         READ globalAltitudeMode         WRITE setGlobalAltitudeMode NOTIFY globalAltitudeModeChanged)   ///< AltitudeModeNone indicates the plan can used mixed modes
    Q_PROPERTY(QGroundControlQmlGlobal::AltitudeMode globalAltitudeModeDefault  READ globalAltitudeModeDefault  NOTIFY globalAltitudeModeChanged)                               ///< Default to use for newly created items

    /******************** 沈航整体bank信息 ********************/
    Q_PROPERTY(int largeBankInfoSlotCapacity READ largeBankInfoSlotCapacity NOTIFY largeBankInfoSlotCapacityChanged)
    Q_PROPERTY(int smallBankInfoSlotCapacity READ smallBankInfoSlotCapacity NOTIFY smallBankInfoSlotCapacityChanged)
    Q_PROPERTY(int largeBankNumber READ largeBankNumber NOTIFY largeBankNumberChanged)
    Q_PROPERTY(int smallBankNumber READ smallBankNumber NOTIFY smallBankNumberChanged)
    Q_PROPERTY(int idTransientBank READ idTransientBank NOTIFY idTransientBankChanged)

    Q_INVOKABLE void removeVisualItemBank(int viIndex);

    /**
     * @brief insertNewBank 插入新的bank
     * @return
     */
    Q_INVOKABLE ItemBank* insertNewBank();

    Q_INVOKABLE void resumeMission(int resumeIndex);

    /// Updates the altitudes of the items in the current mission to the new default altitude
    Q_INVOKABLE void applyDefaultMissionAltitude(void);

    enum SendToVehiclePreCheckState {
        SendToVehiclePreCheckStateOk,                       // Ok to send plan to vehicle
        SendToVehiclePreCheckStateNoActiveVehicle,          // There is no active vehicle
        SendToVehiclePreCheckStateFirwmareVehicleMismatch,  // Firmware/Vehicle type for plan mismatch with actual vehicle
        SendToVehiclePreCheckStateActiveMission,            // Vehicle is currently flying a mission
    };
    Q_ENUM(SendToVehiclePreCheckState)

    Q_INVOKABLE SendToVehiclePreCheckState sendToVehiclePreCheck(void);

    /// Sends the mission items to the specified vehicle
    static void sendItemsToVehicle(Vehicle* vehicle, QmlObjectListModel* itemsBank);

    bool loadJsonFile(QFile& file, QString& errorString);

    QGCGeoBoundingCube* travelBoundingCube  () { return &_travelBoundingCube; }
    QGeoCoordinate      takeoffCoordinate   () { return _takeoffCoordinate; }

    // Overrides from PlanElementController
    bool supported                  (void) const final { return true; }
    void start                      (bool flyView) final;
    void save                       (QJsonObject& json) final;
    bool load                       (const QJsonObject& json, QString& errorString) final;
    void loadFromVehicle            (void) final;
    void sendToVehicle              (void) final;
    void removeAll                  (void) final;
    void removeAllFromVehicle       (void) final;
    bool syncInProgress             (void) const final;
    bool dirty                      (void) const final;
    void setDirty                   (bool dirty) final;
    bool containsItems              (void) const final;
    bool showPlanFromManagerVehicle (void) final;

    // Property accessors
    QmlObjectListModel* itemsBank                   (void) { return _itemsBank; }
    QmlObjectListModel* simpleFlightPathSegments    (void) { return &_simpleFlightPathSegments; }
    QmlObjectListModel* directionArrows             (void) { return &_directionArrows; }
    QStringList         complexMissionItemNames     (void) const;
    QGeoCoordinate      plannedHomePosition         (void) const;
    VisualMissionItem*  currentPlanViewItem         (void) const { return _currentPlanViewItem; }
    double              progressPct                 (void) const { return _progressPct; }
    QString             surveyComplexItemName       (void) const;
    bool                isInsertTakeoffValid        (void) const;
    double              minAMSLAltitude             (void) const { return _minAMSLAltitude; }
    double              maxAMSLAltitude             (void) const { return _maxAMSLAltitude; }

    int indexCurrentBank            (void) const { return _indexCurrentBank; }
    int missionItemCount            (void) const { return _missionItemCount; }
    int currentMissionIndex         (void) const;
    int resumeMissionIndex          (void) const { return 0; }
    int currentPlanViewSeqNum       (void) const { return _currentPlanViewSeqNum; }
    int currentPlanViewVIIndex      (void) const { return _currentPlanViewVIIndex; }

    double  missionDistance         (void) const { return _missionFlightStatus.totalDistance; }
    double  missionTime             (void) const { return _missionFlightStatus.totalTime; }
    double  missionHoverDistance    (void) const { return _missionFlightStatus.hoverDistance; }
    double  missionHoverTime        (void) const { return _missionFlightStatus.hoverTime; }
    double  missionCruiseDistance   (void) const { return _missionFlightStatus.cruiseDistance; }
    double  missionCruiseTime       (void) const { return _missionFlightStatus.cruiseTime; }
    double  missionMaxTelemetry     (void) const { return _missionFlightStatus.maxTelemetryDistance; }

    int  batteryChangePoint         (void) const { return _missionFlightStatus.batteryChangePoint; }    ///< -1 for not supported, 0 for not needed
    int  batteriesRequired          (void) const { return _missionFlightStatus.batteriesRequired; }     ///< -1 for not supported

    bool isEmpty                    (void) const;

    QGroundControlQmlGlobal::AltitudeMode globalAltitudeMode(void);
    QGroundControlQmlGlobal::AltitudeMode globalAltitudeModeDefault(void);
    void setGlobalAltitudeMode(QGroundControlQmlGlobal::AltitudeMode altMode);

    int largeBankInfoSlotCapacity() const { return _totalBankInfo.largeBankInfoSlotCapacity; }
    void setLargeBankInfoSlotCapacity(int newLargeBankInfoSlotCapacity);

    int smallBankInfoSlotCapacity() const { return _totalBankInfo.smallBankInfoSlotCapacity; }
    int largeBankNumber() const { return _totalBankInfo.largeBankNumber; }
    int smallBankNumber() const { return _totalBankInfo.smallBankNumber; }
    int idTransientBank() const { return _totalBankInfo.idTransientBank; }

    QmlObjectListModel *initLoadedInfoSlotsInBanks(const QMap<uint16_t, SingleBankInfo *> &bankInfos, const QMap<uint16_t, QMap<uint16_t, WaypointInfoSlot *> > &infoSlotsInBanks);


    bool loadBankInfo(const QJsonObject& json, SingleBankInfo* bankInfo, QMap<uint16_t, WaypointInfoSlot*> &infoSlots, QString& errorString);
signals:
    void visualItemsChanged                 (void);
    void infoslotVisualItemsChanged         (void);
    void itemsBankChanged         (void);
    void waypointPathChanged                (void);
    void splitSegmentChanged                (void);
    void newItemsFromVehicle                (void);
    void missionDistanceChanged             (double missionDistance);
    void missionTimeChanged                 (void);
    void missionHoverDistanceChanged        (double missionHoverDistance);
    void missionHoverTimeChanged            (void);
    void missionCruiseDistanceChanged       (double missionCruiseDistance);
    void missionCruiseTimeChanged           (void);
    void missionMaxTelemetryChanged         (double missionMaxTelemetry);
    void complexMissionItemNamesChanged     (void);
    void resumeMissionIndexChanged          (void);
    void resumeMissionReady                 (void);
    void resumeMissionUploadFail            (void);
    void batteryChangePointChanged          (int batteryChangePoint);
    void batteriesRequiredChanged           (int batteriesRequired);
    void plannedHomePositionChanged         (QGeoCoordinate plannedHomePosition);
    void progressPctChanged                 (double progressPct);
    void indexCurrentBankChanged            ();
    void currentMissionIndexChanged         (int currentMissionIndex);
    void currentPlanViewSeqNumChanged       (void);
    void currentPlanViewVIIndexChanged      (void);
    void currentPlanViewItemChanged         (void);
    void missionBoundingCubeChanged         (void);
    void missionItemCountChanged            (int missionItemCount);
    void onlyInsertTakeoffValidChanged      (void);
    void isInsertTakeoffValidChanged        (void);
    void isInsertLandValidChanged           (void);
    void isROIActiveChanged                 (void);
    void isROIBeginCurrentItemChanged       (void);
    void flyThroughCommandsAllowedChanged   (void);
    void previousCoordinateChanged          (void);
    void minAMSLAltitudeChanged             (double minAMSLAltitude);
    void maxAMSLAltitudeChanged             (double maxAMSLAltitude);
    void recalcTerrainProfile               (void);
    void _recalcMissionFlightStatusSignal   (void);
    void _recalcFlightPathSegmentsSignal    (void);
    void globalAltitudeModeChanged          (void);

    void largeBankInfoSlotCapacityChanged();
    void smallBankInfoSlotCapacityChanged();
    void largeBankNumberChanged();
    void smallBankNumberChanged();
    void idTransientBankChanged();

private slots:
    void _totalBankInfoChangedFromMissionManager(const TotalBankInfo& totalBankInfo);
    void _newMissionItemsAvailableFromVehicle   (bool removeAllRequested);
    void _itemCommandChanged                    (void);
    void _inProgressChanged                     (bool inProgress);
    void _currentMissionIndexChanged            (int sequenceNumber);
    void _updateContainsItems                   (void);
    void _progressPctChanged                    (double progressPct);
    void _itemsBankDirtyChanged                 (bool dirty);
    void _managerSendComplete                   (bool error);
    void _managerRemoveAllComplete              (bool error);
    void _updateTimeout                         (void);
    void _complexBoundingBoxChanged             (void);
    void _managerVehicleChanged                 (Vehicle* managerVehicle);
    void _takeoffItemNotRequiredChanged         (void);

private:
    void                    _init                               (void);
    void                    _recalcSequence                     (void);
    void                    _initAllVisualItems                 (void);
    void                    _deinitAllVisualItems               (void);
    void                    _setupActiveVehicle                 (Vehicle* activeVehicle, bool forceLoadFromVehicle);
    void                    _centerHomePositionOnMissionItems   (QmlObjectListModel* visualItems);
    bool                    _loadJsonMissionFile                (const QJsonObject& json,  QMap<uint16_t, SingleBankInfo *> &bankInfos, QMap<uint16_t, QMap<uint16_t, WaypointInfoSlot*>> &infoSlots, QString& errorString);
    void                    _resetMissionFlightStatus           (void);
    void                    _addHoverTime                       (double hoverTime, double hoverDistance, int waypointIndex);
    void                    _addCruiseTime                      (double cruiseTime, double cruiseDistance, int wayPointIndex);
    void                    _updateBatteryInfo                  (int waypointIndex);
    bool                    _loadItemsFromJson                  (const QJsonObject& json, QMap<uint16_t, SingleBankInfo*> bankInfos, QMap<uint16_t, QMap<uint16_t,WaypointInfoSlot*>>& infoSlots, QString& errorString);
    void                    _initLoadedVisualItems              (QmlObjectListModel* loadedVisualItems);
    FlightPathSegment*      _addFlightPathSegment               (FlightPathSegmentHashTable& prevItemPairHashTable, VisualItemPair& pair);
    void                    _addTimeDistance                    (bool vtolInHover, double hoverTime, double cruiseTime, double extraTime, double distance, int seqNum);
    bool                    _isROIBeginItem                     (SimpleMissionItem* simpleItem);
    bool                    _isROICancelItem                    (SimpleMissionItem* simpleItem);
    FlightPathSegment*      _createFlightPathSegmentWorker      (VisualItemPair& pair);
    void                    _allItemsRemoved                    (void);
    void                    _firstItemAdded                     (void);

    static double           _normalizeLat                       (double lat);
    static double           _normalizeLon                       (double lon);
    static bool             _convertToMissionItems              (QmlObjectListModel* itemsBank, QMap<uint16_t, SingleBankInfo *> &bankInfos, QMap<uint16_t, QMap<uint16_t, WaypointInfoSlot *> > &infoSlotsInBanks, QObject* missionItemParent);

private:
    Vehicle*                    _controllerVehicle =            nullptr;
    Vehicle*                    _managerVehicle =               nullptr;
    MissionManager*             _missionManager =               nullptr;
    int                         _indexCurrentBank =             -1;         // 当前选中的索引，-1表示不选中列表中的元素
    int                         _missionItemCount =             0;
    QmlObjectListModel*         _itemsBank =                    nullptr;
    PlanViewSettings*           _planViewSettings =             nullptr;
    QmlObjectListModel          _simpleFlightPathSegments =     nullptr;
    QmlObjectListModel          _directionArrows =              nullptr;
    FlightPathSegmentHashTable  _flightPathSegmentHashTable;
    bool                        _firstItemsFromVehicle =        false;
    bool                        _itemsRequested =               false;
    bool                        _inRecalcSequence =             false;
    MissionFlightStatus_t       _missionFlightStatus;
    AppSettings*                _appSettings =                  nullptr;
    double                      _progressPct =                  0;
    int                         _currentPlanViewSeqNum =        -1;
    int                         _currentPlanViewVIIndex =       -1;
    VisualMissionItem*          _currentPlanViewItem =          nullptr;
    QTimer                      _updateTimer;
    QGCGeoBoundingCube          _travelBoundingCube;
    QGeoCoordinate              _takeoffCoordinate;
    QGeoCoordinate              _previousCoordinate;
    FlightPathSegment*          _splitSegment =                 nullptr;
    bool                        _onlyInsertTakeoffValid =       true;
    bool                        _isInsertTakeoffValid =         true;
    bool                        _isInsertLandValid =            false;
    bool                        _isROIActive =                  false;
    bool                        _flyThroughCommandsAllowed =    false;
    bool                        _isROIBeginCurrentItem =        false;
    double                      _minAMSLAltitude =              0;
    double                      _maxAMSLAltitude =              0;
    bool                        _missionContainsVTOLTakeoff =   false;

    /******************** 沈航bank信息 ********************/
    TotalBankInfo _totalBankInfo = {};
    QGroundControlQmlGlobal::AltitudeMode _globalAltMode = QGroundControlQmlGlobal::AltitudeModeRelative;

    static const char* _settingsGroup;

    // Json file keys for persistence
    static const char* _jsonKeyFileType;
    static const char* _jsonKeyFirmwareType;
    static const char* _jsonKeyVehicleType;
    static const char* _jsonKeyCruiseSpeed;
    static const char* _jsonKeyHoverSpeed;
    static const char* _jsonKeyBankInfo;
    static const char* _jsonKeyPlannedHomePosition;
    static const char* _jsonParamsKey;
    static const char* _jsonKeyGlobalPlanAltitudeMode;

    /******************** 单个航线信息 ********************/
    static const char* _jsonKeyIdBank;
    static const char* _jsonKeyTypeBank;
    static const char* _jsonKeyNInfoSlotMax;
    static const char* _jsonKeyIWp;
    static const char* _jsonKeyNWp;
    static const char* _jsonKeyNInfoSlot;
    static const char* _jsonKeyIdBankSuc;
    static const char* _jsonKeyIdBankIWpSuc;
    static const char* _jsonKeyActBankEnd;
    static const char* _jsonKeyStateBank;
    static const char* _jsonKeySwitchState;
    static const char* _jsonKeyInfoSlots;

    /******************** 单个infoSlot信息 ********************/
    static const char* _jsonKeyIdWp;
    static const char* _jsonKeyIdInfoSlot;
    static const char* _jsonKeyTypeInfoSlot;
    static const char* _jsonKeyLon;
    static const char* _jsonKeyLat;
    static const char* _jsonKeyAlt;
    static const char* _jsonKeyRadiusCh;
    static const char* _jsonKeyRVGnd;
    static const char* _jsonKeyRRocDoc;
    static const char* _jsonKeySwTd;
    static const char* _jsonKeySwVGnd10x;
    static const char* _jsonKeyRHdgDeg100x;
    static const char* _jsonKeyActDept;
    static const char* _jsonKeyActArrvl;
    static const char* _jsonKeySwCondSet;
    static const char* _jsonKeyActInFlt;
    static const char* _jsonKeyHgtHdgCtrlMode;
    static const char* _jsonKeySwAngDeg;
    static const char* _jsonKeySwTurns;
    static const char* _jsonKeyGdMode;
    static const char* _jsonKeyMnvrSty;
    static const char* _jsonKeyTransSty;
    static const char* _jsonKeyReserved0;
    static const char* _jsonKeyReserved1;

    // Deprecated V1 format keys
    static const char* _jsonMavAutopilotKey;
    static const char* _jsonComplexItemsKey;

    static const int    _missionFileVersion;

    WaypointInfoSlot infoslotTest = {};
};
