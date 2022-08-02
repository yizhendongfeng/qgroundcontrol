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
#include "ComponentInformationManager.h"
#include "CompInfoParam.h"

#include <QEasingCurve>
#include <QFile>
#include <QDebug>
#include <QVariantAnimation>
#include <QJsonArray>

QGC_LOGGING_CATEGORY(ParameterManagerVerbose1Log,           "ParameterManagerVerbose1Log")
QGC_LOGGING_CATEGORY(ParameterManagerVerbose2Log,           "ParameterManagerVerbose2Log")
QGC_LOGGING_CATEGORY(ParameterManagerDebugCacheFailureLog,  "ParameterManagerDebugCacheFailureLog") // Turn on to debug parameter cache crc misses

const QHash<int, QString> _mavlinkCompIdHash {
    { MAV_COMP_ID_CAMERA,   "Camera1" },
    { MAV_COMP_ID_CAMERA2,  "Camera2" },
    { MAV_COMP_ID_CAMERA3,  "Camera3" },
    { MAV_COMP_ID_CAMERA4,  "Camera4" },
    { MAV_COMP_ID_CAMERA5,  "Camera5" },
    { MAV_COMP_ID_CAMERA6,  "Camera6" },
    { MAV_COMP_ID_SERVO1,   "Servo1" },
    { MAV_COMP_ID_SERVO2,   "Servo2" },
    { MAV_COMP_ID_SERVO3,   "Servo3" },
    { MAV_COMP_ID_SERVO4,   "Servo4" },
    { MAV_COMP_ID_SERVO5,   "Servo5" },
    { MAV_COMP_ID_SERVO6,   "Servo6" },
    { MAV_COMP_ID_SERVO7,   "Servo7" },
    { MAV_COMP_ID_SERVO8,   "Servo8" },
    { MAV_COMP_ID_SERVO9,   "Servo9" },
    { MAV_COMP_ID_SERVO10,  "Servo10" },
    { MAV_COMP_ID_SERVO11,  "Servo11" },
    { MAV_COMP_ID_SERVO12,  "Servo12" },
    { MAV_COMP_ID_SERVO13,  "Servo13" },
    { MAV_COMP_ID_SERVO14,  "Servo14" },
    { MAV_COMP_ID_GIMBAL,   "Gimbal1" },
    { MAV_COMP_ID_ADSB,     "ADSB" },
    { MAV_COMP_ID_OSD,      "OSD" },
    { MAV_COMP_ID_FLARM,    "FLARM" },
    { MAV_COMP_ID_GIMBAL2,  "Gimbal2" },
    { MAV_COMP_ID_GIMBAL3,  "Gimbal3" },
    { MAV_COMP_ID_GIMBAL4,  "Gimbal4" },
    { MAV_COMP_ID_GIMBAL5,  "Gimbal5" },
    { MAV_COMP_ID_GIMBAL6,  "Gimbal6" },
    { MAV_COMP_ID_IMU,      "IMU1" },
    { MAV_COMP_ID_IMU_2,    "IMU2" },
    { MAV_COMP_ID_IMU_3,    "IMU3" },
    { MAV_COMP_ID_GPS,      "GPS1" },
    { MAV_COMP_ID_GPS2,     "GPS2" }
};

const char* ParameterManager::_jsonParametersKey =          "parameters";
const char* ParameterManager::_jsonCompIdKey =              "compId";
const char* ParameterManager::_jsonParamNameKey =           "name";
const char* ParameterManager::_jsonParamValueKey =          "value";

ParameterManager::ParameterManager(Vehicle* vehicle)
    : QObject                           (vehicle)
    , _vehicle                          (vehicle)
    , _mavlink                          (nullptr)
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

    for (int compId: _waitingReadParamIndexMap.keys()) {
        for (int groupId: _waitingReadParamIndexMap[compId].keys())
            waitingReadParamIndexCount += _waitingReadParamIndexMap[compId][groupId].count();
    }

    for(int compId: _waitingWriteParamIndexMap.keys()) {
        for (int groupId: _waitingWriteParamIndexMap[compId].keys())
            waitingWriteParamCount += _waitingWriteParamIndexMap[compId][groupId].count();
    }

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


