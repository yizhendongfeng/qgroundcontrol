/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "GPSRTKFactGroup.h"

const char* GPSRTKFactGroup::_connectedFactName =                "connected";
const char* GPSRTKFactGroup::_currentAccuracyFactName =          "currentAccuracy";
const char* GPSRTKFactGroup::_currentDurationFactName =          "currentDuration";
const char* GPSRTKFactGroup::_currentLatitudeFactName =          "currentLatitude";
const char* GPSRTKFactGroup::_currentLongitudeFactName =         "currentLongitude";
const char* GPSRTKFactGroup::_currentAltitudeFactName =          "currentAltitude";
const char* GPSRTKFactGroup::_validFactName =                    "valid";
const char* GPSRTKFactGroup::_activeFactName =                   "active";
const char* GPSRTKFactGroup::_numSatellitesFactName =            "numSatellites";

GPSRTKFactGroup::GPSRTKFactGroup(QObject* parent)
    : FactGroup             (1000, ":/json/Vehicle/GPSRTKFact.json", parent)
    , _connected            (_connectedFactName,         FactMetaData::valueTypeBool)
    , _currentDuration      (_currentDurationFactName,   FactMetaData::valueTypeDouble)
    , _currentAccuracy      (_currentAccuracyFactName,   FactMetaData::valueTypeDouble)
    , _currentLatitude      (_currentLatitudeFactName,   FactMetaData::valueTypeDouble)
    , _currentLongitude     (_currentLongitudeFactName,  FactMetaData::valueTypeDouble)
    , _currentAltitude      (_currentAltitudeFactName,   FactMetaData::valueTypeFloat)
    , _valid                (_validFactName,             FactMetaData::valueTypeBool)
    , _active               (_activeFactName,            FactMetaData::valueTypeBool)
    , _numSatellites        (_numSatellitesFactName,     FactMetaData::valueTypeInt32)
{
    _addFact(&_connected,          _connectedFactName);
    _addFact(&_currentDuration,    _currentDurationFactName);
    _addFact(&_currentAccuracy,    _currentAccuracyFactName);
    _addFact(&_currentLatitude,    _currentLatitudeFactName);
    _addFact(&_currentLongitude,   _currentLongitudeFactName);
    _addFact(&_currentAltitude,    _currentAltitudeFactName);
    _addFact(&_valid,              _validFactName);
    _addFact(&_active,             _activeFactName);
    _addFact(&_numSatellites,      _numSatellitesFactName);
}

