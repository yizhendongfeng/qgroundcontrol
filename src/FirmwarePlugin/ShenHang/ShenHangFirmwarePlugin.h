/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include "FirmwarePlugin.h"
#include "ParameterManager.h"
#include "ShenHangParameterMetaData.h"

class ShenHangFirmwarePlugin : public FirmwarePlugin
{
    Q_OBJECT

public:
    ShenHangFirmwarePlugin   ();
    ~ShenHangFirmwarePlugin  () override;

    // Called internally only
    void _changeAltAfterPause(void* resultHandlerData, bool pauseSucceeded);

    // Overrides from FirmwarePlugin

    QList<VehicleComponent*> componentsForVehicle(AutoPilotPlugin* vehicle) override;
    QList<MAV_CMD> supportedMissionCommands(QGCMAVLink::VehicleClass_t vehicleClass) override;

    AutoPilotPlugin*    autopilotPlugin                 (Vehicle* vehicle) override;
    bool                isCapable                       (const Vehicle *vehicle, FirmwareCapabilities capabilities) override;
    QStringList         flightModes                     (Vehicle* vehicle) override;
    QString             flightMode                      (uint8_t base_mode, uint32_t custom_mode) const override;
    bool                setFlightMode                   (const QString& flightMode, uint8_t* base_mode, uint32_t* custom_mode) override;
    void                setGuidedMode                   (Vehicle* vehicle, bool guidedMode) override;
    QString             pauseFlightMode                 (void) const override { return _holdFlightMode; }
    QString             missionFlightMode               (void) const override { return _missionFlightMode; }
    QString             rtlFlightMode                   (void) const override { return _rtlFlightMode; }
    QString             landFlightMode                  (void) const override { return _landingFlightMode; }
    QString             takeControlFlightMode           (void) const override { return _manualFlightMode; }
    QString             gotoFlightMode                  (void) const override { return _holdFlightMode; }
    void                pauseVehicle                    (Vehicle* vehicle) override;
    void                guidedModeRTL                   (Vehicle* vehicle, bool smartRTL) override;
    void                guidedModeLand                  (Vehicle* vehicle) override;
    void                guidedModeTakeoff               (Vehicle* vehicle, double takeoffAltRel) override;
    void                guidedModeGotoLocation          (Vehicle* vehicle, const QGeoCoordinate& gotoCoord) override;
    void                guidedModeChangeAltitude        (Vehicle* vehicle, double altitudeRel, bool pauseVehicle) override;
    double              minimumTakeoffAltitude          (Vehicle* vehicle) override;
    void                startMission                    (Vehicle* vehicle) override;
    bool                isGuidedMode                    (const Vehicle* vehicle) const override;
    void                initializeVehicle               (Vehicle* vehicle) override;
    bool                sendHomePositionToVehicle       (void) override;
    QString             missionCommandOverrides         (QGCMAVLink::VehicleClass_t vehicleClass) const override;
    /**
     * @brief _getMetaDataForFact 此函数弃用，由_getMetaDataForFactByIndexes函数代替
     * @param parameterMetaData
     * @param name
     * @param type
     * @param vehicleType
     * @return
     */
    FactMetaData*       _getMetaDataForFact             (QObject* parameterMetaData, const QString& name, FactMetaData::ValueType_t type, MAV_TYPE vehicleType) override;

    FactMetaData*       _getMetaDataForFactByIndexes    (QObject* parameterMetaData, uint8_t idGroup, uint16_t addrOffset, MAV_TYPE vehicleType) override;

    QString             _internalParameterMetaDataFile  (Vehicle* vehicle) override { Q_UNUSED(vehicle); return QString(":/FirmwarePlugin/ShenHang/ShenHangParameterFactMetaData.xml"); }
    void                _getParameterMetaDataVersionInfo(const QString& metaDataFile, int& majorVersion, int& minorVersion) override;
    QObject*            _loadParameterMetaData          (const QString& metaDataFile) final;
    bool                adjustIncomingMavlinkMessage    (Vehicle* vehicle, mavlink_message_t* message) override;
    QString             offlineEditingParamFile         (Vehicle* vehicle) override { Q_UNUSED(vehicle); return QStringLiteral(":/FirmwarePlugin/ShenHang/ShenHang.OfflineEditing.params"); }
    QString             brandImageIndoor                (const Vehicle* vehicle) const override { Q_UNUSED(vehicle); return QStringLiteral("/qmlimages/ShenHang/BrandImage"); }
    QString             brandImageOutdoor               (const Vehicle* vehicle) const override { Q_UNUSED(vehicle); return QStringLiteral("/qmlimages/ShenHang/BrandImage"); }
    QString             autoDisarmParameter             (Vehicle* vehicle) override { Q_UNUSED(vehicle); return QStringLiteral("COM_DISARM_LAND"); }
    uint32_t            highLatencyCustomModeTo32Bits   (uint16_t hlCustomMode) override;
    bool                supportsNegativeThrust          (Vehicle *vehicle) override;

protected:
    typedef struct {
        uint8_t         main_mode;
        uint8_t         sub_mode;
        const QString*  name;       ///< Name for flight mode
        bool            canBeSet;   ///< true: Vehicle can be set to this flight mode
        bool            fixedWing;  /// fixed wing compatible
        bool            multiRotor; /// multi rotor compatible
    } FlightModeInfo_t;

    QList<FlightModeInfo_t> _flightModeInfoList;

    // Use these constants to set flight modes using setFlightMode method. Don't use hardcoded string names since the
    // names may change.

    // If plugin superclass wants to change a mode name, then set a new name for the flight mode in the superclass constructor
    QString _manualFlightMode;
    QString _acroFlightMode;
    QString _stabilizedFlightMode;
    QString _rattitudeFlightMode;
    QString _altCtlFlightMode;
    QString _posCtlFlightMode;
    QString _offboardFlightMode;
    QString _readyFlightMode;
    QString _takeoffFlightMode;
    QString _holdFlightMode;
    QString _missionFlightMode;
    QString _rtlFlightMode;
    QString _landingFlightMode;
    QString _preclandFlightMode;
    QString _rtgsFlightMode;
    QString _simpleFlightMode;
    QString _orbitFlightMode;

private slots:
    void _mavCommandResult(int vehicleId, int component, int command, int result, bool noReponseFromVehicle);

private:
    void    _handleAutopilotVersion         (Vehicle* vehicle, mavlink_message_t* message);

    QString _getLatestVersionFileUrl        (Vehicle* vehicle) override;
    QString _versionRegex                   () override;

    // Any instance data here must be global to all vehicles
    // Vehicle specific data should go into ShenHangFirmwarePluginInstanceData
};

class ShenHangFirmwarePluginInstanceData : public QObject
{
    Q_OBJECT

public:
    ShenHangFirmwarePluginInstanceData(QObject* parent = nullptr);

    bool versionNotified;  ///< true: user notified over version issue
};
