/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "ParameterManager.h"
#include "QGCApplication.h"
#include "QGCLoggingCategory.h"
#include "QGCApplication.h"
#include "FirmwarePlugin.h"
#include "JsonHelper.h"
//#include "CompInfoParam.h"

#include <QEasingCurve>
#include <QFile>
#include <QDebug>
#include <QVariantAnimation>
#include <QJsonArray>

QGC_LOGGING_CATEGORY(ParameterManagerVerbose1Log,           "ParameterManagerVerbose1Log")
QGC_LOGGING_CATEGORY(ParameterManagerVerbose2Log,           "ParameterManagerVerbose2Log")
QGC_LOGGING_CATEGORY(ParameterManagerDebugCacheFailureLog,  "ParameterManagerDebugCacheFailureLog") // Turn on to debug parameter cache crc misses


const char* ParameterManager::_jsonParametersKey =          "parameters";
const char* ParameterManager::_jsonCompIdKey =              "compId";
const char* ParameterManager::_jsonParamNameKey =           "name";
const char* ParameterManager::_jsonParamValueKey =          "value";

ParameterManager::ParameterManager(Vehicle* vehicle)
    : QObject                           (vehicle)
    , _vehicle                          (vehicle)
    , _loadProgress                     (0.0)
    , _parametersReady                  (false)
    , _missingParameters                (false)
    , _initialLoadComplete              (false)
    , _waitingForDefaultComponent       (false)
    , _saveRequired                     (false)
    , _metaDataAddedToFacts             (false)
    , _logReplay                        (!vehicle->vehicleLinkManager()->primaryLink().expired() && vehicle->vehicleLinkManager()->primaryLink().lock()->isLogReplay())
    , _prevWaitingReadParamIndexCount   (0)
    , _prevWaitingWriteParamIndexCount  (0)
    , _initialRequestRetryCount         (0)
    , _disableAllRetries                (false)
    , _indexBatchQueueActive            (false)
    , _totalParamCount                  (0)
{
    if (_vehicle->isOfflineEditingVehicle()) {
        _loadOfflineEditingParams();
        return;
    }

    _loadParameterMetaDataFromFile();

    _initialRequestTimeoutTimer.setSingleShot(true);
    _initialRequestTimeoutTimer.setInterval(5000);
    connect(&_initialRequestTimeoutTimer, &QTimer::timeout, this, &ParameterManager::_initialRequestTimeout);

    _groupCommandTimeoutTimer.setSingleShot(true);
    _groupCommandTimeoutTimer.setInterval(3000);
    connect(&_groupCommandTimeoutTimer, &QTimer::timeout, this, &ParameterManager::_groupCommandTimeout);

    _waitingParamTimeoutTimer.setSingleShot(true);
    _waitingParamTimeoutTimer.setInterval(3000);
    connect(&_waitingParamTimeoutTimer, &QTimer::timeout, this, &ParameterManager::_waitingParamTimeout);


    // Ensure the cache directory exists
    QFileInfo(QSettings().fileName()).dir().mkdir("ParamCache");
}

void ParameterManager::_updateProgressBar(void)
{
    int waitingReadParamIndexCount = 0;
    int waitingWriteParamCount = 0;


    for (int groupId: _waitingReadParamIndexMap.keys())
        waitingReadParamIndexCount += _waitingReadParamIndexMap[groupId].count();

    for (int groupId: _waitingWriteParamIndexMap.keys())
        waitingWriteParamCount += _waitingWriteParamIndexMap[groupId].count();

    if (waitingReadParamIndexCount == 0) {
        if (_readParamIndexProgressActive) {
            _readParamIndexProgressActive = false;
            _setLoadProgress(0.0);
            return;
        }
    } else {
        _readParamIndexProgressActive = true;
        _setLoadProgress((double)(_totalParamCount - waitingReadParamIndexCount) / (double)_totalParamCount);
        return;
    }

    if (waitingWriteParamCount == 0) {
        if (_writeParamProgressActive) {
            _writeParamProgressActive = false;
            _waitingWriteParamBatchCount = 0;
            _setLoadProgress(0.0);
            emit pendingWritesChanged(false);
            return;
        }
    } else {
        _writeParamProgressActive = true;
        _setLoadProgress((double)(qMax(_waitingWriteParamBatchCount - waitingWriteParamCount, 1)) / (double)(_waitingWriteParamBatchCount + 1));
        emit pendingWritesChanged(true);
        return;
    }

//    if (waitingReadParamNameCount == 0) {
//        if (_readParamNameProgressActive) {
//            _readParamNameProgressActive = false;
//            _waitingReadParamNameBatchCount = 0;
//            _setLoadProgress(0.0);
//            return;
//        }
//    } else {
//        _readParamNameProgressActive = true;
//        _setLoadProgress((double)(qMax(_waitingReadParamNameBatchCount - waitingReadParamNameCount, 1)) / (double)(_waitingReadParamNameBatchCount + 1));
//        return;
//    }
}

