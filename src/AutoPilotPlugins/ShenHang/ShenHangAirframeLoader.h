/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/


#ifndef ShenHangAIRFRAMELOADER_H
#define ShenHangAIRFRAMELOADER_H

#include <QObject>
#include <QMap>
#include <QXmlStreamReader>
#include <QLoggingCategory>

#include "ParameterManager.h"
#include "FactSystem.h"
#include "UASInterface.h"
#include "AutoPilotPlugin.h"

/// @file ShenHangAirframeLoader.h
///     @author Lorenz Meier <lm@qgroundcontrol.org>

Q_DECLARE_LOGGING_CATEGORY(ShenHangAirframeLoaderLog)

/// Collection of Parameter Facts for ShenHang AutoPilot

class ShenHangAirframeLoader : QObject
{
    Q_OBJECT

public:
    /// @param uas Uas which this set of facts is associated with
    ShenHangAirframeLoader(AutoPilotPlugin* autpilot,UASInterface* uas, QObject* parent = nullptr);

    static void loadAirframeMetaData(void);

    /// @return Location of ShenHang airframe fact meta data file
    static QString aiframeMetaDataFile(void);

private:
    enum {
        XmlStateNone,
        XmlStateFoundAirframes,
        XmlStateFoundVersion,
        XmlStateFoundGroup,
        XmlStateFoundAirframe,
        XmlStateDone
    };

    static bool _airframeMetaDataLoaded;   ///< true: parameter meta data already loaded
    static QMap<QString, FactMetaData*> _mapParameterName2FactMetaData; ///< Maps from a parameter name to FactMetaData
};

#endif // ShenHangAIRFRAMELOADER_H
