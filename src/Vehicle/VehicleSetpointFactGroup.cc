﻿/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "VehicleSetpointFactGroup.h"
#include "Vehicle.h"

#include <QtMath>

const char* VehicleSetpointFactGroup::_rollFactName =       "roll";
const char* VehicleSetpointFactGroup::_pitchFactName =      "pitch";
const char* VehicleSetpointFactGroup::_yawFactName =        "yaw";
const char* VehicleSetpointFactGroup::_rollRateFactName =   "rollRate";
const char* VehicleSetpointFactGroup::_pitchRateFactName =  "pitchRate";
const char* VehicleSetpointFactGroup::_yawRateFactName =    "yawRate";

VehicleSetpointFactGroup::VehicleSetpointFactGroup(QObject* parent)
    : FactGroup     (1000, ":/json/Vehicle/SetpointFact.json", parent)
    , _rollFact     (_rollFactName,      FactMetaData::valueTypeDouble)
    , _pitchFact    (_pitchFactName,     FactMetaData::valueTypeDouble)
    , _yawFact      (_yawFactName,       FactMetaData::valueTypeDouble)
    , _rollRateFact (_rollRateFactName,  FactMetaData::valueTypeDouble)
    , _pitchRateFact(_pitchRateFactName, FactMetaData::valueTypeDouble)
    , _yawRateFact  (_yawRateFactName,   FactMetaData::valueTypeDouble)
{
    _addFact(&_rollFact,        _rollFactName);
    _addFact(&_pitchFact,       _pitchFactName);
    _addFact(&_yawFact,         _yawFactName);
    _addFact(&_rollRateFact,    _rollRateFactName);
    _addFact(&_pitchRateFact,   _pitchRateFactName);
    _addFact(&_yawRateFact,     _yawRateFactName);

    // Start out as not available "--.--"
    _rollFact.setRawValue(qQNaN());
    _pitchFact.setRawValue(qQNaN());
    _yawFact.setRawValue(qQNaN());
    _rollRateFact.setRawValue(qQNaN());
    _pitchRateFact.setRawValue(qQNaN());
    _yawRateFact.setRawValue(qQNaN());
}