void ParameterManager::shenHangMessageReceived(ShenHangProtocolMessage message)
{
    if (message.tyMsg0 == ACK_COMMAND_PARAM)
    {
        ParamUnion paramUnion;
        uint8_t groupId = 0;

        uint8_t lenCfg;
        uint16_t addrOffset = 0;
        uint8_t data[16] = {};
        QVariant dataVariant;
        FactMetaData* factMetaData;
        uint8_t execuateState;
        uint8_t dgnCode;
        uint8_t errorTyMsg1;
        switch (message.tyMsg1)
        {
        case ACK_RESET_PARAM:
            groupId = message.payload[0];
            execuateState = message.payload[1];
            if(_waitingGroupCommandMap.contains(groupId)
                    && _waitingGroupCommandMap[groupId].contains(COMMAND_RESET_PARAM)) {
                _waitingGroupCommandMap[groupId].remove(COMMAND_RESET_PARAM);
                if (_waitingGroupCommandMap[groupId].size() == 0) {
                    _waitingGroupCommandMap.remove(groupId);
                }
            }
            if (execuateState != 0) {
                qgcApp()->showAppMessage(QString("Rest groupId: %1 parameter failed!").arg(groupId));
            } else {    // 重置成功
                refreshGroupParameters(groupId);
            }

            break;
        case ACK_LOAD_PARAM:
            groupId = message.payload[0];
            execuateState = message.payload[1];
            if(_waitingGroupCommandMap.contains(groupId)
                    && _waitingGroupCommandMap[groupId].contains(COMMAND_LOAD_PARAM)) {
                _waitingGroupCommandMap[groupId].remove(COMMAND_LOAD_PARAM);
                if (_waitingGroupCommandMap[groupId].size() == 0) {
                    _waitingGroupCommandMap.remove(groupId);
                }
            }
            if (execuateState != 0) {
                qgcApp()->showAppMessage(QString("Load groupId: %1 parameter failed!").arg(groupId));
            }

            break;
        case ACK_SAVE_PARAM:
            groupId = message.payload[0];
            execuateState = message.payload[1];
            if(_waitingGroupCommandMap.contains(groupId)
                    && _waitingGroupCommandMap[groupId].contains(COMMAND_SAVE_PARAM)) {
                _waitingGroupCommandMap[groupId].remove(COMMAND_SAVE_PARAM);
                if (_waitingGroupCommandMap[groupId].size() == 0) {
                    _waitingGroupCommandMap.remove(groupId);
                }
                if (execuateState != 0) {
                    qgcApp()->showAppMessage(QString("Save groupId: %1 parameter failed!").arg(groupId));
                }
            }

            break;
        case ACK_QUERY_PARAM:
        case ACK_SET_PARAM:
            groupId = message.payload[0];
            lenCfg = message.payload[1];
            memcpy(&addrOffset, message.payload + 2, 2);
            memcpy(data, message.payload + 4, 16);
            memcpy(&paramUnion.paramFloat, data, lenCfg);
            factMetaData = _shenHangMetaData->getMetaDataForFact(groupId, addrOffset);//factMetaDataByIndexes(groupId, addrOffset);
            paramUnion.type = factMetaData->type();
            qCDebug(ParameterManagerLog) << QString("handle parameter groupId:%1, addrOffset:%2").arg(groupId).arg(addrOffset);
            switch (paramUnion.type) {
            case FactMetaData::valueTypeFloat:
                dataVariant.setValue(paramUnion.paramFloat);
                break;
            case FactMetaData::valueTypeInt16:
                dataVariant.setValue(paramUnion.paramInt16);
                break;
            case FactMetaData::valueTypeUint8:
                dataVariant.setValue(paramUnion.paramUint8);
                break;
            default:
                break;
            }
            break;

        case ACK_ERROR_PARAM:
            errorTyMsg1 = message.payload[0];
            dgnCode = message.payload[1];
            switch (dgnCode) {      //2：编号超界;4：无效指令
            case 2:
                qgcApp()->showAppMessage(QString("Error msg: %1 access out of border!").arg(errorTyMsg1));
                break;
            case 4:
                qgcApp()->showAppMessage(QString("Invalid parameter command: %1!").arg(errorTyMsg1));
                break;
            default:
                break;
            }
            break;
        default:
            break;
        }

        _handleParamValue(groupId, addrOffset, paramUnion.type, dataVariant);
        if (_waitingGroupCommandMap.keys().count() == 0
                && _groupCommandTimeoutTimer.isActive())
            _groupCommandTimeoutTimer.stop();
    }
}

