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
Q_DECLARE_LOGGING_CATEGORY(ParameterManagerDebugCacheFailureLog)
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
    Q_PROPERTY(bool     pendingWrites       READ pendingWrites      NOTIFY pendingWritesChanged)        ///< true: There are still pending write updates against the vehicle

    bool parametersReady    (void) const { return _parametersReady; }
    bool missingParameters  (void) const { return _missingParameters; }
    double loadProgress     (void) const { return _loadProgress; }

    /// @return Directory of parameter caches
    static QDir parameterCacheDir();

    /// @return Location of parameter cache file
    static QString parameterCacheFile(int vehicleId, int componentId);

    void shenHangMessageReceived(ShenHangProtocolMessage message);

    /// Re-request the full set of parameters from the autopilot
    /**
     * @brief refreshGroupParameters 请求参数，默认请求所有参数组参数
     * @param groupId 0~0xFE对应各设置组id；0xff查询所有设置组。
     */
    void refreshGroupParameters(uint8_t groupId = 0xff);

    /// Request a refresh on all parameters that begin with the specified prefix
//    void refreshParametersPrefix(int componentId, const QString& namePrefix);

    void resetAllParametersToDefaults();
    void resetAllToVehicleConfiguration();

    /******************** 沈航参数组操作函数 ********************/
    /**
     * 操作参数组，默认其addrOffset为0，便于超时检测
     * @param groupId 0~0xFE对应各设置组id；0xFF重置所有设置组
     * @param command COMMAND_RESET_PARAM = 0,COMMAND_LOAD_PARAM = 1, COMMAND_SAVE_PARAM = 2
     */
    void ParameterGroupCommand(CommandParamType command, uint8_t groupId);
    void ParameterGroupCommandWorker(uint8_t groupId, CommandParamType command);
    /**
     * @brief RefreshParameterGroup 在重置参数组后重新获取该组参数
     * @param groupId
     * @param all   true: 所有组参数， false: 仅获取groupId组的参数
     */
    void RefreshParameterGroup(uint8_t groupId, bool all);


    /// Returns true if the specifed parameter exists
    ///     @param componentId: Component id or FactSystem::defaultComponentId
    ///     @param groupId group that parameter belong to
    ///     @param addrOffset: Parameter index in group
    bool parameterExists(int componentId, const uint8_t& groupId, const uint16_t& addrOffset);

    /// Returns true if the specifed parameter exists
    ///     @param name: Parameter name
    bool parameterExists(const QString& paramName);

    /// Returns all parameter names
    QStringList parameterNames(int componentId);
    QMap<uint8_t /* groupId */, QMap<uint16_t /* addrOffset */, Fact*>> getCompGroupId2FactMap() const {return _mapGroupId2FactMap;}
    /// Returns the specified Parameter. Returns a default empty fact is parameter does not exists. Also will pop
    /// a missing parameter error to user if parameter does not exist.
    ///     @param name: Parameter name
    Fact* getParameter(const QString& paramName);
    Fact* getParameter(const uint8_t& groupId, const uint16_t& addrOffset);
    /**
     * @brief getBatchQueueCount 获取需请求参数个数
     * @return
     */
    int32_t getBatchQueueCount();


    /// Returns error messages from loading
    QString readParametersFromStream(QTextStream& stream);

    void writeParametersToStream(QTextStream& stream);

    bool pendingWrites(void);

    Vehicle* vehicle(void) { return _vehicle; }
signals:
    void parametersReadyChanged     (bool parametersReady);
    void missingParametersChanged   (bool missingParameters);
    void loadProgressChanged        (float value);
    void pendingWritesChanged       (bool pendingWrites);
    void factAdded                  (Fact* fact);

private slots:
    void    _factRawValueUpdated                (const QVariant& rawValue);

