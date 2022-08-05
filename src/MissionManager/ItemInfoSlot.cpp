#include <JsonHelper.h>
#include "ItemInfoSlot.h"

const char*  ItemInfoSlot::  _jsonIdBank         = "id_bank";
const char*  ItemInfoSlot::  _jsonIdWp           = "id_wp";
const char*  ItemInfoSlot::  _jsonIdInfosl       = "id_infosl";
const char*  ItemInfoSlot::  _jsonTypeInfosl     = "ty_infosl";
const char*  ItemInfoSlot::  _jsonLon            = "lon";
const char*  ItemInfoSlot::  _jsonLat            = "lat";
const char*  ItemInfoSlot::  _jsonAlt            = "alt";
const char*  ItemInfoSlot::  _jsonRadiusCh       = "radius_ch";
const char*  ItemInfoSlot::  _jsonRVGnd          = "r_v_gnd";
const char*  ItemInfoSlot::  _jsonRRocDoc        = "r_roc_doc";
const char*  ItemInfoSlot::  _jsonSwTd           = "sw_td";
const char*  ItemInfoSlot::  _jsonSwVGnd10x      = "sw_v_gnd_10x";
const char*  ItemInfoSlot::  _jsonRHdgDeg100x    = "r_hdg_deg_100x";
const char*  ItemInfoSlot::  _jsonActDept        = "act_dept";
const char*  ItemInfoSlot::  _jsonActArrvl       = "act_arrvl";
const char*  ItemInfoSlot::  _jsonSwCondSet      = "sw_cond_set";
const char*  ItemInfoSlot::  _jsonActInFlt       = "act_inflt";
const char*  ItemInfoSlot::  _jsonHgtHdgCtrlMode = "he_hdg_ctrl_mode";
const char*  ItemInfoSlot::  _jsonSwAngDeg       = "sw_ang_deg";
const char*  ItemInfoSlot::  _jsonSwTurns        = "sw_turns";
const char*  ItemInfoSlot::  _jsonGdMode         = "gd_mode";
const char*  ItemInfoSlot::  _jsonMnvrSty        = "mnvr_sty";
const char*  ItemInfoSlot::  _jsonTransSty       = "trans_sty";
const char*  ItemInfoSlot::  _jsonReserved0      = "reserved0";
const char*  ItemInfoSlot::  _jsonReserved1      = "reserved1";

//ItemInfoSlot::ItemInfoSlot(QObject *parent)
//    : QObject(parent)
//{

//}
QGC_LOGGING_CATEGORY(ItemInfoSlotLog, "ItemInfoSlotLog")