/// Called whenever a parameter is updated or first seen.
void ParameterManager::_handleParamValue(uint8_t groupId, uint16_t addrOffset, FactMetaData::ValueType_t mavParamType, QVariant parameterValue)
{

    qCDebug(ParameterManagerVerbose1Log) <<  "_handleParamValue"
                                         <<   "groupId:" << groupId
                                         <<   "addrOffset:" << addrOffset
                                         <<   "mavType:" << mavParamType
                                         <<   "value:" << parameterValue << ")";
    _initialRequestTimeoutTimer.stop();
    _waitingParamTimeoutTimer.stop();

    // Update our total parameter counts
    _paramCountMap = _shenHangMetaData->getFactMetaDataCount();
    _totalParamCount = _paramCountMap;

    // If we've never seen this component id before, setup the index wait lists.
    if (_waitingReadParamIndexMap.size() == 0) {
        // Add all indices to the wait list, parameter index is 0-based
//        for (int waitingIndex=0; waitingIndex<_paramCountMap; waitingIndex++) {
//            // This will add the new component id, as well as the the new waiting index and set the retry count for that index to 0
//            _waitingReadParamIndexMap[waitingIndex] = 0;
//        }
        QMap<uint8_t, MetaDataGroup> mapMetaGroups = _shenHangMetaData->getMapMetaDataGroup();
        for(uint8_t groupId: mapMetaGroups.keys()) {
            for(uint16_t addrOffset: mapMetaGroups[groupId].mapMetaData.keys()) {
                _waitingReadParamIndexMap[groupId][addrOffset] = 0;
            }
        }
    }

    // Remove this parameter from the waiting lists
    if (_waitingReadParamIndexMap.contains(groupId)) {
        if (_waitingReadParamIndexMap[groupId].contains(addrOffset)) {
            _waitingReadParamIndexMap[groupId].remove(addrOffset);
        }
        if (_waitingReadParamIndexMap[groupId].size() == 0) {
            _waitingReadParamIndexMap.remove(groupId);
        }
        if (_indexBatchQueue.contains(groupId) && _indexBatchQueue[groupId].contains(addrOffset)) {
            _indexBatchQueue[groupId].removeOne(addrOffset);
            if (_indexBatchQueue[groupId].size() == 0) {
                _indexBatchQueue.remove(groupId);
            }
        }
        _fillIndexBatchQueue(false /* waitingParamTimeout */);
    }

    if (_waitingWriteParamIndexMap.contains(groupId) &&
            _waitingWriteParamIndexMap[groupId].contains(addrOffset)) {
        _waitingWriteParamIndexMap[groupId].remove(addrOffset);
        if (_waitingWriteParamIndexMap[groupId].size() == 0)
            _waitingWriteParamIndexMap.remove(groupId);
    }
//    _waitingWriteParamNameMap.remove(parameterName);
//    if (_waitingReadParamIndexMap.count()) {
//        qCDebug(ParameterManagerVerbose2Log) << _logVehiclePrefix(componentId) << "_waitingReadParamIndexMap:" << _waitingReadParamIndexMap;
//    }
//    if (_waitingReadParamNameMap.count()) {
//        qCDebug(ParameterManagerVerbose2Log) << _logVehiclePrefix(componentId) << "_waitingReadParamNameMap" << _waitingReadParamNameMap;
//    }
//    if (_waitingWriteParamNameMap.count()) {
//        qCDebug(ParameterManagerVerbose2Log) << _logVehiclePrefix(componentId) << "_waitingWriteParamNameMap" << _waitingWriteParamNameMap;
//    }

    // Track how many parameters we are still waiting for

    int waitingReadParamIndexCount = 0;
    int waitingWriteParamIndexCount = 0;
    for (int waitingGroupId: _waitingReadParamIndexMap.keys()) {
        waitingReadParamIndexCount += _waitingReadParamIndexMap[waitingGroupId].count();
    }

    if (waitingReadParamIndexCount) {
        qCDebug(ParameterManagerVerbose1Log)  << "waitingReadParamIndexCount:" << waitingReadParamIndexCount;
    }

//    for(int waitingComponentId: _waitingReadParamNameMap.keys()) {
//        waitingReadParamNameCount += _waitingReadParamNameMap.count();
//    }
//    if (waitingReadParamNameCount) {
//        qCDebug(ParameterManagerVerbose1Log) << _logVehiclePrefix(componentId) << "waitingReadParamNameCount:" << waitingReadParamNameCount;
//    }

    for(int waitingGroupId: _waitingWriteParamIndexMap.keys()) {
        waitingWriteParamIndexCount += _waitingWriteParamIndexMap[waitingGroupId].count();
    }
    if (waitingWriteParamIndexCount) {
        qCDebug(ParameterManagerVerbose1Log) << "waitingWriteParamNameCount:" << waitingWriteParamIndexCount;
    }

    int totalWaitingParamCount = waitingReadParamIndexCount + waitingWriteParamIndexCount;
    if (totalWaitingParamCount) {
        // More params to wait for, restart timer
        _waitingParamTimeoutTimer.start();
        qCDebug(ParameterManagerVerbose1Log) << _logVehiclePrefix(-1) << "Restarting _waitingParamTimeoutTimer: totalWaitingParamCount:" << totalWaitingParamCount;
    } else {
        if (!_mapGroupId2FactMap.contains(_vehicle->defaultComponentId())) {
            // Still waiting for parameters from default component
            qCDebug(ParameterManagerLog) << _logVehiclePrefix(-1) << "Restarting _waitingParamTimeoutTimer (still waiting for default component params)";
            _waitingParamTimeoutTimer.start();
        } else {
            qCDebug(ParameterManagerVerbose1Log) << _logVehiclePrefix(-1) << "Not restarting _waitingParamTimeoutTimer (all requests satisfied)";
        }
    }
\
    _updateProgressBar();
    if (addrOffset != 0) {  // addrOffset=0时，是参数组命令，不能用于创建fact
        Fact* fact = nullptr;
        if (_mapGroupId2FactMap.contains(groupId) &&
                _mapGroupId2FactMap[groupId].contains(addrOffset)) {
            fact = _mapGroupId2FactMap[groupId][addrOffset];
        } else {
            FactMetaData* factMetaData = _shenHangMetaData->getMetaDataForFact(groupId, addrOffset);
            fact = new Fact(groupId, addrOffset, factMetaData->type(), factMetaData->name(), this);
            fact->setMetaData(factMetaData);
            qCDebug(ParameterManagerVerbose1Log) << "Adding new fact groupId:" << groupId << "addrOffset:" << addrOffset
                                                 << "type:" << factMetaData->type() << "name:" << factMetaData->name();
            _mapGroupId2FactMap[groupId][addrOffset] = fact;

            // We need to know when the fact value changes so we can update the vehicle
            connect(fact, &Fact::_containerRawValueChanged, this, &ParameterManager::_factRawValueUpdated);

            emit factAdded(fact);
        }

        fact->_containerSetRawValue(parameterValue);
    }
    _prevWaitingReadParamIndexCount = waitingReadParamIndexCount;
    _prevWaitingWriteParamIndexCount = waitingWriteParamIndexCount;

    _checkInitialLoadComplete();

    qCDebug(ParameterManagerVerbose1Log) << "_initialLoadComplete:" << _initialLoadComplete << "waitingReadParamIndexCount:" << waitingReadParamIndexCount;
    qCDebug(ParameterManagerVerbose1Log) << "_parameterUpdate complete";
}

/// Writes the parameter update to mavlink, sets up for write wait
void ParameterManager::_factRawValueUpdateWorker(int componentId, uint8_t groupId, uint8_t lenCfg, uint16_t addrOffset, FactMetaData::ValueType_t valueType, const QVariant& rawValue)
{
    if (_waitingWriteParamIndexMap.contains(componentId)) {
        if ( _waitingWriteParamIndexMap.contains(groupId)
             && _waitingWriteParamIndexMap[groupId].contains(addrOffset)) {
            _waitingWriteParamIndexMap[groupId].remove(addrOffset);
            if (_waitingWriteParamIndexMap[groupId].size() == 0)
                _waitingWriteParamIndexMap.remove(groupId);
        } else {
            _waitingWriteParamBatchCount++;
        }

        _waitingWriteParamIndexMap[groupId][addrOffset] = 0; // Add new entry and set retry count
        _updateProgressBar();
        _waitingParamTimeoutTimer.start();
        _saveRequired = true;

    } else {
        qWarning() << "Internal error ParameterManager::_factValueUpdateWorker: component id not found" << componentId;
    }
    _sendParamSetToVehicle(groupId, lenCfg,  addrOffset, valueType, rawValue);


    qCDebug(ParameterManagerLog) << _logVehiclePrefix(componentId) << "Update parameter (_waitingParamTimeoutTimer started) - compId:name:rawValue" << componentId << groupId << rawValue;
}

void ParameterManager::_factRawValueUpdated(const QVariant& rawValue)
{
    Fact* fact = qobject_cast<Fact*>(sender());
    if (!fact) {
        qWarning() << "Internal error";
        return;
    }

    _factRawValueUpdateWorker(fact->componentId(), fact->groupId(), fact->metaData()->getTypeSize(), fact->addrOffset(), fact->type(), rawValue);
}