void ParameterManager::mavlinkMessageReceived(mavlink_message_t message)
{
    if (message.msgid == MAVLINK_MSG_ID_PARAM_VALUE) {
        mavlink_param_value_t param_value;
        mavlink_msg_param_value_decode(&message, &param_value);

        // This will null terminate the name string
        QByteArray bytes(param_value.param_id, MAVLINK_MSG_PARAM_VALUE_FIELD_PARAM_ID_LEN);
        QString parameterName(bytes);

        mavlink_param_union_t paramUnion;
        paramUnion.param_float  = param_value.param_value;
        paramUnion.type         = param_value.param_type;

        QVariant parameterValue;

        switch (paramUnion.type) {
        case MAV_PARAM_TYPE_REAL32:
            parameterValue = QVariant(paramUnion.param_float);
            break;
        case MAV_PARAM_TYPE_UINT8:
            parameterValue = QVariant(paramUnion.param_uint8);
            break;
        case MAV_PARAM_TYPE_INT8:
            parameterValue = QVariant(paramUnion.param_int8);
            break;
        case MAV_PARAM_TYPE_UINT16:
            parameterValue = QVariant(paramUnion.param_uint16);
            break;
        case MAV_PARAM_TYPE_INT16:
            parameterValue = QVariant(paramUnion.param_int16);
            break;
        case MAV_PARAM_TYPE_UINT32:
            parameterValue = QVariant(paramUnion.param_uint32);
            break;
        case MAV_PARAM_TYPE_INT32:
            parameterValue = QVariant(paramUnion.param_int32);
            break;
        default:
            qCritical() << "ParameterManager::_handleParamValue - unsupported MAV_PARAM_TYPE" << paramUnion.type;
            break;
        }

//        _handleParamValue(message.compid, parameterName, param_value.param_count, param_value.param_index, static_cast<MAV_PARAM_TYPE>(param_value.param_type), parameterValue);
    }
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
            if(_waitingGroupCommandMap[MAV_COMP_ID_AUTOPILOT1].contains(groupId)
                    && _waitingGroupCommandMap[MAV_COMP_ID_AUTOPILOT1][groupId].contains(COMMAND_RESET_PARAM)) {
                _waitingGroupCommandMap[MAV_COMP_ID_AUTOPILOT1][groupId].remove(COMMAND_RESET_PARAM);
                if (_waitingGroupCommandMap[MAV_COMP_ID_AUTOPILOT1][groupId].size() == 0) {
                    _waitingGroupCommandMap[MAV_COMP_ID_AUTOPILOT1].remove(groupId);
                }
            }
            if (execuateState != 0) {
                qgcApp()->showAppMessage(QString("Rest groupId: %1 parameter failed!").arg(groupId));
            } else {    // 重置成功
                refreshGroupParameters(MAV_COMP_ID_AUTOPILOT1, groupId);
            }

            break;
        case ACK_LOAD_PARAM:
            groupId = message.payload[0];
            execuateState = message.payload[1];
            if(_waitingGroupCommandMap[MAV_COMP_ID_AUTOPILOT1].contains(groupId)
                    && _waitingGroupCommandMap[MAV_COMP_ID_AUTOPILOT1][groupId].contains(COMMAND_LOAD_PARAM)) {
                _waitingGroupCommandMap[MAV_COMP_ID_AUTOPILOT1][groupId].remove(COMMAND_LOAD_PARAM);
                if (_waitingGroupCommandMap[MAV_COMP_ID_AUTOPILOT1][groupId].size() == 0) {
                    _waitingGroupCommandMap[MAV_COMP_ID_AUTOPILOT1].remove(groupId);
                }
            }
            if (execuateState != 0) {
                qgcApp()->showAppMessage(QString("Load groupId: %1 parameter failed!").arg(groupId));
            }

            break;
        case ACK_SAVE_PARAM:
            groupId = message.payload[0];
            execuateState = message.payload[1];
            if(_waitingGroupCommandMap[MAV_COMP_ID_AUTOPILOT1].contains(groupId)
                    && _waitingGroupCommandMap[MAV_COMP_ID_AUTOPILOT1][groupId].contains(COMMAND_SAVE_PARAM)) {
                _waitingGroupCommandMap[MAV_COMP_ID_AUTOPILOT1][groupId].remove(COMMAND_SAVE_PARAM);
                if (_waitingGroupCommandMap[MAV_COMP_ID_AUTOPILOT1][groupId].size() == 0) {
                    _waitingGroupCommandMap[MAV_COMP_ID_AUTOPILOT1].remove(groupId);
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
            factMetaData = _vehicle->compInfoManager()->compInfoParam(MAV_COMP_ID_AUTOPILOT1)->factMetaDataByIndexes(groupId, addrOffset);
            paramUnion.type = factMetaData->type();
            //            qCDebug(ParameterManagerLog) << QString("handle parameter groupId:%1, addrOffset:%2").arg(groupId).arg(addrOffset);
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

        _handleParamValue(MAV_COMP_ID_AUTOPILOT1, groupId, addrOffset, paramUnion.type, dataVariant);
        if (_waitingGroupCommandMap[MAV_COMP_ID_AUTOPILOT1].keys().count() == 0
                && _groupCommandTimeoutTimer.isActive())
            _groupCommandTimeoutTimer.stop();
    }
}

/// Called whenever a parameter is updated or first seen.
void ParameterManager::_handleParamValue(uint8_t componentId, uint8_t groupId, uint16_t addrOffset, FactMetaData::ValueType_t mavParamType, QVariant parameterValue)
{

    qCDebug(ParameterManagerVerbose1Log) << _logVehiclePrefix(componentId)
                                         <<  "_handleParamValue"
                                         <<   "groupId:" << groupId
                                         <<   "addrOffset:" << addrOffset
                                         <<   "mavType:" << mavParamType
                                         <<   "value:" << parameterValue << ")";
    _initialRequestTimeoutTimer.stop();
    _waitingParamTimeoutTimer.stop();

    // Update our total parameter counts
    if (!_paramCountMap.contains(componentId)) {
        QObject* parameterMetaData = _vehicle->compInfoManager()->compInfoParam(componentId)->_getOpaqueParameterMetaData();
        shenHangMetaData = qobject_cast<ShenHangParameterMetaData*>(parameterMetaData);
        _paramCountMap[componentId] = shenHangMetaData->getFactMetaDataCount();
        _totalParamCount += _paramCountMap[componentId];
    }

    // If we've never seen this component id before, setup the index wait lists.
    if (_waitingReadParamIndexMap.size() == 0) {
        // Add all indices to the wait list, parameter index is 0-based
//        for (int waitingIndex=0; waitingIndex<_paramCountMap[componentId]; waitingIndex++) {
//            // This will add the new component id, as well as the the new waiting index and set the retry count for that index to 0
//            _waitingReadParamIndexMap[componentId][waitingIndex] = 0;
//        }
        QMap<uint8_t, MetaDataGroup> mapMetaGroups = shenHangMetaData->getMapMetaDataGroup();
        for(uint8_t groupId: mapMetaGroups.keys()) {
            for(uint16_t addrOffset: mapMetaGroups[groupId].mapMetaData.keys()) {
                _waitingReadParamIndexMap[componentId][groupId][addrOffset] = 0;
            }
        }
    }

    // Remove this parameter from the waiting lists
    if (_waitingReadParamIndexMap[componentId].contains(groupId)) {
        if (_waitingReadParamIndexMap[componentId][groupId].contains(addrOffset)) {
            _waitingReadParamIndexMap[componentId][groupId].remove(addrOffset);
        }
        if (_waitingReadParamIndexMap[componentId][groupId].size() == 0) {
            _waitingReadParamIndexMap[componentId].remove(groupId);
        }
        if (_indexBatchQueue.contains(groupId) && _indexBatchQueue[groupId].contains(addrOffset)) {
            _indexBatchQueue[groupId].removeOne(addrOffset);
            if (_indexBatchQueue[groupId].size() == 0) {
                _indexBatchQueue.remove(groupId);
            }
        }
        _fillIndexBatchQueue(false /* waitingParamTimeout */);
    }

    if (_waitingWriteParamIndexMap[componentId].contains(groupId) &&
            _waitingWriteParamIndexMap[componentId][groupId].contains(addrOffset)) {
        _waitingWriteParamIndexMap[componentId][groupId].remove(addrOffset);
        if (_waitingWriteParamIndexMap[componentId][groupId].size() == 0)
            _waitingWriteParamIndexMap[componentId].remove(groupId);
    }
//    _waitingWriteParamNameMap[componentId].remove(parameterName);
//    if (_waitingReadParamIndexMap[componentId].count()) {
//        qCDebug(ParameterManagerVerbose2Log) << _logVehiclePrefix(componentId) << "_waitingReadParamIndexMap:" << _waitingReadParamIndexMap[componentId];
//    }
//    if (_waitingReadParamNameMap[componentId].count()) {
//        qCDebug(ParameterManagerVerbose2Log) << _logVehiclePrefix(componentId) << "_waitingReadParamNameMap" << _waitingReadParamNameMap[componentId];
//    }
//    if (_waitingWriteParamNameMap[componentId].count()) {
//        qCDebug(ParameterManagerVerbose2Log) << _logVehiclePrefix(componentId) << "_waitingWriteParamNameMap" << _waitingWriteParamNameMap[componentId];
//    }

    // Track how many parameters we are still waiting for

    int waitingReadParamIndexCount = 0;
    int waitingWriteParamIndexCount = 0;

    for(int waitingComponentId: _waitingReadParamIndexMap.keys()) {
        for (int waitingGroupId: _waitingReadParamIndexMap[waitingComponentId].keys()) {
            waitingReadParamIndexCount += _waitingReadParamIndexMap[waitingComponentId][waitingGroupId].count();
        }
    }

    if (waitingReadParamIndexCount) {
        qCDebug(ParameterManagerVerbose1Log) << _logVehiclePrefix(componentId) << "waitingReadParamIndexCount:" << waitingReadParamIndexCount;
    }

//    for(int waitingComponentId: _waitingReadParamNameMap.keys()) {
//        waitingReadParamNameCount += _waitingReadParamNameMap[waitingComponentId].count();
//    }
//    if (waitingReadParamNameCount) {
//        qCDebug(ParameterManagerVerbose1Log) << _logVehiclePrefix(componentId) << "waitingReadParamNameCount:" << waitingReadParamNameCount;
//    }

    for(int waitingComponentId: _waitingWriteParamIndexMap.keys()) {
        for(int waitingGroupId: _waitingWriteParamIndexMap[waitingComponentId].keys()) {
            waitingWriteParamIndexCount += _waitingWriteParamIndexMap[waitingComponentId][waitingGroupId].count();
        }
    }
    if (waitingWriteParamIndexCount) {
        qCDebug(ParameterManagerVerbose1Log) << _logVehiclePrefix(componentId) << "waitingWriteParamNameCount:" << waitingWriteParamIndexCount;
    }

    int totalWaitingParamCount = waitingReadParamIndexCount + waitingWriteParamIndexCount;
    if (totalWaitingParamCount) {
        // More params to wait for, restart timer
        _waitingParamTimeoutTimer.start();
        qCDebug(ParameterManagerVerbose1Log) << _logVehiclePrefix(-1) << "Restarting _waitingParamTimeoutTimer: totalWaitingParamCount:" << totalWaitingParamCount;
    } else {
        if (!_mapCompGroupId2FactMap.contains(_vehicle->defaultComponentId())) {
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
        if (_mapCompGroupId2FactMap.contains(componentId) && _mapCompGroupId2FactMap[componentId].contains(groupId) &&
                _mapCompGroupId2FactMap[componentId][groupId].contains(addrOffset)) {
            fact = _mapCompGroupId2FactMap[componentId][groupId][addrOffset];
        } else {
            FactMetaData* factMetaData = _vehicle->compInfoManager()->compInfoParam(componentId)->factMetaDataByIndexes(groupId, addrOffset);
            fact = new Fact(componentId, groupId, addrOffset, factMetaData->type(), factMetaData->name(), this);
            fact->setMetaData(factMetaData);
            qCDebug(ParameterManagerVerbose1Log) << _logVehiclePrefix(componentId) << "Adding new fact groupId:" << groupId << "addrOffset:" << addrOffset
                                                 << "type:" << factMetaData->type() << "name:" << factMetaData->name();
            _mapCompGroupId2FactMap[componentId][groupId][addrOffset] = fact;

            // We need to know when the fact value changes so we can update the vehicle
            connect(fact, &Fact::_containerRawValueChanged, this, &ParameterManager::_factRawValueUpdated);

            emit factAdded(componentId, fact);
        }

        fact->_containerSetRawValue(parameterValue);
    }
    _prevWaitingReadParamIndexCount = waitingReadParamIndexCount;
    _prevWaitingWriteParamIndexCount = waitingWriteParamIndexCount;

    _checkInitialLoadComplete();

    qCDebug(ParameterManagerVerbose1Log) << "_initialLoadComplete:" << _initialLoadComplete << "waitingReadParamIndexCount:" << waitingReadParamIndexCount;
    qCDebug(ParameterManagerVerbose1Log) << _logVehiclePrefix(componentId) << "_parameterUpdate complete";
}

/// Writes the parameter update to mavlink, sets up for write wait
void ParameterManager::_factRawValueUpdateWorker(int componentId, uint8_t groupId, uint8_t lenCfg, uint16_t addrOffset, FactMetaData::ValueType_t valueType, const QVariant& rawValue)
{
    if (_waitingWriteParamIndexMap.contains(componentId)) {
        if ( _waitingWriteParamIndexMap[componentId].contains(groupId)
             && _waitingWriteParamIndexMap[componentId][groupId].contains(addrOffset)) {
            _waitingWriteParamIndexMap[componentId][groupId].remove(addrOffset);
            if (_waitingWriteParamIndexMap[componentId][groupId].size() == 0)
                _waitingWriteParamIndexMap[componentId].remove(groupId);
        } else {
            _waitingWriteParamBatchCount++;
        }

        _waitingWriteParamIndexMap[componentId][groupId][addrOffset] = 0; // Add new entry and set retry count
        _updateProgressBar();
        _waitingParamTimeoutTimer.start();
        _saveRequired = true;

    } else {
        qWarning() << "Internal error ParameterManager::_factValueUpdateWorker: component id not found" << componentId;
    }
    _sendParamSetToVehicle(componentId, groupId, lenCfg,  addrOffset, valueType, rawValue);


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

void ParameterManager::refreshGroupParameters(uint8_t componentId, uint8_t groupId)
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
    ShenHangParameterMetaData* shenHangParameterMetaData = static_cast<ShenHangParameterMetaData*>(_vehicle->compInfoManager()->compInfoParam(MAV_COMP_ID_AUTOPILOT1)->_getOpaqueParameterMetaData());
    QMap<uint8_t, MetaDataGroup> shenHangMetaDataGroups = shenHangParameterMetaData->getMapMetaDataGroup();
//    _paramCountMap[MAV_COMP_ID_AUTOPILOT1] = shenHangParameterMetaData->getFactMetaDataCount();
    for (int cid: _paramCountMap.keys()) {
        // Add/Update all indices to the wait list, parameter index is 0-based
        if(componentId != MAV_COMP_ID_ALL && componentId != cid)
            continue;
        for (uint8_t waitingGroupId: shenHangMetaDataGroups.keys()) {
            // 若请求所有参数组或与需要请求的参数组相同则加入待读
            if (groupId == 0xff || waitingGroupId == groupId) {
                for (uint16_t waitingIndex: shenHangMetaDataGroups[waitingGroupId].mapMetaData.keys()) {
                    // This will add a new waiting index if needed and set the retry count for that index to 0
                    _waitingReadParamIndexMap[cid][waitingGroupId][waitingIndex] = 0;
                }
            }
        }
    }
//    if (_waitingReadParamIndexMap[MAV_COMP_ID_AUTOPILOT1].keys().count() > 0)
//        _waitingParamTimeoutTimer.start();
    _waitingForDefaultComponent = true;
    ShenHangProtocolMessage msg;
    SharedLinkInterfacePtr  sharedLink = weakLink.lock();
    uint8_t data[16] = {};
    if (groupId == 0xff) {  // 查询所有组参数
        _indexBatchQueueActive = false;
        _initialLoadComplete = false;
        _vehicle->PackSetParamCommand(msg, CommandParamType::COMMAND_QUERY_PARAM, 0XFF, 0, 0, data, 16);
        _vehicle->sendShenHangMessageOnLinkThreadSafe(sharedLink.get(), msg);
        QString what = (componentId == MAV_COMP_ID_ALL) ? "MAV_COMP_ID_ALL" : QString::number(componentId);
        qCDebug(ParameterManagerLog) << _logVehiclePrefix(-1) << "Request to refresh all parameters for component ID:" << what;
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

void ParameterManager::refreshParameter(int componentId, const QString& paramName)
{
//    componentId = _actualComponentId(componentId);
//    qCDebug(ParameterManagerLog) << _logVehiclePrefix(componentId) << "refreshParameter - name:" << paramName << ")";

//    if (_waitingReadParamNameMap.contains(componentId)) {
//        QString mappedParamName = _remapParamNameToVersion(paramName);

//        if (_waitingReadParamNameMap[componentId].contains(mappedParamName)) {
//            _waitingReadParamNameMap[componentId].remove(mappedParamName);
//        } else {
//            _waitingReadParamNameBatchCount++;
//        }
//        _waitingReadParamNameMap[componentId][mappedParamName] = 0;     // Add new wait entry and update retry count
//        _updateProgressBar();
//        qCDebug(ParameterManagerLog) << _logVehiclePrefix(componentId) << "restarting _waitingParamTimeout";
//        _waitingParamTimeoutTimer.start();
//    } else {
//        qWarning() << "Internal error";
//    }

//    _readParameterRaw(componentId, paramName, -1);
}

//void ParameterManager::refreshParametersPrefix(int componentId, const QString& namePrefix)
//{
//    componentId = _actualComponentId(componentId);
//    qCDebug(ParameterManagerLog) << _logVehiclePrefix(componentId) << "refreshParametersPrefix - name:" << namePrefix << ")";

//    for (const QString &paramName: _mapCompGroupId2FactMap[componentId].keys()) {
//        if (paramName.startsWith(namePrefix)) {
//            refreshParameter(componentId, paramName);
//        }
//    }
//}

bool ParameterManager::parameterExists(int componentId, const uint8_t& groupId, const uint16_t& addrOffset)
{
    bool ret = false;

    componentId = _actualComponentId(componentId);
    if (_mapCompGroupId2FactMap.contains(componentId) && _mapCompGroupId2FactMap[componentId].contains(groupId)
            &&_mapCompGroupId2FactMap[componentId][groupId].contains(addrOffset)) {
        ret = true; //_mapCompGroupId2FactMap[componentId].contains(_remapParamNameToVersion(groupId));
    }

    return ret;
}

bool ParameterManager::parameterExists(int componentId, const QString& paramName)
{
    bool ret = false;

    componentId = _actualComponentId(componentId);
    if (_mapCompId2FactMap.contains(componentId)) {
        ret = _mapCompId2FactMap[componentId].contains(paramName);
    }

    return ret;
}

Fact* ParameterManager::getParameter(int componentId, const QString& paramName)
{
    componentId = _actualComponentId(componentId);

    if (!_mapCompId2FactMap.contains(componentId) || !_mapCompId2FactMap[componentId].contains(paramName)) {
        qgcApp()->reportMissingParameter(componentId, paramName);
        return &_defaultFact;
    }

    return _mapCompId2FactMap[componentId][paramName];
}

Fact* ParameterManager::getParameter(int componentId, const uint8_t& groupId, const uint16_t& addrOffset)
{
    componentId = _actualComponentId(componentId);

    QString mappedParamName = QString("groupId:%1, addrOffset:%2").arg(groupId).arg(addrOffset);//_remapParamNameToVersion(groupId);
    if (!_mapCompGroupId2FactMap.contains(componentId) || !_mapCompGroupId2FactMap[componentId].contains(groupId) ||
            !_mapCompGroupId2FactMap[componentId][groupId].contains(addrOffset)) {
        qgcApp()->reportMissingParameter(componentId, mappedParamName);
        return &_defaultFact;
    }

    return _mapCompGroupId2FactMap[componentId][groupId][addrOffset];
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

    for(int componentId: _waitingReadParamIndexMap.keys()) {
        if (_waitingReadParamIndexMap[componentId].count()) {
            qCDebug(ParameterManagerVerbose1Log) << _logVehiclePrefix(componentId) << "_waitingReadParamIndexMap count" << _waitingReadParamIndexMap[componentId].count();
            qCDebug(ParameterManagerLog) << _logVehiclePrefix(componentId) << "_waitingReadParamIndexMap" << _waitingReadParamIndexMap[componentId];
        }

        for(uint8_t groupId: _waitingReadParamIndexMap[componentId].keys()) {
            for (uint16_t addrOffset: _waitingReadParamIndexMap[componentId][groupId].keys()) {
                if (_indexBatchQueue.contains(groupId) && _indexBatchQueue[groupId].contains(addrOffset)) {
                    // Don't add more than once
                    continue;
                }

                if (getBatchQueueCount() >= maxBatchSize) {
                    break;
                }

                _waitingReadParamIndexMap[componentId][groupId][addrOffset]++;   // Bump retry count

                if (_disableAllRetries || _waitingReadParamIndexMap[componentId][groupId][addrOffset] > _maxInitialLoadRetrySingleParam) {
                    // Give up on this index
                    _failedReadParamIndexMap[componentId] << groupId;
                    qCDebug(ParameterManagerLog) << _logVehiclePrefix(componentId) << "Giving up on (paramIndex:"
                                                 << groupId << "retryCount:" << _waitingReadParamIndexMap[componentId][groupId] << ")";
                    _waitingReadParamIndexMap[componentId][groupId].remove(addrOffset);
                    if(_waitingReadParamIndexMap[componentId][groupId].size() == 0)
                        _waitingReadParamIndexMap[componentId].remove(groupId);
                } else {
                    // Retry again
                    _indexBatchQueue[groupId].append(addrOffset);
                    FactMetaData* factMetaData = _vehicle->compInfoManager()->compInfoParam(MAV_COMP_ID_AUTOPILOT1)->factMetaDataByIndexes(groupId, addrOffset);
                    if (!factMetaData) {
                        qDebug() << "fill index batch queue !factMetaData " << groupId << addrOffset;
                        continue;
                    }
                    _readParameterRaw(componentId, groupId, factMetaData->getTypeSize(), addrOffset);
                    qCDebug(ParameterManagerLog) << _logVehiclePrefix(componentId) << "Read re-request for (paramIndex:" << groupId << "retryCount:" << _waitingReadParamIndexMap[componentId][groupId] << ")";
                }
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

    if (!paramsRequested && !_waitingForDefaultComponent && !_mapCompGroupId2FactMap.contains(_vehicle->defaultComponentId())) {
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
        for(int componentId: _waitingWriteParamIndexMap.keys()) {
            for(const uint8_t &groupId: _waitingWriteParamIndexMap[componentId].keys()) {
                for (const uint16_t& addrOffset: _waitingWriteParamIndexMap[componentId][groupId].keys()) {
                    paramsRequested = true;
                    _waitingWriteParamIndexMap[componentId][groupId][addrOffset]++;   // Bump retry count
                    if (_waitingWriteParamIndexMap[componentId][groupId][addrOffset] <= _maxReadWriteRetry) {
                        if (addrOffset == 0) {//参数组命令
                            ParameterGroupCommandWorker(groupId, lastCommandGroupType);
                        } else {
                            Fact* fact = getParameter(componentId, groupId, addrOffset);
                            if (!fact) {
                                qDebug() << "!fact " << groupId << addrOffset;
                                continue;
                            }
                            _sendParamSetToVehicle(componentId, groupId, fact->metaData()->getTypeSize(), addrOffset, fact->type(), fact->rawValue());
                            qCDebug(ParameterManagerLog) << _logVehiclePrefix(componentId) << "Write resend for (groupId:" << groupId << "retryCount:" << _waitingWriteParamIndexMap[componentId][groupId][addrOffset] << ")";
                        }
                        if (++batchCount > maxBatchSize) {
                            goto Out;
                        }
                    } else {
                        // Exceeded max retry count, notify user
                        _waitingWriteParamIndexMap[componentId][groupId].remove(addrOffset);
                        if (_waitingWriteParamIndexMap[componentId][groupId].size() == 0)
                            _waitingWriteParamIndexMap[componentId].remove(groupId);
                        QString errorMsg = tr("Parameter write failed: veh:%1 comp:%2 groupId:%3 addrOffset: %4")
                                .arg(_vehicle->id()).arg(componentId).arg(groupId).arg(addrOffset);
                        qCDebug(ParameterManagerLog) << errorMsg;
                        qgcApp()->showAppMessage(errorMsg);
                    }
                }
            }
        }
    }

    if (!paramsRequested) {
        for(int componentId: _waitingReadParamIndexMap.keys()) {
            for(const uint8_t &groupId: _waitingReadParamIndexMap[componentId].keys()) {
                for (const uint16_t &addrOffset: _waitingReadParamIndexMap[componentId][groupId].keys()) {
                    paramsRequested = true;
                    _waitingReadParamIndexMap[componentId][groupId][addrOffset]++;   // Bump retry count
                    if (_waitingReadParamIndexMap[componentId][groupId][addrOffset] <= _maxReadWriteRetry) {
                        FactMetaData* factMetaData = _vehicle->compInfoManager()->compInfoParam(MAV_COMP_ID_AUTOPILOT1)->factMetaDataByIndexes(groupId, addrOffset);
                        if (!factMetaData) {
                            qDebug() << "waiting param timeout !factMetaData " << groupId << addrOffset;
                            continue;
                        }
                        _readParameterRaw(componentId, groupId, factMetaData->getTypeSize(), addrOffset);
                        qCDebug(ParameterManagerLog) << _logVehiclePrefix(componentId) << "Read re-request for (groupId:" << groupId
                                                     << "addrOffset:" << addrOffset <<  "retryCount:"
                                                     << _waitingReadParamIndexMap[componentId][groupId] << ")";
                        if (++batchCount > maxBatchSize) {
                            goto Out;
                        }
                    } else {
                        // Exceeded max retry count, notify user
                        _waitingReadParamIndexMap[componentId][groupId].remove(addrOffset);
                        if (_waitingReadParamIndexMap[componentId][groupId].size() == 0)
                            _waitingReadParamIndexMap[componentId].remove(groupId);
                        QString errorMsg = tr("Parameter read failed: veh:%1 comp:%2 group:%3 addrOffset:%4")
                                .arg(_vehicle->id()).arg(componentId).arg(groupId).arg(addrOffset);
                        qCDebug(ParameterManagerLog) << errorMsg;
                        qgcApp()->showAppMessage(errorMsg);
                    }
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

void ParameterManager::_readParameterRaw(int componentId, uint8_t groupId, uint8_t lenCfg, uint16_t addrOffset)
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

void ParameterManager::_sendParamSetToVehicle(int componentId, uint8_t groupId, uint8_t lenCfg, uint16_t addrOffset, FactMetaData::ValueType_t valueType, const QVariant& value)
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

//    for (const QString& paramName: _mapCompGroupId2FactMap[componentId].keys()) {
//        const Fact *fact = _mapCompGroupId2FactMap[componentId][paramName];
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

void ParameterManager::_tryCacheHashLoad(int vehicleId, int componentId, QVariant hash_value)
{
//    qCInfo(ParameterManagerLog) << "Attemping load from cache";

//    uint32_t crc32_value = 0;
//    /* The datastructure of the cache table */
//    CacheMapName2ParamTypeVal cacheMap;
//    QFile cacheFile(parameterCacheFile(vehicleId, componentId));
//    if (!cacheFile.exists()) {
//        /* no local cache, just wait for them to come in*/
//        return;
//    }
//    cacheFile.open(QIODevice::ReadOnly);

//    /* Deserialize the parameter cache table */
//    QDataStream ds(&cacheFile);
//    ds >> cacheMap;

//    /* compute the crc of the local cache to check against the remote */

//    for (const QString& name: cacheMap.keys()) {
//        const ParamTypeVal&             paramTypeVal    = cacheMap[name];
//        const FactMetaData::ValueType_t fact_type       = static_cast<FactMetaData::ValueType_t>(paramTypeVal.first);

//        if (_vehicle->compInfoManager()->compInfoParam(MAV_COMP_ID_AUTOPILOT1)->factMetaDataForName(name, fact_type)->volatileValue()) {
//            // Does not take part in CRC
//            qCDebug(ParameterManagerLog) << "Volatile parameter" << name;
//        } else {
//            const void *vdat = paramTypeVal.second.constData();
//            const FactMetaData::ValueType_t fact_type = static_cast<FactMetaData::ValueType_t>(paramTypeVal.first);
//            crc32_value = QGC::crc32((const uint8_t *)qPrintable(name), name.length(),  crc32_value);
//            crc32_value = QGC::crc32((const uint8_t *)vdat, FactMetaData::typeToSize(fact_type), crc32_value);
//        }
//    }

//    /* if the two param set hashes match, just load from the disk */
//    if (crc32_value == hash_value.toUInt()) {
//        qCInfo(ParameterManagerLog) << "Parameters loaded from cache" << qPrintable(QFileInfo(cacheFile).absoluteFilePath());

//        int count = cacheMap.count();
//        int index = 0;
//        for (const QString& name: cacheMap.keys()) {
//            const ParamTypeVal& paramTypeVal = cacheMap[name];
//            const FactMetaData::ValueType_t fact_type = static_cast<FactMetaData::ValueType_t>(paramTypeVal.first);
//            const MAV_PARAM_TYPE mavParamType = factTypeToMavType(fact_type);
//            _handleParamValue(componentId, name, count, index++, mavParamType, paramTypeVal.second);
//        }

//        WeakLinkInterfacePtr weakLink = _vehicle->vehicleLinkManager()->primaryLink();

//        if (!weakLink.expired()) {
//            mavlink_param_set_t     p;
//            mavlink_param_union_t   union_value;
//            SharedLinkInterfacePtr  sharedLink = weakLink.lock();

//            // Return the hash value to notify we don't want any more updates
//            memset(&p, 0, sizeof(p));
//            p.param_type = MAV_PARAM_TYPE_UINT32;
//            strncpy(p.param_id, "_HASH_CHECK", sizeof(p.param_id));
//            union_value.param_uint32 = crc32_value;
//            p.param_value = union_value.param_float;
//            p.target_system = (uint8_t)_vehicle->id();
//            p.target_component = (uint8_t)componentId;
//            mavlink_message_t msg;
//            mavlink_msg_param_set_encode_chan(_mavlink->getSystemId(),
//                                              _mavlink->getComponentId(),
//                                              sharedLink->mavlinkChannel(),
//                                              &msg,
//                                              &p);
//            _vehicle->sendMessageOnLinkThreadSafe(sharedLink.get(), msg);
//        }

//        // Give the user some feedback things loaded properly
//        QVariantAnimation *ani = new QVariantAnimation(this);
//        ani->setEasingCurve(QEasingCurve::OutCubic);
//        ani->setStartValue(0.0);
//        ani->setEndValue(1.0);
//        ani->setDuration(750);

//        connect(ani, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
//            _setLoadProgress(value.toDouble());
//        });

//        // Hide 500ms after animation finishes
//        connect(ani, &QVariantAnimation::finished, this, [this] {
//            QTimer::singleShot(500, [this] {
//                _setLoadProgress(0);
//            });
//        });

//        ani->start(QAbstractAnimation::DeleteWhenStopped);
//    } else {
//        qCInfo(ParameterManagerLog) << "Parameters cache match failed" << qPrintable(QFileInfo(cacheFile).absoluteFilePath());
//        if (ParameterManagerDebugCacheFailureLog().isDebugEnabled()) {
//            _debugCacheCRC[componentId] = true;
//            _debugCacheMap[componentId] = cacheMap;
//            for (const QString& name: cacheMap.keys()) {
//                _debugCacheParamSeen[componentId][name] = false;
//            }
//            qgcApp()->showAppMessage(tr("Parameter cache CRC match failed"));
//        }
//    }
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
//        for (const QString &paramName: _mapCompGroupId2FactMap[componentId].keys()) {
//            Fact* fact = _mapCompGroupId2FactMap[componentId][paramName];
//            if (fact) {
//                stream << _vehicle->id() << "\t" << componentId << "\t" << paramName << "\t" << fact->rawValueStringFullPrecision() << "\t" << QString("%1").arg(factTypeToMavType(fact->type())) << "\n";
//            } else {
//                qWarning() << "Internal error: missing fact";
//            }
//        }
//    }

//    stream.flush();
}

MAV_PARAM_TYPE ParameterManager::factTypeToMavType(FactMetaData::ValueType_t factType)
{
    switch (factType) {
    case FactMetaData::valueTypeUint8:
        return MAV_PARAM_TYPE_UINT8;

    case FactMetaData::valueTypeInt8:
        return MAV_PARAM_TYPE_INT8;

    case FactMetaData::valueTypeUint16:
        return MAV_PARAM_TYPE_UINT16;

    case FactMetaData::valueTypeInt16:
        return MAV_PARAM_TYPE_INT16;

    case FactMetaData::valueTypeUint32:
        return MAV_PARAM_TYPE_UINT32;

    case FactMetaData::valueTypeUint64:
        return MAV_PARAM_TYPE_UINT64;

    case FactMetaData::valueTypeInt64:
        return MAV_PARAM_TYPE_INT64;

    case FactMetaData::valueTypeFloat:
        return MAV_PARAM_TYPE_REAL32;

    case FactMetaData::valueTypeDouble:
        return MAV_PARAM_TYPE_REAL64;

    default:
        qWarning() << "Unsupported fact type" << factType;
        // fall through

    case FactMetaData::valueTypeInt32:
        return MAV_PARAM_TYPE_INT32;
    }
}

FactMetaData::ValueType_t ParameterManager::mavTypeToFactType(MAV_PARAM_TYPE mavType)
{
    switch (mavType) {
    case MAV_PARAM_TYPE_UINT8:
        return FactMetaData::valueTypeUint8;

    case MAV_PARAM_TYPE_INT8:
        return FactMetaData::valueTypeInt8;

    case MAV_PARAM_TYPE_UINT16:
        return FactMetaData::valueTypeUint16;

    case MAV_PARAM_TYPE_INT16:
        return FactMetaData::valueTypeInt16;

    case MAV_PARAM_TYPE_UINT32:
        return FactMetaData::valueTypeUint32;

    case MAV_PARAM_TYPE_UINT64:
        return FactMetaData::valueTypeUint64;

    case MAV_PARAM_TYPE_INT64:
        return FactMetaData::valueTypeInt64;

    case MAV_PARAM_TYPE_REAL32:
        return FactMetaData::valueTypeFloat;

    case MAV_PARAM_TYPE_REAL64:
        return FactMetaData::valueTypeDouble;

    default:
        qWarning() << "Unsupported mav param type" << mavType;
        // fall through

    case MAV_PARAM_TYPE_INT32:
        return FactMetaData::valueTypeInt32;
    }
}

void ParameterManager::_checkInitialLoadComplete(void)
{
    // Already processed?
    if (_initialLoadComplete) {
        return;
    }

    for (int componentId: _waitingReadParamIndexMap.keys()) {
        if (_waitingReadParamIndexMap[componentId].count()) {
            // We are still waiting on some parameters, not done yet
            return;
        }
    }

    if (!_mapCompGroupId2FactMap.contains(_vehicle->defaultComponentId())) {
        // No default component params yet, not done yet
        return;
    }

    // We aren't waiting for any more initial parameter updates, initial parameter loading is complete
    _initialLoadComplete = true;

    // Parameter cache crc failure debugging
    for (int componentId: _debugCacheParamSeen.keys()) {
        if (!_logReplay && _debugCacheCRC.contains(componentId) && _debugCacheCRC[componentId]) {
            for (const QString& paramName: _debugCacheParamSeen[componentId].keys()) {
                if (!_debugCacheParamSeen[componentId][paramName]) {
                    qDebug() << "Parameter in cache but not on vehicle componentId:Name" << componentId << paramName;
                }
            }
        }
    }
    _debugCacheCRC.clear();

    qCDebug(ParameterManagerLog) << _logVehiclePrefix(-1) << "Initial load complete";

    // Check for index based load failures
    QString indexList;
    bool initialLoadFailures = false;
    for (int componentId: _failedReadParamIndexMap.keys()) {
        for (int paramIndex: _failedReadParamIndexMap[componentId]) {
            if (initialLoadFailures) {
                indexList += ", ";
            }
            indexList += QString("%1:%2").arg(componentId).arg(paramIndex);
            initialLoadFailures = true;
            qCDebug(ParameterManagerLog) << _logVehiclePrefix(componentId) << "Gave up on initial load after max retries (paramIndex:" << paramIndex << ")";
        }
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
        refreshGroupParameters(MAV_COMP_ID_ALL, 0xff);
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
        for (uint8_t waitingGroupId: _waitingGroupCommandMap[cid].keys()) {
            for (CommandParamType waitingGroupCommand: _waitingGroupCommandMap[cid][waitingGroupId].keys()) {
                // This will add a new waiting index if needed and set the retry count for that index to 0
                if (!_disableAllRetries && ++_waitingGroupCommandMap[cid][waitingGroupId][waitingGroupCommand] <= _maxGroupCommandRetry) {
                    qCDebug(ParameterManagerLog) << _logVehiclePrefix(-1) << "Retrying group command list, groupId:"
                                                 << waitingGroupId << "command:" << waitingGroupCommand;
                    ParameterGroupCommandWorker(waitingGroupId, waitingGroupCommand);
                    _groupCommandTimeoutTimer.start();
                } else {
                    _waitingGroupCommandMap[cid][waitingGroupId].remove(waitingGroupCommand);
                    if (_waitingGroupCommandMap[cid][waitingGroupId].size() == 0) {
                        _waitingGroupCommandMap[cid].remove(waitingGroupId);
                        if (_waitingGroupCommandMap[cid].size() == 0) {
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
    if (_waitingGroupCommandMap.contains(MAV_COMP_ID_AUTOPILOT1)) {
        if ( _waitingGroupCommandMap[MAV_COMP_ID_AUTOPILOT1].contains(groupId)
             && _waitingGroupCommandMap[MAV_COMP_ID_AUTOPILOT1][groupId].contains(command)) {
            _waitingGroupCommandMap[MAV_COMP_ID_AUTOPILOT1][groupId].remove(command);
            if (_waitingGroupCommandMap[MAV_COMP_ID_AUTOPILOT1][groupId].size() == 0)
                _waitingGroupCommandMap[MAV_COMP_ID_AUTOPILOT1].remove(groupId);
        } else {
            _waitingWriteParamBatchCount++;
        }

    } else {
        qWarning() << "Internal error ParameterManager::_factValueUpdateWorker: component id not found" << MAV_COMP_ID_AUTOPILOT1;
    }
    _waitingGroupCommandMap[MAV_COMP_ID_AUTOPILOT1][groupId][command] = 0; // Add new entry and set retry count
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
//    if (_waitingWriteParamIndexMap[componentId].contains(groupId) &&
//            _waitingWriteParamIndexMap[componentId][groupId].contains(addrOffset)) {
//        _waitingWriteParamIndexMap[componentId][groupId].remove(addrOffset);
//        if (_waitingWriteParamIndexMap[componentId][groupId].size() == 0)
//            _waitingWriteParamIndexMap[componentId].remove(groupId);
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

QList<int> ParameterManager::componentIds(void)
{
    return _paramCountMap.keys();
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
