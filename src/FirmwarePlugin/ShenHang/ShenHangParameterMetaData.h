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

#include "FactSystem.h"
#include "AutoPilotPlugin.h"
#include "Vehicle.h"

/// @file
///     @author Don Gagne <don@thegagnes.com>

Q_DECLARE_LOGGING_CATEGORY(ShenHangParameterMetaDataLog)

//#define GENERATE_PARAMETER_JSON


struct MetaDataGroup
{
    QMap<uint16_t, FactMetaData*> mapMetaData;
    QString groupName;
};

/// Loads and holds parameter fact meta data for ShenHang stack
class ShenHangParameterMetaData : public QObject
{
    Q_OBJECT
    
public:
    ShenHangParameterMetaData(void);

    void            loadParameterFactMetaDataFile   (const QString& metaDataFile);
    int             getFactMetaDataCount();
    FactMetaData*   getMetaDataForFact              (uint8_t idGroup, uint16_t addrOffset);
    QMap<uint8_t, MetaDataGroup> getMapMetaDataGroup();
    static void getParameterMetaDataVersionInfo(const QString& metaDataFile, int& majorVersion, int& minorVersion);

private:
    enum {
        XmlStateNone,
        XmlStateFoundParameters,
        XmlStateFoundVersion,
        XmlStateFoundGroup,
        XmlStateFoundParameter,
        XmlStateDone
    };    

    QVariant _stringToTypedVariant(const QString& string, FactMetaData::ValueType_t type, bool* convertOk);
    static void _outputFileWarning(const QString& metaDataFile, const QString& error1, const QString& error2);

#ifdef GENERATE_PARAMETER_JSON
    void _generateParameterJson();
#endif
    int                                 _metaDataCount = 0;
    bool                                _parameterMetaDataLoaded        = false;    ///< true: parameter meta data already loaded
    FactMetaData::NameToMetaDataMap_t   _mapParameterName2FactMetaData;             ///< Maps from a parameter name to FactMetaData

    uint8_t _groupId = 0;
    uint16_t _addrOffset = 0;
    QMap<uint8_t, MetaDataGroup> _metaDataGroups;
};