void ParameterManager::_loadParameterMetaDataFromFile()
{
    if (!_shenHangMetaData) {
        FirmwarePlugin* plugin = _vehicle->firmwarePlugin();
        QString metaDataFile = plugin->_internalParameterMetaDataFile(_vehicle);
        _shenHangMetaData = qobject_cast<ShenHangParameterMetaData*>(plugin->_loadParameterMetaData(metaDataFile));
    }
}

void ParameterManager::refreshGroupParameters(uint8_t groupId)
{
    WeakLinkInterfacePtr weakLink = _vehicle->vehicleLinkManager()->primaryLink();

    if (weakLink.expired()) {
        return;
    }

    if (weakLink.lock()->linkConfiguration()->isHighLatency() || _logReplay) {
        // These links don't load params
        _parametersReady = true;
        _missingParameters = true;
        _initialLoadComplete = true;
        _waitingForDefaultComponent = false;
        emit parametersReadyChanged(_parametersReady);
        emit missingParametersChanged(_missingParameters);
    }

    if (!_initialLoadComplete) {
        _initialRequestTimeoutTimer.start();
    }
    // Reset index wait lists
    QMap<uint8_t, MetaDataGroup> shenHangMetaDataGroups = _shenHangMetaData->getMapMetaDataGroup();
//    _paramCountMap = shenHangParameterMetaData->getFactMetaDataCount();

    for (uint8_t waitingGroupId: shenHangMetaDataGroups.keys()) {
        // 若请求所有参数组或与需要请求的参数组相同则加入待读
        if (groupId == 0xff || waitingGroupId == groupId) {
            for (uint16_t waitingIndex: shenHangMetaDataGroups[waitingGroupId].mapMetaData.keys()) {
                // This will add a new waiting index if needed and set the retry count for that index to 0
                _waitingReadParamIndexMap[waitingGroupId][waitingIndex] = 0;
            }
        }
    }
    _waitingParamTimeoutTimer.start();
    _waitingForDefaultComponent = true;
    ShenHangProtocolMessage msg;
    SharedLinkInterfacePtr  sharedLink = weakLink.lock();
    uint8_t data[16] = {};
    if (groupId == 0xff) {  // 查询所有组参数
        _indexBatchQueueActive = false;
        _initialLoadComplete = false;
        _vehicle->PackSetParamCommand(msg, CommandParamType::COMMAND_QUERY_PARAM, 0XFF, 0, 0, data, 16);
        _vehicle->sendShenHangMessageOnLinkThreadSafe(sharedLink.get(), msg);
    } else {
        _fillIndexBatchQueue(false /* waitingParamTimeout */);
    }
}

/// Translates FactSystem::defaultComponentId to real component id if needed
int ParameterManager::_actualComponentId(int componentId)
{
    if (componentId == FactSystem::defaultComponentId) {
        componentId = _vehicle->defaultComponentId();
        if (componentId == FactSystem::defaultComponentId) {
            qWarning() << _logVehiclePrefix(-1) << "Default component id not set";
        }
    }

    return componentId;
}

//void ParameterManager::refreshParametersPrefix(int componentId, const QString& namePrefix)
//{
//    componentId = _actualComponentId(componentId);
//    qCDebug(ParameterManagerLog) << _logVehiclePrefix(componentId) << "refreshParametersPrefix - name:" << namePrefix << ")";

//    for (const QString &paramName: _mapCompGroupId2FactMap.keys()) {
//        if (paramName.startsWith(namePrefix)) {
//            refreshParameter(componentId, paramName);
//        }
//    }
//}

bool ParameterManager::parameterExists(int componentId, const uint8_t& groupId, const uint16_t& addrOffset)
{
    bool ret = false;

    componentId = _actualComponentId(componentId);
    if (_mapGroupId2FactMap.contains(componentId) && _mapGroupId2FactMap.contains(groupId)
            &&_mapGroupId2FactMap[groupId].contains(addrOffset)) {
        ret = true; //_mapCompGroupId2FactMap.contains(_remapParamNameToVersion(groupId));
    }

    return ret;
}

bool ParameterManager::parameterExists(const QString& paramName)
{
    bool ret = false;
    ret = _mapCompId2FactMap.contains(paramName);

    return ret;
}

Fact* ParameterManager::getParameter(const QString& paramName)
{
    if (!_mapCompId2FactMap.contains(paramName)) {
        qgcApp()->reportMissingParameter(paramName);
        return &_defaultFact;
    }

    return _mapCompId2FactMap[paramName];
}

Fact* ParameterManager::getParameter(const uint8_t& groupId, const uint16_t& addrOffset)
{

    QString mappedParamName = QString("groupId:%1, addrOffset:%2").arg(groupId).arg(addrOffset);//_remapParamNameToVersion(groupId);
    if (!_mapGroupId2FactMap.contains(groupId) ||
            !_mapGroupId2FactMap[groupId].contains(addrOffset)) {
        qgcApp()->reportMissingParameter(mappedParamName);
        return &_defaultFact;
    }

    return _mapGroupId2FactMap[groupId][addrOffset];
}

int32_t ParameterManager::getBatchQueueCount()
{
    int32_t count = 0;
    for(uint8_t groupId: _indexBatchQueue.keys()) {
        count += _indexBatchQueue[groupId].count();
    }
    return count;
}

QStringList ParameterManager::parameterNames(int componentId)
{
    QStringList names;

//    for(const QString &paramName: _mapCompGroupId2FactMap[_actualComponentId(componentId)].keys()) {
//        names << paramName;
//    }

    return names;
}

