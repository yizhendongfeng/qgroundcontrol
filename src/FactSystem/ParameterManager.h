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
#include <QMap>
#include <QXmlStreamReader>
#include <QLoggingCategory>
#include <QMutex>
#include <QDir>
#include <QJsonObject>

#include "FactSystem.h"
#include "AutoPilotPlugin.h"
#include "QGCMAVLink.h"
#include "Vehicle.h"
#include "ShenHangParameterMetaData.h"

Q_DECLARE_LOGGING_CATEGORY(ParameterManagerVerbose1Log)
Q_DECLARE_LOGGING_CATEGORY(ParameterManagerVerbose2Log)
#define MAVPACKED( __Declaration__ ) __pragma( pack(push, 1) ) __Declaration__ __pragma( pack(pop) )
struct ParamUnion {
    union {
        float paramFloat;
        int32_t paramInt32;
        uint32_t paramUint32;
        int16_t paramInt16;
        uint16_t paramUint16;
        int8_t paramInt8;
        uint8_t paramUint8;
        uint8_t bytes[4];
    };
    FactMetaData::ValueType_t type;
};
/**
 * Old-style 4 byte param union
 *
 * This struct is the data format to be used when sending
 * parameters. The parameter should be copied to the native
 * type (without type conversion)
 * and re-instanted on the receiving side using the
 * native type as well.
 */
MAVPACKED(
typedef struct param_union {
    union {
        float param_float;
        int32_t param_int32;
        uint32_t param_uint32;
        int16_t param_int16;
        uint16_t param_uint16;
        int8_t param_int8;
        uint8_t param_uint8;
        uint8_t bytes[4];
    };
    uint8_t type;
}) mavlink_param_union_t;


class ParameterEditorController;

class ParameterManager : public QObject
{
    Q_OBJECT

    friend class ParameterEditorController;

public:
    /// @param uas Uas which this set of facts is associated with
    ParameterManager(Vehicle* vehicle);

    Q_PROPERTY(bool     parametersReady     READ parametersReady    NOTIFY parametersReadyChanged)      ///< true: Parameters are ready for use
    Q_PROPERTY(bool     missingParameters   READ missingParameters  NOTIFY missingParametersChanged)    ///< true: Parameters are missing from firmware response, false: all parameters received from firmware
    Q_PROPERTY(double   loadProgress        READ loadProgress       NOTIFY loadProgressChanged)

    bool parametersReady    (void) const { return _parametersReady; }
    bool missingParameters  (void) const { return _missingParameters; }
    double loadProgress     (void) const { return _loadProgress; }

    void shenHangMessageReceived(ShenHangProtocolMessage message);

    /// Re-request the full set of parameters from the autopilot
    /**
     * @brief refreshGroupParameters 请求参数，默认请求所有参数组参数
     * @param groupId 0~0xFE对应各设置组id；0XFF查询所有设置组。
     */
    void refreshGroupParameters(uint8_t groupId = 0XFF);

    void resetAllToVehicleConfiguration();
    void parameterGroupCommand(CommandParamType command, uint8_t groupId);
    /******************** 沈航参数组操作函数 ********************/
    /**
     * @brief resendParameterCommand 超时重新发送参数命令
     */
    void resendParameterCommand();

    /// Returns true if the specifed parameter exists
    ///     @param groupId group that parameter belong to
    ///     @param addrOffset: Parameter index in group
    bool parameterExists(const uint8_t& groupId, const uint16_t& addrOffset);

    /// Returns all parameter names
    QStringList parameterNames(int componentId);
    QMap<uint8_t /* groupId */, QMap<uint16_t /* addrOffset */, Fact*>> getCompGroupId2FactMap() const {return _mapGroupId2FactMap;}

    Fact* getParameter(const uint8_t& groupId, const uint16_t& addrOffset);

    Vehicle* vehicle(void) { return _vehicle; }
signals:
    void parametersReadyChanged     (bool parametersReady);
    void missingParametersChanged   (bool missingParameters);
    void loadProgressChanged        (float value);
    void factAdded                  (Fact* fact);

private slots:
    void    _factRawValueUpdated                (const QVariant& rawValue);

private:
    void    _loadParameterMetaDataFromFile();
    void    _handleParamValue                   (uint8_t groupId, uint16_t addrOffset, FactMetaData::ValueType_t mavParamType, QVariant parameterValue);
    void    _waitingParamTimeout                (void);
    int     _actualComponentId                  (int componentId);
    void    _sendParamCommandToVehicle          (CommandParamType command, uint8_t groupId, uint8_t lenCfg = 0, uint16_t addrOffset = 0, FactMetaData::ValueType_t valueType = FactMetaData::valueTypeUint8, const QVariant& value = 0);
    void    _loadMetaData                       (void);
    void    _clearMetaData                      (void);
    QString _logVehiclePrefix                   (int componentId);
    void    _setLoadProgress                    (double loadProgress);
    void    _updateProgressBar                  (void);
    void    _checkInitialLoadComplete           (void);

    static QVariant _stringToTypedVariant(const QString& string, FactMetaData::ValueType_t type, bool failOk = false);

    Vehicle*            _vehicle;

    QMap<QString /* fact name */,  Fact*> _mapCompId2FactMap;
    QMap<uint8_t /* groupId */, QMap<uint16_t /* addrOffset */, Fact*>> _mapGroupId2FactMap;
    ShenHangProtocolMessage lastSentMessage = {};
    double      _loadProgress;                  ///< Parameter load progess, [0.0,1.0]
    bool        _parametersReady;               ///< true: parameter load complete
    bool        _missingParameters;             ///< true: parameter missing from initial load
    bool        _metaDataAddedToFacts;          ///< true: FactMetaData has been adde to the default component facts
    bool        _logReplay;                     ///< true: running with log replay link

    typedef QPair<int /* FactMetaData::ValueType_t */, QVariant /* Fact::rawValue */> ParamTypeVal;

    bool _readParamIndexProgressActive =    false;
    bool _readParamNameProgressActive =     false;
    bool _writeParamProgressActive =        false;

    static const int    _maxParameterRetry = 5;                 ///< Maximum retries read/write
    bool                _disableAllRetries;                     ///< true: Don't retry any requests (used for testing)

    ///Key: ComponentId, Value:{ Key: GroupId, Value: List {addrOffset} }
    QMap<uint8_t, QList<uint16_t> > _waitingReadParamIndexMap;

    QMap<uint8_t, QList<uint16_t> > _failedReadParamIndexMap;   ///< failed parameter index

    int _totalParamCount;                       ///< Number of parameters across all components
    int _paramResentCount = 0;                  ///< Number of parameters resent

    QTimer _waitingParamTimeoutTimer;
    ShenHangParameterMetaData* _shenHangMetaData = nullptr;
    Fact _defaultFact;   ///< Used to return default fact, when parameter not found
    bool lastCommandGroupApplyToAll;
    static const char* _jsonParametersKey;
    static const char* _jsonCompIdKey;
    static const char* _jsonParamNameKey;
    static const char* _jsonParamValueKey;

};