ItemInfoSlot::ItemInfoSlot(const WaypointInfoSlot &infoSlot, QObject* parent)
    : QObject(parent)
    ,_majorType("major type", FactMetaData::valueTypeUint8)
    , _minorType("minor type", FactMetaData::valueTypeUint8)
    , _infoslotCount("infoslot count", FactMetaData::valueTypeUint8)
    , _lon("lon", FactMetaData::valueTypeDouble)
    , _lat("lat", FactMetaData::valueTypeDouble)
    , _alt("alt", FactMetaData::valueTypeFloat)
    , _radiusCh("radius ch", FactMetaData::valueTypeFloat)
    , _rVGnd("r v gnd", FactMetaData::valueTypeFloat)
    , _rRocDoc("r roc doc", FactMetaData::valueTypeFloat)
    , _swTd("sw td", FactMetaData::valueTypeFloat)
    , _swVGnd("sw v gnd", FactMetaData::valueTypeFloat)
    , _rHdgDeg("r hdg deg", FactMetaData::valueTypeFloat)
    , _actDeptMainEngine("act dept main engine", FactMetaData::valueTypeUint8)

    , _actDeptDecelerate("act dept decelerate", FactMetaData::valueTypeUint8)
    , _actDeptLandingGear("act dept landing gear", FactMetaData::valueTypeUint8)
    , _actDeptPayload("act dept payload", FactMetaData::valueTypeUint8)
    , _actDeptVehicle("act dept vehicle", FactMetaData::valueTypeUint8)
    , _actDeptMnvr("act dept maneuver", FactMetaData::valueTypeUint8)
    , _actArrvlMainEngine("act arrvl main engine", FactMetaData::valueTypeUint8)

    , _actArrvlDecelerate("act arrvl decelerate", FactMetaData::valueTypeUint8)
    , _actArrvlLandingGear("act arrvl landing gear", FactMetaData::valueTypeUint8)
    , _actArrvlPayload("act arrvl payload", FactMetaData::valueTypeUint8)
    , _actArrvlVehicle("act arrvl vehicle", FactMetaData::valueTypeUint8)
    , _actArrvlMnvr("act arrvl maneuver", FactMetaData::valueTypeUint8)
    , _swCondAuto("sw cond auto switch", FactMetaData::valueTypeUint8)

    , _swCondSpeedSource("sw cond speed source", FactMetaData::valueTypeUint8)
    , _swCondHorizontalError("sw cond horizontal error", FactMetaData::valueTypeUint8)
    , _swCondHeight("sw cond height", FactMetaData::valueTypeUint8)
    , _swCondSpeed("sw cond speed", FactMetaData::valueTypeUint8)
    , _swCondLongitudinalMotion("sw cond longitudinal motion", FactMetaData::valueTypeUint8)
    , _swCondReserved("sw cond reserved", FactMetaData::valueTypeUint8)
    , _actInFlt("act inflt", FactMetaData::valueTypeUint8)

    , _hgtCtrlMode("hgt ctrl mode", FactMetaData::valueTypeUint8)
    , _hdgCtrlMode("hdg ctrl mode", FactMetaData::valueTypeUint8)
    , _swAngDeg("sw ang deg", FactMetaData::valueTypeInt8)
    , _swTurns("sw turns", FactMetaData::valueTypeUint8)
    , _gdMode("gd mode", FactMetaData::valueTypeUint8)
    , _mnvrStyEndMode("mnvr sty end mode", FactMetaData::valueTypeUint8)

    , _mnvrStyTurn("mnvr sty turn", FactMetaData::valueTypeUint8)
    , _mnvrStyReserved("mnvr sty reserved", FactMetaData::valueTypeUint8)
    , _transSty("trans sty", FactMetaData::valueTypeUint8)
    , _reserved0("reserved0", FactMetaData::valueTypeUint8)
    , _reserved1("reserved1", FactMetaData::valueTypeUint8)
    , _infoSlot(infoSlot)

{
    _setupMetaData();
    qDebug() << "ItemInfoSlot::ItemInfoSlot()" << _infoSlot.idBank << _infoSlot.idWp << _infoSlot.idInfoSlot;
}

//ItemInfoSlot::ItemInfoSlot(const ItemInfoSlot& other, QObject* parent) : QObject(parent)
//{
//    *this = other;
//    _setupMetaData();
//}

ItemInfoSlot::~ItemInfoSlot()
{
}

void ItemInfoSlot::save(QJsonObject& json) const
{
    json[_jsonIdBank]         = _infoSlot.idBank;
    json[_jsonIdWp]           = _infoSlot.idWp;
    json[_jsonIdInfosl]       = _infoSlot.idInfoSlot;
    json[_jsonTypeInfosl]     = _infoSlot.typeInfoSlot;
    json[_jsonLon]            = _infoSlot.lon;
    json[_jsonLat]            = _infoSlot.lat;
    json[_jsonAlt]            = _infoSlot.alt;
    json[_jsonRadiusCh]       = _infoSlot.radiusCh;
    json[_jsonRVGnd]          = _infoSlot.rVGnd;
    json[_jsonRRocDoc]        = _infoSlot.rRocDoc;
    json[_jsonSwTd]           = _infoSlot.swTd;
    json[_jsonSwVGnd10x]      = _infoSlot.swVGnd10x;
    json[_jsonRHdgDeg100x]    = _infoSlot.rHdgDeg100x;
    json[_jsonActDept]        = _infoSlot.actDept;
    json[_jsonActArrvl]       = _infoSlot.actArrvl;
    json[_jsonSwCondSet]      = _infoSlot.swCondSet;
    json[_jsonActInFlt]       = _infoSlot.actInflt;
    json[_jsonHgtHdgCtrlMode] = _infoSlot.hgtHdgCtrlMode;
    json[_jsonSwAngDeg]       = _infoSlot.swAngDeg;
    json[_jsonSwTurns]        = _infoSlot.swTurns;
    json[_jsonGdMode]         = _infoSlot.gdMode;
    json[_jsonMnvrSty]        = _infoSlot.mnvrSty;
    json[_jsonTransSty]       = _infoSlot.transSty;
    json[_jsonReserved0]      = _infoSlot.reserved0;
    json[_jsonReserved1]      = _infoSlot.reserved1;
}