/// Requests missing index based parameters from the vehicle.
///     @param waitingParamTimeout: true: being called due to timeout, false: being called to re-fill the batch queue
/// return true: Parameters were requested, false: No more requests needed
bool ParameterManager::_fillIndexBatchQueue(bool waitingParamTimeout)
{
    if (!_indexBatchQueueActive) {
        return false;
    }

    const int maxBatchSize = 10;

    if (waitingParamTimeout) {
        // We timed out, clear the queue and try again
        qCDebug(ParameterManagerLog) << "Refilling index based batch queue due to timeout";
        _indexBatchQueue.clear();
    } else {
        qCDebug(ParameterManagerLog) << "Refilling index based batch queue due to received parameter";
    }


    for(uint8_t groupId: _waitingReadParamIndexMap.keys()) {
        for (uint16_t addrOffset: _waitingReadParamIndexMap[groupId].keys()) {
            if (_indexBatchQueue.contains(groupId) && _indexBatchQueue[groupId].contains(addrOffset)) {
                // Don't add more than once
                continue;
            }

            if (getBatchQueueCount() >= maxBatchSize) {
                break;
            }

            _waitingReadParamIndexMap[groupId][addrOffset]++;   // Bump retry count

            if (_disableAllRetries || _waitingReadParamIndexMap[groupId][addrOffset] > _maxInitialLoadRetrySingleParam) {
                // Give up on this index
                _failedReadParamIndexMap << groupId;
                qCDebug(ParameterManagerLog) << "Giving up on (paramIndex:" << groupId << "retryCount:" << _waitingReadParamIndexMap[groupId] << ")";
                _waitingReadParamIndexMap[groupId].remove(addrOffset);
                if(_waitingReadParamIndexMap[groupId].size() == 0)
                    _waitingReadParamIndexMap.remove(groupId);
            } else {
                // Retry again
                _indexBatchQueue[groupId].append(addrOffset);
                FactMetaData* factMetaData = _shenHangMetaData->getMetaDataForFact(groupId, addrOffset);
                if (!factMetaData) {
                    qDebug() << "fill index batch queue !factMetaData " << groupId << addrOffset;
                    continue;
                }
                _readParameterRaw(groupId, factMetaData->getTypeSize(), addrOffset);
                qCDebug(ParameterManagerLog) << "Read re-request for (groupId:" << groupId << "retryCount:" << _waitingReadParamIndexMap[groupId] << ")";
            }
        }
    }

    return _indexBatchQueue.count() != 0;
}

void ParameterManager::_waitingParamTimeout(void)
{
    if (_logReplay) {
        return;
    }

    bool paramsRequested = false;
    const int maxBatchSize = 10;
    int batchCount = 0;

    qCDebug(ParameterManagerLog) << _logVehiclePrefix(-1) << "_waitingParamTimeout";

    // Now that we have timed out for possibly the first time we can activate the index batch queue
    _indexBatchQueueActive = true;

    // First check for any missing parameters from the initial index based load
    paramsRequested = _fillIndexBatchQueue(true /* waitingParamTimeout */);

    if (!paramsRequested && !_waitingForDefaultComponent && !_mapGroupId2FactMap.contains(_vehicle->defaultComponentId())) {
        // Initial load is complete but we still don't have any default component params. Wait one more cycle to see if the
        // any show up.
        qCDebug(ParameterManagerLog) << _logVehiclePrefix(-1) << "Restarting _waitingParamTimeoutTimer - still don't have default component params" << _vehicle->defaultComponentId();
        _waitingParamTimeoutTimer.start();
        _waitingForDefaultComponent = true;
        return;
    }
    _waitingForDefaultComponent = false;

    _checkInitialLoadComplete();

    if (!paramsRequested) {
        for(const uint8_t &groupId: _waitingWriteParamIndexMap.keys()) {
            for (const uint16_t& addrOffset: _waitingWriteParamIndexMap[groupId].keys()) {
                paramsRequested = true;
                _waitingWriteParamIndexMap[groupId][addrOffset]++;   // Bump retry count
                if (_waitingWriteParamIndexMap[groupId][addrOffset] <= _maxReadWriteRetry) {
                    if (addrOffset == 0) {//参数组命令
                        ParameterGroupCommandWorker(groupId, lastCommandGroupType);
                    } else {
                        Fact* fact = getParameter(groupId, addrOffset);
                        if (!fact) {
                            qDebug() << "!fact " << groupId << addrOffset;
                            continue;
                        }
                        _sendParamSetToVehicle(groupId, fact->metaData()->getTypeSize(), addrOffset, fact->type(), fact->rawValue());
                        qCDebug(ParameterManagerLog) << "Write resend for (groupId:" << groupId << "retryCount:" << _waitingWriteParamIndexMap[groupId][addrOffset] << ")";
                    }
                    if (++batchCount > maxBatchSize) {
                        goto Out;
                    }
                } else {
                    // Exceeded max retry count, notify user
                    _waitingWriteParamIndexMap[groupId].remove(addrOffset);
                    if (_waitingWriteParamIndexMap[groupId].size() == 0)
                        _waitingWriteParamIndexMap.remove(groupId);
                    QString errorMsg = tr("Parameter write failed: veh:%1 groupId:%2 addrOffset: %3")
                            .arg(_vehicle->id()).arg(groupId).arg(addrOffset);
                    qCDebug(ParameterManagerLog) << errorMsg;
                    qgcApp()->showAppMessage(errorMsg);
                }
            }
        }

    }

    if (!paramsRequested) {
        for(const uint8_t &groupId: _waitingReadParamIndexMap.keys()) {
            for (const uint16_t &addrOffset: _waitingReadParamIndexMap[groupId].keys()) {
                paramsRequested = true;
                _waitingReadParamIndexMap[groupId][addrOffset]++;   // Bump retry count
                if (_waitingReadParamIndexMap[groupId][addrOffset] <= _maxReadWriteRetry) {
                    FactMetaData* factMetaData = _shenHangMetaData->getMetaDataForFact(groupId, addrOffset);
                    if (!factMetaData) {
                        qDebug() << "waiting param timeout !factMetaData " << groupId << addrOffset;
                        continue;
                    }
                    _readParameterRaw(groupId, factMetaData->getTypeSize(), addrOffset);
                    qCDebug(ParameterManagerLog) << "Read re-request for (groupId:" << groupId
                                                 << "addrOffset:" << addrOffset <<  "retryCount:"
                                                 << _waitingReadParamIndexMap[groupId] << ")";
                    if (++batchCount > maxBatchSize) {
                        goto Out;
                    }
                } else {
                    // Exceeded max retry count, notify user
                    _waitingReadParamIndexMap[groupId].remove(addrOffset);
                    if (_waitingReadParamIndexMap[groupId].size() == 0)
                        _waitingReadParamIndexMap.remove(groupId);
                    QString errorMsg = tr("Parameter read failed: veh:%1 group:%2 addrOffset:%3")
                            .arg(_vehicle->id()).arg(groupId).arg(addrOffset);
                    qCDebug(ParameterManagerLog) << errorMsg;
                    qgcApp()->showAppMessage(errorMsg);
                }
            }
        }
    }

Out:
    if (paramsRequested) {
        qCDebug(ParameterManagerLog) << _logVehiclePrefix(-1) << "Restarting _waitingParamTimeoutTimer - re-request";
        _waitingParamTimeoutTimer.start();
    }
}

