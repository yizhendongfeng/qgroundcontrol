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
    , _metaDataAddedToFacts             (false)
    , _logReplay                        (!vehicle->vehicleLinkManager()->primaryLink().expired() && vehicle->vehicleLinkManager()->primaryLink().lock()->isLogReplay())
    , _disableAllRetries                (false)
    , _totalParamCount                  (0)
{
    _loadParameterMetaDataFromFile();
    _waitingParamTimeoutTimer.setSingleShot(true);
    _waitingParamTimeoutTimer.setInterval(3000);
    connect(&_waitingParamTimeoutTimer, &QTimer::timeout, this, &ParameterManager::_waitingParamTimeout);


    // Ensure the cache directory exists
    QFileInfo(QSettings().fileName()).dir().mkdir("ParamCache");
}

void ParameterManager::_updateProgressBar(void)
{
    int waitingReadParamIndexCount = 0;

    for (int groupId: _waitingReadParamIndexMap.keys())
        waitingReadParamIndexCount += _waitingReadParamIndexMap[groupId].count();

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
}

void ParameterManager::shenHangMessageReceived(ShenHangProtocolMessage message)
{
    if (message.tyMsg0 != ACK_COMMAND_PARAM)
        return;
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
    if (lastSentMessage.tyMsg1 == message.tyMsg1)
        _waitingParamTimeoutTimer.stop();
    switch (message.tyMsg1)
    {
    case ACK_RESET_PARAM:
        groupId = message.payload[0];
        execuateState = message.payload[1];

        if (execuateState != 0) {
            qgcApp()->showAppMessage(QString("Rest groupId: %1 parameter failed!").arg(groupId));
        } else {    // 重置成功
            refreshGroupParameters(groupId);
        }

        break;
    case ACK_LOAD_PARAM:
        groupId = message.payload[0];
        execuateState = message.payload[1];
        if (execuateState != 0) {
            qgcApp()->showAppMessage(QString("Load groupId: %1 parameter failed!").arg(groupId));
        }

        break;
    case ACK_SAVE_PARAM:
        groupId = message.payload[0];
        execuateState = message.payload[1];
        if (execuateState != 0) {
            qgcApp()->showAppMessage(QString("Save groupId: %1 parameter failed!").arg(groupId));
        }

        break;
    case ACK_QUERY_PARAM:
        groupId = message.payload[0];
        lenCfg = message.payload[1];
        memcpy(&addrOffset, message.payload + 2, 2);
        memcpy(data, message.payload + 4, 16);
        memcpy(&paramUnion.paramFloat, data, lenCfg);
        factMetaData = _shenHangMetaData->getMetaDataForFact(groupId, addrOffset);
        if (!factMetaData) {
            qCDebug(ParameterManagerLog) << "Didn't find parameter groupId:" << groupId << ", addrOffset:" << addrOffset;
            return;
        }
        paramUnion.type = factMetaData->type();
        qCDebug(ParameterManagerLog) << QString("handle %1 parameter groupId:%2, addrOffset:%3")
                                        .arg(message.tyMsg1).arg(groupId).arg(addrOffset);
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
        _handleParamValue(groupId, addrOffset, paramUnion.type, dataVariant);
        break;
    case ACK_SET_PARAM:
        groupId = message.payload[0];
        lenCfg = message.payload[1];
        memcpy(&addrOffset, message.payload + 2, 2);
        memcpy(data, message.payload + 4, 16);
        memcpy(&paramUnion.paramFloat, data, lenCfg);

        factMetaData = _shenHangMetaData->getMetaDataForFact(groupId, addrOffset);
        if (!factMetaData) {
            qCDebug(ParameterManagerLog) << "Didn't find parameter groupId:" << groupId << ", addrOffset:" << addrOffset;
            return;
        }
        paramUnion.type = factMetaData->type();
        qCDebug(ParameterManagerLog) << QString("handle %1 parameter groupId:%2, addrOffset:%3")
                                        .arg(message.tyMsg1).arg(groupId).arg(addrOffset);
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
        _handleParamValue(groupId, addrOffset, paramUnion.type, dataVariant);
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
}

/// Called whenever a parameter is updated or first seen.
void ParameterManager::_handleParamValue(uint8_t groupId, uint16_t addrOffset, FactMetaData::ValueType_t mavParamType, QVariant parameterValue)
{

    qCDebug(ParameterManagerVerbose1Log) <<  "_handleParamValue groupId:" << groupId
                                         <<   "addrOffset:" << addrOffset
                                         <<   "mavType:" << mavParamType
                                         <<   "value:" << parameterValue << ")";

    // Update our total parameter counts
    _totalParamCount = _shenHangMetaData->getFactMetaDataCount();
    // Remove this parameter from the waiting lists
    if (_waitingReadParamIndexMap.contains(groupId)) {
        if (_waitingReadParamIndexMap[groupId].contains(addrOffset)) {
            _waitingReadParamIndexMap[groupId].removeAll(addrOffset);
        }
        if (_waitingReadParamIndexMap[groupId].size() == 0) {
            _waitingReadParamIndexMap.remove(groupId);
        }
    }

    _updateProgressBar();
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

    // Track how many parameters we are still waiting for
    int waitingReadParamIndexCount = 0;
    for (int waitingGroupId: _waitingReadParamIndexMap.keys()) {
        waitingReadParamIndexCount += _waitingReadParamIndexMap[waitingGroupId].count();
    }

    if (waitingReadParamIndexCount) {
        qCDebug(ParameterManagerVerbose1Log)  << "waitingReadParamIndexCount:" << waitingReadParamIndexCount;
        uint8_t groupId = _waitingReadParamIndexMap.firstKey();
        uint16_t addrOffset = _waitingReadParamIndexMap.first().first();
        FactMetaData* factMetaData = _shenHangMetaData->getMetaDataForFact(groupId, addrOffset);
        _sendParamCommandToVehicle(CommandParamType::COMMAND_QUERY_PARAM, groupId, factMetaData->getTypeSize(), addrOffset);
        qCDebug(ParameterManagerVerbose1Log) << _logVehiclePrefix(-1) << "Restarting _waitingParamTimeoutTimer: waitingReadParamIndexCount:" << waitingReadParamIndexCount;
    } else {
        _checkInitialLoadComplete();
    }
    qCDebug(ParameterManagerVerbose1Log) << "waitingReadParamIndexCount:" << waitingReadParamIndexCount;
}

void ParameterManager::_factRawValueUpdated(const QVariant& rawValue)
{
    Fact* fact = qobject_cast<Fact*>(sender());
    if (!fact) {
        qWarning() << "Internal error";
        return;
    }
    _sendParamCommandToVehicle(CommandParamType::COMMAND_SET_PARAM, fact->groupId(),
                               fact->metaData()->getTypeSize(), fact->addrOffset(), fact->type(), rawValue);

    qCDebug(ParameterManagerLog) << "Set parameter(_factRawValueUpdated) groupId:" << fact->groupId()
                                 << ", addrOffset:" << fact->addrOffset() << ", value:" << rawValue;
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
    // Reset index wait lists
    QMap<uint8_t, MetaDataGroup> shenHangMetaDataGroups = _shenHangMetaData->getMapMetaDataGroup();
    _waitingReadParamIndexMap.clear();
    for (uint8_t waitingGroupId: shenHangMetaDataGroups.keys()) {
        // 若请求所有参数组或与需要请求的参数组相同则加入待读
        if (groupId == 0XFF || waitingGroupId == groupId) {
            for (uint16_t waitingAddrOffset: shenHangMetaDataGroups[waitingGroupId].mapMetaData.keys()) {
                // This will add a new waiting index if needed and set the retry count for that index to 0
                _waitingReadParamIndexMap[waitingGroupId].append(waitingAddrOffset);
            }
        }
    }
    if (!_waitingReadParamIndexMap.isEmpty() && !_waitingReadParamIndexMap.first().isEmpty()) {
        uint8_t groupIdQurey = _waitingReadParamIndexMap.firstKey();
        uint16_t addrOffsetQurey= _waitingReadParamIndexMap.first().first();
        FactMetaData* factMetaData = _shenHangMetaData->getMetaDataForFact(groupIdQurey, addrOffsetQurey);
        if (!factMetaData) {
            qDebug() << "fill index batch queue !factMetaData " << groupIdQurey << addrOffsetQurey;
            return;
        }
        _sendParamCommandToVehicle(COMMAND_QUERY_PARAM, groupIdQurey, factMetaData->getTypeSize(), addrOffsetQurey);
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

bool ParameterManager::parameterExists(const uint8_t& groupId, const uint16_t& addrOffset)
{
    bool ret = false;
    if (_mapGroupId2FactMap.contains(groupId) &&_mapGroupId2FactMap[groupId].contains(addrOffset)) {
        ret = true;
    }
    return ret;
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

QStringList ParameterManager::parameterNames(int componentId)
{
    QStringList names;

//    for(const QString &paramName: _mapCompGroupId2FactMap[_actualComponentId(componentId)].keys()) {
//        names << paramName;
//    }

    return names;
}

void ParameterManager::_waitingParamTimeout(void)
{
    resendParameterCommand();
}

void ParameterManager::_sendParamCommandToVehicle(CommandParamType command, uint8_t groupId, uint8_t lenCfg, uint16_t addrOffset, FactMetaData::ValueType_t valueType, const QVariant& value)
{
    WeakLinkInterfacePtr weakLink = _vehicle->vehicleLinkManager()->primaryLink();
    if (!weakLink.expired()) {
        mavlink_param_union_t   union_value;
        SharedLinkInterfacePtr  sharedLink = weakLink.lock();
        ShenHangProtocolMessage message;
        message.tyMsg0 = MsgId::COMMAND_PARAM;
        message.tyMsg1 = command;
        message.payload[0] = groupId;
        switch (command) {
        case CommandParamType::COMMAND_QUERY_PARAM:
            message.payload[1] = lenCfg;
            memcpy(message.payload + 2, &addrOffset, 2);
            break;

        case CommandParamType::COMMAND_SET_PARAM:
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
            break;

        default:
            break;
        }

        _vehicle->sendShenHangMessageOnLinkThreadSafe(sharedLink.get(), message);
        lastSentMessage = message;
        _waitingParamTimeoutTimer.start();
        _paramResentCount = 0;
    }
}

void ParameterManager::_checkInitialLoadComplete(void)
{
    if (_waitingReadParamIndexMap.count()) {
        // We are still waiting on some parameters, not done yet
        return;
    }

    qCDebug(ParameterManagerLog) << _logVehiclePrefix(-1) << "Initial load complete";

    // Check for index based load failures
    QString strFailedParameterList;
    bool initialLoadFailures = false;
    for (uint8_t groupId: _failedReadParamIndexMap.keys()) {
        for (uint16_t addrOffset: _failedReadParamIndexMap[groupId]) {
            if (initialLoadFailures) {
                strFailedParameterList += ", ";
            }
            strFailedParameterList += QString("groupId:%1 addrOffset:%2").arg(groupId).arg(addrOffset);
            initialLoadFailures = true;
            qCDebug(ParameterManagerLog) << "Gave up on initial load after max retries (groupId:"
                                         << groupId << "addrOffset:" << addrOffset << ")";
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
            qCWarning(ParameterManagerLog) << _logVehiclePrefix(-1) << "The following parameter indices could not be loaded after the maximum number of retries: " << strFailedParameterList;
        }
    }

    // Signal load complete
    _parametersReady = true;
    _vehicle->autopilotPlugin()->parametersReadyPreChecks();
    emit parametersReadyChanged(true);
    emit missingParametersChanged(_missingParameters);
}

void ParameterManager::resetAllToVehicleConfiguration()
{
//    //-- https://github.com/PX4/Firmware/pull/11760
//    Fact* sysAutoConfigFact = getParameter(-1, "SYS_AUTOCONFIG");
//    if(sysAutoConfigFact) {
//        sysAutoConfigFact->setRawValue(2);
    //    }
}

void ParameterManager::parameterGroupCommand(CommandParamType command, uint8_t groupId)
{
    if (command == CommandParamType::COMMAND_QUERY_PARAM)
        refreshGroupParameters(groupId);
    else
        _sendParamCommandToVehicle(command, groupId);
}

void ParameterManager::resendParameterCommand()
{
    WeakLinkInterfacePtr weakLink = _vehicle->vehicleLinkManager()->primaryLink();
    if (!weakLink.expired()) {
        SharedLinkInterfacePtr  sharedLink = weakLink.lock();
        _vehicle->sendShenHangMessageOnLinkThreadSafe(sharedLink.get(), lastSentMessage);
        if (++_paramResentCount > _maxParameterRetry) {
            uint8_t groupIdTemp = 0;
            uint16_t addrOffsetTemp = 0;
            groupIdTemp = lastSentMessage.payload[0];
            memcpy(&addrOffsetTemp, lastSentMessage.payload + 2, 2);
            switch (lastSentMessage.tyMsg1) {
            case CommandParamType::COMMAND_QUERY_PARAM:
                // Remove this parameter from the waiting lists
                if (_waitingReadParamIndexMap.contains(groupIdTemp)) {
                    if (_waitingReadParamIndexMap[groupIdTemp].contains(addrOffsetTemp)) {
                        _waitingReadParamIndexMap[groupIdTemp].removeAll(addrOffsetTemp);
                        _failedReadParamIndexMap[groupIdTemp].append(addrOffsetTemp);
                    }
                    if (_waitingReadParamIndexMap[groupIdTemp].size() == 0) {
                        _waitingReadParamIndexMap.remove(groupIdTemp);
                    }
                }
                break;
            default:
                break;
            }
        } else {
            _waitingParamTimeoutTimer.start();
        }
    }
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