bool ItemInfoSlot::loadInfoSlot(const QJsonObject& json, WaypointInfoSlot* infoSlot, QString& errorString)
{
    // Validate root object keys
    QList<JsonHelper::KeyValidateInfo> keyInfoList = {
        { _jsonIdBank,      QJsonValue::Double, true },
        { _jsonIdWp,        QJsonValue::Double,  true },
        { _jsonIdInfosl,    QJsonValue::Double, true },
        { _jsonTypeInfosl,  QJsonValue::Double, true },
        { _jsonLon,         QJsonValue::Double, true },
        { _jsonLat,         QJsonValue::Double, true },
        { _jsonAlt,         QJsonValue::Double, true },
    };
    if (!JsonHelper::validateKeys(json, keyInfoList, errorString)) {
        return false;
    }
    if (json[_jsonIdBank].toInt() >= 0)
        infoSlot->idBank = json[_jsonIdBank].toInt();
    else {
        errorString = QString(_jsonIdBank) + " less than 0";
        return false;
    }

    if (json[_jsonIdWp].toInt() >= 0)
        infoSlot->idWp = json[_jsonIdWp].toInt();
    else {
        errorString = QString(_jsonIdWp) + " less than 0";
        return false;
    }

    if (json[_jsonIdInfosl].toInt() >= 0)
        infoSlot->idInfoSlot = json[_jsonIdInfosl].toInt();
    else {
        errorString = QString(_jsonIdInfosl) + " less than 0";
        return false;
    }

    if (json[_jsonTypeInfosl].toInt() >= 0)
        infoSlot->typeInfoSlot = json[_jsonTypeInfosl].toInt();
    else {
        errorString = QString(_jsonTypeInfosl) + " less than 0";
        return false;
    }

    infoSlot->lon = json[_jsonLon].toDouble();
    infoSlot->lat = json[_jsonLat].toDouble();
    infoSlot->alt = json[_jsonAlt].toDouble();
    infoSlot->radiusCh = json[_jsonRadiusCh].toDouble();
    infoSlot->rVGnd = json[_jsonRVGnd].toDouble();
    infoSlot->rRocDoc = json[_jsonRRocDoc].toDouble();
    infoSlot->swTd           = json[_jsonSwTd].toDouble();
    infoSlot->swVGnd10x      = json[_jsonSwVGnd10x].toDouble();
    infoSlot->rHdgDeg100x    = json[_jsonRHdgDeg100x].toDouble();

    infoSlot->actDept        = json[_jsonActDept].toInt();
    infoSlot->actArrvl       = json[_jsonActArrvl].toInt();
    infoSlot->swCondSet      = json[_jsonSwCondSet].toInt();
    infoSlot->actInflt       = json[_jsonActInFlt].toInt();
    infoSlot->hgtHdgCtrlMode = json[_jsonHgtHdgCtrlMode].toInt();
    infoSlot->swAngDeg       = json[_jsonSwAngDeg].toInt();
    infoSlot->swTurns        = json[_jsonSwTurns].toInt();
    infoSlot->gdMode         = json[_jsonGdMode].toInt();
    infoSlot->mnvrSty        = json[_jsonMnvrSty].toInt();
    infoSlot->transSty       = json[_jsonTransSty].toInt();
    infoSlot->reserved0      = json[_jsonReserved0].toInt();
    infoSlot->reserved1      = json[_jsonReserved1].toInt();
    return true;
}