void ParameterManager::_readParameterRaw(uint8_t groupId, uint8_t lenCfg, uint16_t addrOffset)
{
    WeakLinkInterfacePtr weakLink = _vehicle->vehicleLinkManager()->primaryLink();
    if (!weakLink.expired()) {
        ShenHangProtocolMessage message;
        message.tyMsg0 = MsgId::COMMAND_PARAM;
        message.tyMsg1 = CommandParamType::COMMAND_QUERY_PARAM;
        message.payload[0] = groupId;
        message.payload[1] = lenCfg;
        memcpy(message.payload + 2, &addrOffset, 2);
        SharedLinkInterfacePtr  sharedLink = weakLink.lock();

        _vehicle->sendShenHangMessageOnLinkThreadSafe(sharedLink.get(), message);
    }
}

void ParameterManager::_sendParamSetToVehicle(uint8_t groupId, uint8_t lenCfg, uint16_t addrOffset, FactMetaData::ValueType_t valueType, const QVariant& value)
{
    WeakLinkInterfacePtr weakLink = _vehicle->vehicleLinkManager()->primaryLink();

    if (!weakLink.expired()) {
        mavlink_param_union_t   union_value;
        SharedLinkInterfacePtr  sharedLink = weakLink.lock();
        ShenHangProtocolMessage message;
        message.tyMsg0 = MsgId::COMMAND_PARAM;
        message.tyMsg1 = CommandParamType::COMMAND_SET_PARAM;
        message.payload[0] = groupId;
        message.payload[1] = lenCfg;
        memcpy(message.payload + 2, &addrOffset, 2);
        memcpy(message.payload + 4, value.data(), lenCfg);
        switch (valueType) {
        case FactMetaData::valueTypeUint8:
            union_value.param_uint8 = (uint8_t)value.toUInt();
            break;

        case FactMetaData::valueTypeInt8:
            union_value.param_int8 = (int8_t)value.toInt();
            break;

        case FactMetaData::valueTypeUint16:
            union_value.param_uint16 = (uint16_t)value.toUInt();
            break;

        case FactMetaData::valueTypeInt16:
            union_value.param_int16 = (int16_t)value.toInt();
            break;

        case FactMetaData::valueTypeUint32:
            union_value.param_uint32 = (uint32_t)value.toUInt();
            break;

        case FactMetaData::valueTypeFloat:
            union_value.param_float = value.toFloat();
            break;

        default:
            qCritical() << "Unsupported fact falue type" << valueType;
            // fall through

        case FactMetaData::valueTypeInt32:
            union_value.param_int32 = (int32_t)value.toInt();
            break;
        }

        _vehicle->sendShenHangMessageOnLinkThreadSafe(sharedLink.get(), message);
    }
}

void ParameterManager::_writeLocalParamCache(int vehicleId, int componentId)
{
//    CacheMapName2ParamTypeVal cacheMap;

//    for (const QString& paramName: _mapCompGroupId2FactMap.keys()) {
//        const Fact *fact = _mapCompGroupId2FactMap[paramName];
//        cacheMap[paramName] = ParamTypeVal(fact->type(), fact->rawValue());
//    }

//    QFile cacheFile(parameterCacheFile(vehicleId, componentId));
//    cacheFile.open(QIODevice::WriteOnly | QIODevice::Truncate);

//    QDataStream ds(&cacheFile);
//    ds << cacheMap;
}

QDir ParameterManager::parameterCacheDir()
{
    const QString spath(QFileInfo(QSettings().fileName()).dir().absolutePath());
    return spath + QDir::separator() + "ParamCache";
}

QString ParameterManager::parameterCacheFile(int vehicleId, int componentId)
{
    return parameterCacheDir().filePath(QString("%1_%2.v2").arg(vehicleId).arg(componentId));
}

QString ParameterManager::readParametersFromStream(QTextStream& stream)
{
//    QString missingErrors;
//    QString typeErrors;

//    while (!stream.atEnd()) {
//        QString line = stream.readLine();
//        if (!line.startsWith("#")) {
//            QStringList wpParams = line.split("\t");
//            int lineMavId = wpParams.at(0).toInt();
//            if (wpParams.size() == 5) {
//                if (_vehicle->id() != lineMavId) {
//                    return QString("The parameters in the stream have been saved from System Id %1, but the current vehicle has the System Id %2.").arg(lineMavId).arg(_vehicle->id());
//                }

//                int     componentId = wpParams.at(1).toInt();
//                QString paramName = wpParams.at(2);
//                QString valStr = wpParams.at(3);
//                uint    mavType = wpParams.at(4).toUInt();

//                if (!parameterExists(componentId, paramName)) {
//                    QString error;
//                    error += QStringLiteral("%1:%2 ").arg(componentId).arg(paramName);
//                    missingErrors += error;
//                    qCDebug(ParameterManagerLog) << QStringLiteral("Skipped due to missing: %1").arg(error);
//                    continue;
//                }

//                Fact* fact = getParameter(componentId, paramName);
//                if (fact->type() != mavTypeToFactType((MAV_PARAM_TYPE)mavType)) {
//                    QString error;
//                    error  = QStringLiteral("%1:%2 ").arg(componentId).arg(paramName);
//                    typeErrors += error;
//                    qCDebug(ParameterManagerLog) << QStringLiteral("Skipped due to type mismatch: %1").arg(error);
//                    continue;
//                }

//                qCDebug(ParameterManagerLog) << "Updating parameter" << componentId << paramName << valStr;
//                fact->setRawValue(valStr);
//            }
//        }
//    }

    QString errors;

//    if (!missingErrors.isEmpty()) {
//        errors = tr("Parameters not loaded since they are not currently on the vehicle: %1\n").arg(missingErrors);
//    }

//    if (!typeErrors.isEmpty()) {
//        errors += tr("Parameters not loaded due to type mismatch: %1").arg(typeErrors);
//    }

    return errors;
}