private:
    void    _loadParameterMetaDataFromFile();
    void    _handleParamValue                   (uint8_t groupId, uint16_t addrOffset, FactMetaData::ValueType_t mavParamType, QVariant parameterValue);
    void    _factRawValueUpdateWorker           (int componentId, uint8_t groupId, uint8_t lenCfg, uint16_t addrOffset, FactMetaData::ValueType_t valueType, const QVariant& rawValue);
    void    _waitingParamTimeout                (void);
    void    _tryCacheLookup                     (void);
    void    _initialRequestTimeout              (void);
    void    _groupCommandTimeout                (void);
    int     _actualComponentId                  (int componentId);
    void    _readParameterRaw                   (uint8_t groupId, uint8_t lenCfg, uint16_t addrOffset);
    void    _sendParamSetToVehicle              (uint8_t groupId, uint8_t lenCfg, uint16_t addrOffset, FactMetaData::ValueType_t valueType, const QVariant& value);
    void    _writeLocalParamCache               (int vehicleId, int componentId);
    void    _loadMetaData                       (void);
    void    _clearMetaData                      (void);
    void    _loadOfflineEditingParams           (void);
    QString _logVehiclePrefix                   (int componentId);
    void    _setLoadProgress                    (double loadProgress);
    bool    _fillIndexBatchQueue                (bool waitingParamTimeout);
    void    _updateProgressBar                  (void);
    void    _checkInitialLoadComplete           (void);

    static QVariant _stringToTypedVariant(const QString& string, FactMetaData::ValueType_t type, bool failOk = false);

    Vehicle*            _vehicle;

    QMap<QString /* fact name */,  Fact*> _mapCompId2FactMap;
    QMap<uint8_t /* groupId */, QMap<uint16_t /* addrOffset */, Fact*>> _mapGroupId2FactMap;

    double      _loadProgress;                  ///< Parameter load progess, [0.0,1.0]
    bool        _parametersReady;               ///< true: parameter load complete
    bool        _missingParameters;             ///< true: parameter missing from initial load
    bool        _initialLoadComplete;           ///< true: Initial load of all parameters complete, whether successful or not
    bool        _waitingForDefaultComponent;    ///< true: last chance wait for default component params
    bool        _saveRequired;                  ///< true: _saveToEEPROM should be called
    bool        _metaDataAddedToFacts;          ///< true: FactMetaData has been adde to the default component facts
    bool        _logReplay;                     ///< true: running with log replay link

    typedef QPair<int /* FactMetaData::ValueType_t */, QVariant /* Fact::rawValue */> ParamTypeVal;
    typedef QMap<QString /* parameter name */, ParamTypeVal> CacheMapName2ParamTypeVal;

    // Wait counts from previous parameter update cycle
    int _prevWaitingReadParamIndexCount;
    int _prevWaitingWriteParamIndexCount;

    bool _readParamIndexProgressActive =    false;
    bool _readParamNameProgressActive =     false;
    bool _writeParamProgressActive =        false;

    static const int    _maxInitialRequestListRetry = 4;        ///< Maximum retries for request list
    int                 _initialRequestRetryCount;              ///< Current retry count for request list
    static const int    _maxGroupCommandRetry = 4;              ///< Maximum retries for group command list
    int                 _groupCommandRetryCount;                ///< Current retry count for group command list
    static const int    _maxInitialLoadRetrySingleParam = 5;    ///< Maximum retries for initial index based load of a single param
    static const int    _maxReadWriteRetry = 5;                 ///< Maximum retries read/write
    bool                _disableAllRetries;                     ///< true: Don't retry any requests (used for testing)

    bool        _indexBatchQueueActive; ///< true: we are actively batching re-requests for missing index base params, false: index based re-request has not yet started
    QMap<uint8_t, QList<int> >  _indexBatchQueue;                   ///< Key: Group id, Value: retry list }
    int _indexBatchQueueCount = 0;
    int _paramCountMap;             ///< Key: Component id, Value: count of parameters in this component

    ///Key: ComponentId, Value:{ Key: GroupId, Value: Map { Key: addrOffset, Value: retry count } }
    QMap<uint8_t, QMap<uint16_t, int> > _waitingReadParamIndexMap;
    QMap<uint8_t, QMap<uint16_t, int> > _waitingWriteParamIndexMap;
    /// Key: GroupId, Value: Map { Key: CommandParamType, Value: retry count }
    QMap<uint8_t, QMap<CommandParamType, int>> _waitingGroupCommandMap;


    QMap<int, QMap<QString, int> >  _waitingReadParamNameMap;   ///< Key: Component id, Value: Map { Key: groupId still waiting for, Value: retry count }
    QMap<int, QMap<QString, int> >  _waitingWriteParamNameMap;  ///< Key: Component id, Value: Map { Key: groupId name still waiting for, Value: retry count }
    QList<int>          _failedReadParamIndexMap;   ///< failed parameter index

    int _totalParamCount;                       ///< Number of parameters across all components
    int _waitingWriteParamBatchCount = 0;       ///< Number of parameters which are batched up waiting on write responses
    int _waitingReadParamNameBatchCount = 0;    ///< Number of parameters which are batched up waiting on read responses

    QTimer _initialRequestTimeoutTimer;
    QTimer _groupCommandTimeoutTimer;
    QTimer _waitingParamTimeoutTimer;
    ShenHangParameterMetaData* _shenHangMetaData = nullptr;
    Fact _defaultFact;   ///< Used to return default fact, when parameter not found
    CommandParamType lastCommandGroupType;
    uint8_t lastCommandGroupId;
    bool lastCommandGroupApplyToAll;
    static const char* _jsonParametersKey;
    static const char* _jsonCompIdKey;
    static const char* _jsonParamNameKey;
    static const char* _jsonParamValueKey;

};