void ItemInfoSlot::updateSlotInfoData()
{
    _infoSlot.typeInfoSlot =static_cast<uint16_t>(_majorType.rawValue().toUInt()
                                              | (_minorType.rawValue().toUInt() << 4)
                                              | (_infoslotCount.rawValue().toUInt() << 12));
    _infoSlot.lon = _lon.rawValue().toDouble();
    _infoSlot.lat = _lat.rawValue().toDouble();
    _infoSlot.alt = _alt.rawValue().toFloat();
    _infoSlot.radiusCh = _radiusCh.rawValue().toFloat();
    _infoSlot.rVGnd = _rVGnd.rawValue().toFloat();
    _infoSlot.rRocDoc = _rRocDoc.rawValue().toFloat();
    _infoSlot.swTd = _swTd.rawValue().toFloat();
    _infoSlot.swVGnd10x = static_cast<uint16_t>(_swVGnd.rawValue().toFloat() * 10);
    _infoSlot.rHdgDeg100x = static_cast<int16_t>(_rHdgDeg.rawValue().toFloat() * 100);
    _infoSlot.actDept = static_cast<uint16_t>(_actDeptMainEngine.rawValue().toUInt()
                                              |  _actDeptDecelerate.rawValue().toUInt() << 2
                                              |  _actDeptLandingGear.rawValue().toUInt() << 4
                                              |  _actDeptPayload.rawValue().toUInt() << 6
                                              |  _actDeptVehicle.rawValue().toUInt() << 8
                                              |_actDeptMnvr.rawValue().toUInt() << 11);
    _infoSlot.actArrvl = static_cast<uint16_t>(_actArrvlMainEngine.rawValue().toUInt()
                                               |  _actArrvlDecelerate.rawValue().toUInt() << 2
                                               |  _actArrvlLandingGear.rawValue().toUInt() << 4
                                               |  _actArrvlPayload.rawValue().toUInt() << 6
                                               |  _actArrvlVehicle.rawValue().toUInt() << 8
                                               |_actArrvlMnvr.rawValue().toUInt() << 11);
    _infoSlot.swCondSet = static_cast<uint16_t>(_swCondAuto.rawValue().toUInt()
                                                |  _swCondSpeedSource.rawValue().toUInt() << 1
                                                |  _swCondHorizontalError.rawValue().toUInt() << 2
                                                |  _swCondHeight.rawValue().toUInt() << 4
                                                |  _swCondSpeed.rawValue().toUInt() << 6
                                                |  _swCondLongitudinalMotion.rawValue().toUInt() << 8
                                                |_swCondReserved.rawValue().toUInt() << 10);

    _infoSlot.actInflt = static_cast<uint8_t>(_actInFlt.rawValue().toUInt());
    _infoSlot.hgtHdgCtrlMode = static_cast<uint8_t>(_hgtCtrlMode.rawValue().toUInt()
                                                    |_hdgCtrlMode.rawValue().toUInt() << 4);
    _infoSlot.swAngDeg = static_cast<int8_t>(_swAngDeg.rawValue().toInt());
    _infoSlot.swTurns = static_cast<uint8_t>(_swTurns.rawValue().toInt());
    _infoSlot.gdMode = static_cast<uint8_t>(_gdMode.rawValue().toInt());
    _infoSlot.gdMode = static_cast<uint8_t>(_gdMode.rawValue().toInt());
    _infoSlot.mnvrSty = static_cast<uint8_t>(_mnvrStyEndMode.rawValue().toUInt()
                                             | _mnvrStyTurn.rawValue().toUInt() << 1
                                             |_mnvrStyReserved.rawValue().toUInt() << 2);
    _infoSlot.transSty = static_cast<uint8_t>(_transSty.rawValue().toUInt());
    _infoSlot.reserved0 = static_cast<uint8_t>(_reserved0.rawValue().toUInt());
    _infoSlot.reserved1 = static_cast<uint8_t>(_reserved1.rawValue().toUInt());
    _infoSlot.cs = ShenHangProtocol::CaculateCrc8((uint8_t*)&_infoSlot, sizeof(_infoSlot) - 1);
}

void ItemInfoSlot::setDirty(bool dirty)
{
    if (dirty != _dirty) {
        _dirty = dirty;
        emit dirtyChanged(dirty);
    }
    qDebug() << "ItemInfoSlot::setDirty" << _infoSlot.idInfoSlot << dirty;
}