void ParameterManager::writeParametersToStream(QTextStream& stream)
{
//    stream << "# Onboard parameters for Vehicle " << _vehicle->id() << "\n";
//    stream << "#\n";

//    stream << "# Stack: " << _vehicle->firmwareTypeString() << "\n";
//    stream << "# Vehicle: " << _vehicle->vehicleTypeString() << "\n";
//    stream << "# Version: "
//           << _vehicle->firmwareMajorVersion() << "."
//           << _vehicle->firmwareMinorVersion() << "."
//           << _vehicle->firmwarePatchVersion() << " "
//           << _vehicle->firmwareVersionTypeString() << "\n";
//    stream << "# Git Revision: " << _vehicle->gitHash() << "\n";

//    stream << "#\n";
//    stream << "# Vehicle-Id Component-Id Name Value Type\n";

//    for (int componentId: _mapCompGroupId2FactMap.keys()) {
//        for (const QString &paramName: _mapCompGroupId2FactMap.keys()) {
//            Fact* fact = _mapCompGroupId2FactMap[paramName];
//            if (fact) {
//                stream << _vehicle->id() << "\t" << componentId << "\t" << paramName << "\t" << fact->rawValueStringFullPrecision() << "\t" << QString("%1").arg(factTypeToMavType(fact->type())) << "\n";
//            } else {
//                qWarning() << "Internal error: missing fact";
//            }
//        }
//    }

//    stream.flush();
}

void ParameterManager::_checkInitialLoadComplete(void)
{
    // Already processed?
    if (_initialLoadComplete) {
        return;
    }

    if (_waitingReadParamIndexMap.count()) {
        // We are still waiting on some parameters, not done yet
        return;
    }

//    if (!_mapGroupId2FactMap.contains(_vehicle->defaultComponentId())) {
//        // No default component params yet, not done yet
//        return;
//    }

    // We aren't waiting for any more initial parameter updates, initial parameter loading is complete
    _initialLoadComplete = true;

    qCDebug(ParameterManagerLog) << _logVehiclePrefix(-1) << "Initial load complete";

    // Check for index based load failures
    QString indexList;
    bool initialLoadFailures = false;
    for (int paramIndex: _failedReadParamIndexMap) {
        if (initialLoadFailures) {
            indexList += ", ";
        }
        indexList += QString("%1").arg(paramIndex);
        initialLoadFailures = true;
        qCDebug(ParameterManagerLog) << "Gave up on initial load after max retries (paramIndex:" << paramIndex << ")";
    }

    _missingParameters = false;
    if (initialLoadFailures) {
        _missingParameters = true;
        QString errorMsg = tr("%1 was unable to retrieve the full set of parameters from vehicle %2. "
                              "This will cause %1 to be unable to display its full user interface. "
                              "If you are using modified firmware, you may need to resolve any vehicle startup errors to resolve the issue. "
                              "If you are using standard firmware, you may need to upgrade to a newer version to resolve the issue.").arg(qgcApp()->applicationName()).arg(_vehicle->id());
        qCDebug(ParameterManagerLog) << errorMsg;
        qgcApp()->showAppMessage(errorMsg);
        if (!qgcApp()->runningUnitTests()) {
            qCWarning(ParameterManagerLog) << _logVehiclePrefix(-1) << "The following parameter indices could not be loaded after the maximum number of retries: " << indexList;
        }
    }

    // Signal load complete
    _parametersReady = true;
    _vehicle->autopilotPlugin()->parametersReadyPreChecks();
    emit parametersReadyChanged(true);
    emit missingParametersChanged(_missingParameters);
}

void ParameterManager::_initialRequestTimeout(void)
{
    if (!_disableAllRetries && ++_initialRequestRetryCount <= _maxInitialRequestListRetry) {
        qCDebug(ParameterManagerLog) << _logVehiclePrefix(-1) << "Retrying initial parameter request list";
        refreshGroupParameters(0xff);
        _initialRequestTimeoutTimer.start();
    } else {
        if (!_vehicle->genericFirmware()) {
            QString errorMsg = tr("Vehicle %1 did not respond to request for parameters. "
                                  "This will cause %2 to be unable to display its full user interface.").arg(_vehicle->id()).arg(qgcApp()->applicationName());
            qCDebug(ParameterManagerLog) << errorMsg;
            qgcApp()->showAppMessage(errorMsg);
        }
    }
}

void ParameterManager::_groupCommandTimeout()
{
    for (int cid: _waitingGroupCommandMap.keys()) {
        // Add/Update all indices to the wait list, parameter index is 0-based
        for (uint8_t waitingGroupId: _waitingGroupCommandMap.keys()) {
            for (CommandParamType waitingGroupCommand: _waitingGroupCommandMap[waitingGroupId].keys()) {
                // This will add a new waiting index if needed and set the retry count for that index to 0
                if (!_disableAllRetries && ++_waitingGroupCommandMap[waitingGroupId][waitingGroupCommand] <= _maxGroupCommandRetry) {
                    qCDebug(ParameterManagerLog) << _logVehiclePrefix(-1) << "Retrying group command list, groupId:"
                                                 << waitingGroupId << "command:" << waitingGroupCommand;
                    ParameterGroupCommandWorker(waitingGroupId, waitingGroupCommand);
                    _groupCommandTimeoutTimer.start();
                } else {
                    _waitingGroupCommandMap[waitingGroupId].remove(waitingGroupCommand);
                    if (_waitingGroupCommandMap[waitingGroupId].size() == 0) {
                        _waitingGroupCommandMap.remove(waitingGroupId);
                        if (_waitingGroupCommandMap.size() == 0) {
                            _waitingGroupCommandMap.remove(cid);
                        }
                    }
                    if (!_vehicle->genericFirmware()) {
                        QString errorMsg = tr("Vehicle %1 did not respond to parameters group command. "
                                              "This will cause %2 to be unable to display its full user interface.")
                                .arg(_vehicle->id()).arg(qgcApp()->applicationName());
                        qCDebug(ParameterManagerLog) << errorMsg;
                        qgcApp()->showAppMessage(errorMsg);
                    }
                }
            }
        }
    }
}


