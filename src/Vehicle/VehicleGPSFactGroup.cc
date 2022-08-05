/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "VehicleGPSFactGroup.h"
#include "Vehicle.h"
#include "QGCGeo.h"

const char* VehicleGPSFactGroup::_latFactName =                 "lat";
const char* VehicleGPSFactGroup::_lonFactName =                 "lon";
const char* VehicleGPSFactGroup::_mgrsFactName =                "mgrs";
const char* VehicleGPSFactGroup::_hdopFactName =                "hdop";
const char* VehicleGPSFactGroup::_vdopFactName =                "vdop";
const char* VehicleGPSFactGroup::_courseOverGroundFactName =    "courseOverGround";
const char* VehicleGPSFactGroup::_countFactName =               "count";
const char* VehicleGPSFactGroup::_statusFactName =              "status";
const char* VehicleGPSFactGroup::_dualAntennaFactName =         "dualAntenna";

VehicleGPSFactGroup::VehicleGPSFactGroup(QObject* parent)
    : FactGroup(1000, ":/json/Vehicle/GPSFact.json", parent)
    , _latFact              (_latFactName,               FactMetaData::valueTypeDouble)
    , _lonFact              (_lonFactName,               FactMetaData::valueTypeDouble)
    , _mgrsFact             (_mgrsFactName,              FactMetaData::valueTypeString)
    , _hdopFact             (_hdopFactName,              FactMetaData::valueTypeDouble)
    , _vdopFact             (_vdopFactName,              FactMetaData::valueTypeDouble)
    , _courseOverGroundFact (_courseOverGroundFactName,  FactMetaData::valueTypeDouble)
    , _countFact            (_countFactName,             FactMetaData::valueTypeInt32)
    , _statusFact           (_statusFactName,            FactMetaData::valueTypeInt32)
    , _dualAntennaFact      (_dualAntennaFactName,        FactMetaData::valueTypeInt32)
{
    _addFact(&_latFact,                 _latFactName);
    _addFact(&_lonFact,                 _lonFactName);
    _addFact(&_mgrsFact,                _mgrsFactName);
    _addFact(&_hdopFact,                _hdopFactName);
    _addFact(&_vdopFact,                _vdopFactName);
    _addFact(&_courseOverGroundFact,    _courseOverGroundFactName);
    _addFact(&_countFact,               _countFactName);
    _addFact(&_statusFact,              _statusFactName);
    _addFact(&_dualAntennaFact,         _dualAntennaFactName);

    _latFact.setRawValue(std::numeric_limits<float>::quiet_NaN());
    _lonFact.setRawValue(std::numeric_limits<float>::quiet_NaN());
    _mgrsFact.setRawValue("");
    _hdopFact.setRawValue(std::numeric_limits<float>::quiet_NaN());
    _vdopFact.setRawValue(std::numeric_limits<float>::quiet_NaN());
    _courseOverGroundFact.setRawValue(std::numeric_limits<float>::quiet_NaN());
}

void VehicleGPSFactGroup::setShenHangGpsInfo(GpsRawInt& gpsRawInt)
{
    lat()->setRawValue              (gpsRawInt.lat * 1e-7);
    lon()->setRawValue              (gpsRawInt.lon * 1e-7);
    mgrs()->setRawValue             (convertGeoToMGRS(QGeoCoordinate(gpsRawInt.lat * 1e-7, gpsRawInt.lon * 1e-7)));
    count()->setRawValue            (gpsRawInt.satellitesUsed == 255 ? 0 : gpsRawInt.satellitesUsed);
    hdop()->setRawValue             (gpsRawInt.eph == UINT16_MAX ? qQNaN() : gpsRawInt.eph / 100.0);
    vdop()->setRawValue             (gpsRawInt.epv == UINT16_MAX ? qQNaN() : gpsRawInt.epv / 100.0);
    courseOverGround()->setRawValue (gpsRawInt.cog == INT16_MAX ? qQNaN() : gpsRawInt.cog / 100.0);
    status()->setRawValue           (gpsRawInt.status);
    dualAntenna()->setRawValue      (gpsRawInt.dualAntenna);
}