void ItemInfoSlot::setIdInfoSlot(uint16_t id)
{
    if (_infoSlot.idInfoSlot == id)
        return;
    _infoSlot.idInfoSlot = id;
    emit idInfoSlotChanged();
    setDirty(true);
}

void ItemInfoSlot::setIdWaypoint(uint16_t index)
{
    if (_infoSlot.idWp == index)
        return;
    _infoSlot.idWp = index;
    emit idWaypointChanged();
    setDirty(true);
}

void ItemInfoSlot::setIdBank(uint16_t index)
{
    if (_infoSlot.idBank == index)
        return;
    _infoSlot.idBank = index;
    setDirty(true);
}

//const ItemInfoSlot& ItemInfoSlot::operator=(const ItemInfoSlot& other)
//{
//    *this = other;
//    _setupMetaData();
//    return *this;
//}

void ItemInfoSlot::_connectSignals()
{
    connect(&_majorType, &Fact::valueChanged, this, [this]() { setDirty(true); });
    connect(&_minorType, &Fact::valueChanged, this, [this]() { setDirty(true); });
    connect(&_infoslotCount, &Fact::valueChanged, this, [this]() { setDirty(true); });
    connect(&_lon, &Fact::valueChanged, this, [this]() { setDirty(true); });
    connect(&_lat, &Fact::valueChanged, this, [this]() { setDirty(true); });
    connect(&_alt, &Fact::valueChanged, this, [this]() { setDirty(true); });
    connect(&_radiusCh, &Fact::valueChanged, this, [this]() { setDirty(true); });
    connect(&_rVGnd, &Fact::valueChanged, this, [this]() { setDirty(true); });
    connect(&_rRocDoc, &Fact::valueChanged, this, [this]() { setDirty(true); });
    connect(&_swTd, &Fact::valueChanged, this, [this]() { setDirty(true); });
    connect(&_swVGnd, &Fact::valueChanged, this, [this]() { setDirty(true); });
    connect(&_rHdgDeg, &Fact::valueChanged, this, [this]() { setDirty(true); });
    connect(&_actDeptMainEngine, &Fact::valueChanged, this, [this]() { setDirty(true); });
    connect(&_actDeptDecelerate, &Fact::valueChanged, this, [this]() { setDirty(true); });
    connect(&_actDeptLandingGear, &Fact::valueChanged, this, [this]() { setDirty(true); });
    connect(&_actDeptPayload, &Fact::valueChanged, this, [this]() { setDirty(true); });
    connect(&_actDeptVehicle, &Fact::valueChanged, this, [this]() { setDirty(true); });
    connect(&_actDeptMnvr, &Fact::valueChanged, this, [this]() { setDirty(true); });
    connect(&_actArrvlMainEngine, &Fact::valueChanged, this, [this]() { setDirty(true); });
    connect(&_actArrvlDecelerate, &Fact::valueChanged, this, [this]() { setDirty(true); });
    connect(&_actArrvlLandingGear, &Fact::valueChanged, this, [this]() { setDirty(true); });
    connect(&_actArrvlPayload, &Fact::valueChanged, this, [this]() { setDirty(true); });
    connect(&_actArrvlVehicle, &Fact::valueChanged, this, [this]() { setDirty(true); });
    connect(&_actArrvlMnvr, &Fact::valueChanged, this, [this]() { setDirty(true); });
    connect(&_swCondAuto, &Fact::valueChanged, this, [this]() { setDirty(true); });
    connect(&_swCondSpeedSource, &Fact::valueChanged, this, [this]() { setDirty(true); });
    connect(&_swCondHorizontalError, &Fact::valueChanged, this, [this]() { setDirty(true); });
    connect(&_swCondHeight, &Fact::valueChanged, this, [this]() { setDirty(true); });
    connect(&_swCondSpeed, &Fact::valueChanged, this, [this]() { setDirty(true); });
    connect(&_swCondLongitudinalMotion, &Fact::valueChanged, this, [this]() { setDirty(true); });
    connect(&_swCondReserved, &Fact::valueChanged, this, [this]() { setDirty(true); });
    connect(&_actInFlt, &Fact::valueChanged, this, [this]() { setDirty(true); });
    connect(&_hgtCtrlMode, &Fact::valueChanged, this, [this]() { setDirty(true); });
    connect(&_hdgCtrlMode, &Fact::valueChanged, this, [this]() { setDirty(true); });
    connect(&_swAngDeg, &Fact::valueChanged, this, [this]() { setDirty(true); });
    connect(&_swTurns, &Fact::valueChanged, this, [this]() { setDirty(true); });
    connect(&_gdMode, &Fact::valueChanged, this, [this]() { setDirty(true); });
    connect(&_mnvrStyEndMode, &Fact::valueChanged, this, [this]() { setDirty(true); });
    connect(&_mnvrStyTurn, &Fact::valueChanged, this, [this]() { setDirty(true); });
    connect(&_mnvrStyReserved, &Fact::valueChanged, this, [this]() { setDirty(true); });
    connect(&_transSty, &Fact::valueChanged, this, [this]() { setDirty(true); });
    connect(&_reserved0, &Fact::valueChanged, this, [this]() { setDirty(true); });
    connect(&_reserved1, &Fact::valueChanged, this, [this]() { setDirty(true); });
}