/// The offline editing vehicle can have custom loaded params bolted into it.
void ParameterManager::_loadOfflineEditingParams(void)
{
//    QString paramFilename = _vehicle->firmwarePlugin()->offlineEditingParamFile(_vehicle);
//    if (paramFilename.isEmpty()) {
//        return;
//    }

//    QFile paramFile(paramFilename);
//    if (!paramFile.open(QFile::ReadOnly)) {
//        qCWarning(ParameterManagerLog) << "Unable to open offline editing params file" << paramFilename;
//    }

//    QTextStream paramStream(&paramFile);

//    while (!paramStream.atEnd()) {
//        QString line = paramStream.readLine();

//        if (line.startsWith("#")) {
//            continue;
//        }

//        QStringList paramData = line.split("\t");
//        Q_ASSERT(paramData.count() == 5);

//        int defaultComponentId = paramData.at(1).toInt();
//        _vehicle->setOfflineEditingDefaultComponentId(defaultComponentId);
//        QString paramName = paramData.at(2);
//        QString valStr = paramData.at(3);
//        MAV_PARAM_TYPE paramType = static_cast<MAV_PARAM_TYPE>(paramData.at(4).toUInt());

//        QVariant paramValue;
//        switch (paramType) {
//        case MAV_PARAM_TYPE_REAL32:
//            paramValue = QVariant(valStr.toFloat());
//            break;
//        case MAV_PARAM_TYPE_UINT32:
//            paramValue = QVariant(valStr.toUInt());
//            break;
//        case MAV_PARAM_TYPE_INT32:
//            paramValue = QVariant(valStr.toInt());
//            break;
//        case MAV_PARAM_TYPE_UINT16:
//            paramValue = QVariant((quint16)valStr.toUInt());
//            break;
//        case MAV_PARAM_TYPE_INT16:
//            paramValue = QVariant((qint16)valStr.toInt());
//            break;
//        case MAV_PARAM_TYPE_UINT8:
//            paramValue = QVariant((quint8)valStr.toUInt());
//            break;
//        case MAV_PARAM_TYPE_INT8:
//            paramValue = QVariant((qint8)valStr.toUInt());
//            break;
//        default:
//            qCritical() << "Unknown type" << paramType;
//            paramValue = QVariant(valStr.toInt());
//            break;
//        }

//        Fact* fact = new Fact(defaultComponentId, paramName, mavTypeToFactType(paramType), this);

//        FactMetaData* factMetaData = _vehicle->compInfoManager()->compInfoParam(defaultComponentId)->factMetaDataForName(paramName, fact->type());
//        fact->setMetaData(factMetaData);

//        _mapCompGroupId2FactMap[defaultComponentId][paramName] = fact;
//    }

//    _parametersReady = true;
//    _initialLoadComplete = true;
//    _debugCacheCRC.clear();
}

void ParameterManager::resetAllParametersToDefaults()
{

}

void ParameterManager::resetAllToVehicleConfiguration()
{
//    //-- https://github.com/PX4/Firmware/pull/11760
//    Fact* sysAutoConfigFact = getParameter(-1, "SYS_AUTOCONFIG");
//    if(sysAutoConfigFact) {
//        sysAutoConfigFact->setRawValue(2);
    //    }
}

void ParameterManager::ParameterGroupCommand(CommandParamType command, uint8_t groupId)
{
    if ( _waitingGroupCommandMap.contains(groupId)
         && _waitingGroupCommandMap[groupId].contains(command)) {
        _waitingGroupCommandMap[groupId].remove(command);
        if (_waitingGroupCommandMap[groupId].size() == 0)
            _waitingGroupCommandMap.remove(groupId);
    } else {
        _waitingWriteParamBatchCount++;
    }

    _waitingGroupCommandMap[groupId][command] = 0; // Add new entry and set retry count
    _saveRequired = true;
    ParameterGroupCommandWorker(groupId, command);
//    lastCommandGroupType = command;
//    lastCommandGroupId = groupId;
}

void ParameterManager::ParameterGroupCommandWorker(uint8_t groupId, CommandParamType command)
{
    WeakLinkInterfacePtr weakLink = _vehicle->vehicleLinkManager()->primaryLink();
    if (!weakLink.expired()) {
        SharedLinkInterfacePtr  sharedLink = weakLink.lock();
        ShenHangProtocolMessage message;
        message.tyMsg0 = MsgId::COMMAND_PARAM;
        message.tyMsg1 = command;
        message.payload[0] = groupId;
        _vehicle->sendShenHangMessageOnLinkThreadSafe(sharedLink.get(), message);
        _groupCommandTimeoutTimer.start();
    }
}

void ParameterManager::RefreshParameterGroup(uint8_t groupId, bool all)
{
//    if (_waitingWriteParamIndexMap.contains(groupId) &&
//            _waitingWriteParamIndexMap[groupId].contains(addrOffset)) {
//        _waitingWriteParamIndexMap[groupId].remove(addrOffset);
//        if (_waitingWriteParamIndexMap[groupId].size() == 0)
//            _waitingWriteParamIndexMap.remove(groupId);
//    }
}

QString ParameterManager::_logVehiclePrefix(int componentId)
{
    if (componentId == -1) {
        return QString("V:%1").arg(_vehicle->id());
    } else {
        return QString("V:%1 C:%2").arg(_vehicle->id()).arg(componentId);
    }
}

void ParameterManager::_setLoadProgress(double loadProgress)
{
    if (_loadProgress != loadProgress) {
        _loadProgress = loadProgress;
        emit loadProgressChanged(static_cast<float>(loadProgress));
    }
}

bool ParameterManager::pendingWrites(void)
{
    for (int compId: _waitingWriteParamNameMap.keys()) {
        if (_waitingWriteParamNameMap[compId].count()) {
            return true;
        }
    }

    return false;
}