void ItemInfoSlot::_setupMetaData()
{
    QStringList enumStrings;
    QVariantList enumValues;
    enumStrings << "invalid" << "main" << "supplementary";
    enumValues << 0 << 1 << 2;
    _majorType.metaData()->setEnumInfo(enumStrings, enumValues);
    _majorType.setRawValue(_infoSlot.typeInfoSlot & 0x0f);

    enumStrings.clear();
    enumValues.clear();
    enumStrings << "basic" << "dynamic";
    enumValues << 0 << 1;
    _minorType.metaData()->setEnumInfo(enumStrings, enumValues);
    _minorType.setRawValue((_infoSlot.typeInfoSlot >> 4) & 0x0ff);

    _infoslotCount.setRawValue((_infoSlot.typeInfoSlot >> 12) & 0x0f);

    _lon.metaData()->setRawUnits("deg");
    _lon.metaData()->setDecimalPlaces(7);
    _lon.setRawValue(_infoSlot.lon);
    _lat.metaData()->setRawUnits("deg");
    _lat.metaData()->setDecimalPlaces(7);
    _lat.setRawValue(_infoSlot.lat);
    _alt.metaData()->setRawUnits("m");
    _alt.metaData()->setDecimalPlaces(2);
    _alt.setRawValue(_infoSlot.alt);

    _radiusCh.metaData()->setRawUnits("m");
    _radiusCh.setRawValue(_infoSlot.radiusCh);

    _rVGnd.metaData()->setRawUnits("m/s");
    _rVGnd.metaData()->setRawMin(0);
    _rVGnd.setRawValue(_infoSlot.rVGnd);

    _rRocDoc.metaData()->setRawUnits("m/s");
    _rRocDoc.setRawValue(_infoSlot.rRocDoc);

    _swTd.metaData()->setRawMin(0);
    _swTd.setRawValue(_infoSlot.swTd);

    _swVGnd.metaData()->setDecimalPlaces(1);
    _swVGnd.setRawValue(_infoSlot.swVGnd10x / 10.f);

    _rHdgDeg.metaData()->setDecimalPlaces(2);
    _rHdgDeg.setRawValue(_infoSlot.rHdgDeg100x / 100.f);

    /******************** 离开动作字段/到达动作字段 ********************/
    enumStrings.clear();
    enumValues.clear();
    enumStrings << "keep current/no augment" << "power off" << "power on" << "augment";
    enumValues << 0 << 1 << 2 << 3;
    _actDeptMainEngine.metaData()->setEnumInfo(enumStrings, enumValues);
    _actDeptMainEngine.setRawValue(_infoSlot.actDept & 0x03);
    _actArrvlMainEngine.metaData()->setEnumInfo(enumStrings, enumValues);
    _actArrvlMainEngine.setRawValue(_infoSlot.actArrvl & 0x03);
    enumStrings.clear();
    enumValues.clear();
    enumStrings << "keep current" << "retract" << "spread" << "spread and open umbrella";
    enumValues << 0 << 1 << 2 << 3;
    _actDeptDecelerate.metaData()->setEnumInfo(enumStrings, enumValues);
    _actDeptDecelerate.setRawValue((_infoSlot.actDept >> 2) & 0x03);
    _actArrvlDecelerate.metaData()->setEnumInfo(enumStrings, enumValues);
    _actArrvlDecelerate.setRawValue((_infoSlot.actArrvl >> 2) & 0x03);
    enumStrings.clear();
    enumValues.clear();
    enumStrings << "keep current" << "retract" << "spread" << "spread and brake";
    enumValues << 0 << 1 << 2 << 3;
    _actDeptLandingGear.metaData()->setEnumInfo(enumStrings, enumValues);
    _actDeptLandingGear.setRawValue((_infoSlot.actDept >> 4) & 0x30);
    _actArrvlLandingGear.metaData()->setEnumInfo(enumStrings, enumValues);
    _actArrvlLandingGear.setRawValue((_infoSlot.actArrvl >> 4) & 0x30);
    enumStrings.clear();
    enumValues.clear();
    enumStrings << "keep current" << "open" << "close" << "trigger";
    enumValues << 0 << 1 << 2 << 3;
    _actDeptPayload.metaData()->setEnumInfo(enumStrings, enumValues);
    _actDeptPayload.setRawValue((_infoSlot.actDept >> 6) & 0x30);
    _actArrvlPayload.metaData()->setEnumInfo(enumStrings, enumValues);
    _actArrvlPayload.setRawValue((_infoSlot.actArrvl >> 6) & 0x30);
    enumStrings.clear();
    enumValues.clear();
    enumStrings << "keep current" << "reserved";
    enumValues << 0 << 1;
    _actDeptVehicle.metaData()->setEnumInfo(enumStrings, enumValues);
    _actDeptVehicle.setRawValue((_infoSlot.actDept >> 8) & 0x07);
    _actArrvlVehicle.metaData()->setEnumInfo(enumStrings, enumValues);
    _actArrvlVehicle.setRawValue((_infoSlot.actArrvl >> 8) & 0x07);
    enumStrings.clear();
    enumValues.clear();
    enumStrings << "keep current" << "reserved";
    enumValues << 0 << 1;
    _actDeptMnvr.metaData()->setEnumInfo(enumStrings, enumValues);
    _actDeptMnvr.setRawValue((_infoSlot.actDept >> 11) & 0x1f);
    _actArrvlMnvr.metaData()->setEnumInfo(enumStrings, enumValues);
    _actArrvlMnvr.setRawValue((_infoSlot.actArrvl >> 11) & 0x1f);

    /******************** 切换条件设置 ********************/
    enumStrings.clear();
    enumValues.clear();
    enumStrings << "auto off" << "auto on";
    enumValues << 0 << 1;
    _swCondAuto.metaData()->setEnumInfo(enumStrings, enumValues);
    _swCondAuto.setRawValue(_infoSlot.swCondSet & 0x01);
    enumStrings.clear();
    enumValues.clear();
    enumStrings << "ground speed" << "air speed";
    enumValues << 0 << 1;
    _swCondSpeedSource.metaData()->setEnumInfo(enumStrings, enumValues);
    _swCondSpeedSource.setRawValue((_infoSlot.swCondSet >> 1) & 0x01);
    enumStrings.clear();
    enumValues.clear();
    enumStrings << "disable" << "switch in error circle/bilateral" << "switch on enter error circle/unilateral" << "stumbling horse rope";
    enumValues << 0 << 1 << 2 << 3;
    _swCondHorizontalError.metaData()->setEnumInfo(enumStrings, enumValues);
    _swCondHorizontalError.setRawValue((_infoSlot.swCondSet >> 2) & 0x03);
    enumStrings.clear();
    enumValues.clear();
    enumStrings << "disable" << "switch when below" << "switch when above" << "switch when in threshould error";
    enumValues << 0 << 1 << 2 << 3;
    _swCondHeight.metaData()->setEnumInfo(enumStrings, enumValues);
    _swCondHeight.setRawValue((_infoSlot.swCondSet >> 4) & 0x03);
    enumStrings.clear();
    enumValues.clear();
    enumStrings << "disable" << "switch when below" << "switch when above" << "switch when in threshould error";
    enumValues << 0 << 1 << 2 << 3;
    _swCondSpeed.metaData()->setEnumInfo(enumStrings, enumValues);
    _swCondSpeed.setRawValue((_infoSlot.swCondSet >> 6) & 0x03);
    enumStrings.clear();
    enumValues.clear();
    enumStrings << "disable" << "switch when above" << "switch when below" << "switch when in threshould error";
    enumValues << 0 << 1 << 2 << 3;
    _swCondLongitudinalMotion.metaData()->setEnumInfo(enumStrings, enumValues);
    _swCondLongitudinalMotion.setRawValue((_infoSlot.swCondSet >> 8) & 0x03);
    enumStrings.clear();
    enumValues.clear();
    enumStrings << "reserved0";
    enumValues << 0;
    _swCondReserved.metaData()->setEnumInfo(enumStrings, enumValues);
    _swCondReserved.setRawValue((_infoSlot.swCondSet >> 10) & 0x3f);

    _actInFlt.setRawValue(_infoSlot.actInflt);

    enumStrings.clear();
    enumValues.clear();
    enumStrings << "disable height control" << "keep current" << "move to waypoint height" << "interpolation" << "aim" << "dynamic follow target" << "climb rate" << "climb gradient" << "glid with speed" << "reserve";
    enumValues << 0 << 1 << 2 << 3 << 4 << 5 << 6 << 7 << 8 << 9;
    _hgtCtrlMode.metaData()->setEnumInfo(enumStrings, enumValues);
    _hgtCtrlMode.setRawValue(_infoSlot.hgtHdgCtrlMode & 0x0f);
    enumStrings.clear();
    enumValues.clear();
    enumStrings << "disable heading control" << "aim at path" << "aim at waypoint" << "aim at path" << "aim at other target" << "keep waypoint heading" << "keep waypoint heading outside circle";
    enumValues << 0 << 1 << 2 << 3 << 4 << 5 << 6;
    _hdgCtrlMode.metaData()->setEnumInfo(enumStrings, enumValues);
    _hdgCtrlMode.setRawValue((_infoSlot.hgtHdgCtrlMode >> 4) & 0x0f);

    _swAngDeg.setRawValue(_infoSlot.swAngDeg);

    _swTurns.setRawValue(_infoSlot.swTurns);

    enumStrings.clear();
    enumValues.clear();
    enumStrings << "to point" << "follow line" << "loiter";
    enumValues << 0 << 1 << 2;
    _gdMode.metaData()->setEnumInfo(enumStrings, enumValues);
    _gdMode.setRawValue(_infoSlot.gdMode);

    enumStrings.clear();
    enumValues.clear();
    enumStrings << "normal" << "forward";
    enumValues << 0 << 1;
    _mnvrStyEndMode.metaData()->setEnumInfo(enumStrings, enumValues);
    _mnvrStyEndMode.setRawValue(_infoSlot.mnvrSty & 0x01);
    enumStrings.clear();
    enumValues.clear();
    enumStrings << "btt" << "stt";
    enumValues << 0 << 1;
    _mnvrStyTurn.metaData()->setEnumInfo(enumStrings, enumValues);
    _mnvrStyTurn.setRawValue((_infoSlot.mnvrSty >> 1) & 0x01);

    _mnvrStyReserved.setRawValue((_infoSlot.mnvrSty >> 2) & 0x3f);

    enumStrings.clear();
    enumValues.clear();
    enumStrings << "direct transaction" << "dubins transaction" << "reserved";
    enumValues << 0 << 1 << 2;
    _transSty.metaData()->setEnumInfo(enumStrings, enumValues);
    _transSty.setRawValue(_infoSlot.transSty);

    _reserved0.setRawValue(_infoSlot.reserved0);
    _reserved1.setRawValue(_infoSlot.reserved1);

}

WaypointInfoSlot ItemInfoSlot::infoSlot() const
{
    return _infoSlot;
}

void ItemInfoSlot::_setDirty(bool dirty)
{
    setDirty(dirty);
}


