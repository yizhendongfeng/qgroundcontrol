/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/


/// @file
///     @author Don Gagne <don@thegagnes.com>

#include "ShenHangAirframeLoader.h"
#include "QGCApplication.h"
#include "QGCLoggingCategory.h"
#include "AirframeComponentAirframes.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDebug>

QGC_LOGGING_CATEGORY(ShenHangAirframeLoaderLog, "ShenHangAirframeLoaderLog")

bool ShenHangAirframeLoader::_airframeMetaDataLoaded = false;

ShenHangAirframeLoader::ShenHangAirframeLoader(AutoPilotPlugin* autopilot, UASInterface* uas, QObject* parent)
{
    Q_UNUSED(autopilot);
    Q_UNUSED(uas);
    Q_UNUSED(parent);
}

QString ShenHangAirframeLoader::aiframeMetaDataFile(void)
{
    QSettings settings;
    QDir parameterDir = QFileInfo(settings.fileName()).dir();
    return parameterDir.filePath("ShenHangAirframeFactMetaData.xml");
}

/// Load Airframe Fact meta data
///
/// The meta data comes from firmware airframes.xml file.
void ShenHangAirframeLoader::loadAirframeMetaData(void)
{
    if (_airframeMetaDataLoaded) {
        return;
    }

    qCDebug(ShenHangAirframeLoaderLog) << "Loading ShenHang airframe fact meta data";

    if (AirframeComponentAirframes::get().count() != 0) {
        qCWarning(ShenHangAirframeLoaderLog) << "Internal error";
        return;
    }

    QString airframeFilename;

    // We want unit test builds to always use the resource based meta data to provide repeatable results
    if (!qgcApp()->runningUnitTests()) {
        // First look for meta data that comes from a firmware download. Fall back to resource if not there.
        airframeFilename = aiframeMetaDataFile();
    }
    if (airframeFilename.isEmpty() || !QFile(airframeFilename).exists()) {
        airframeFilename = ":/AutoPilotPlugins/ShenHang/AirframeFactMetaData.xml";
    }

    qCDebug(ShenHangAirframeLoaderLog) << "Loading meta data file:" << airframeFilename;

    QFile xmlFile(airframeFilename);
    if (!xmlFile.exists()) {
        qCWarning(ShenHangAirframeLoaderLog) << "Internal error";
        return;
    }

    bool success = xmlFile.open(QIODevice::ReadOnly);

    if (!success) {
        qCWarning(ShenHangAirframeLoaderLog) << "Failed opening airframe XML";
        return;
    }

    QXmlStreamReader xml(xmlFile.readAll());
    xmlFile.close();
    if (xml.hasError()) {
        qCWarning(ShenHangAirframeLoaderLog) << "Badly formed XML" << xml.errorString();
        return;
    }

    QString         airframeGroup;
    QString         image;
    int             xmlState = XmlStateNone;

    while (!xml.atEnd()) {
        if (xml.isStartElement()) {
            QString elementName = xml.name().toString();

            if (elementName == "airframes") {
                if (xmlState != XmlStateNone) {
                    qCWarning(ShenHangAirframeLoaderLog) << "Badly formed XML";
                    return;
                }
                xmlState = XmlStateFoundAirframes;

            } else if (elementName == "version") {
                if (xmlState != XmlStateFoundAirframes) {
                    qCWarning(ShenHangAirframeLoaderLog) << "Badly formed XML";
                    return;
                }
                xmlState = XmlStateFoundVersion;

                bool convertOk;
                QString strVersion = xml.readElementText();
                int intVersion = strVersion.toInt(&convertOk);
                if (!convertOk) {
                    qCWarning(ShenHangAirframeLoaderLog) << "Badly formed XML";
                    return;
                }
                if (intVersion < 1) {
                    // We can't read these old files
                    qDebug() << "Airframe version stamp too old, skipping load. Found:" << intVersion << "Want: 3 File:" << airframeFilename;
                    return;
                }

            } else if (elementName == "airframe_version_major") {
                // Just skip over for now
            } else if (elementName == "airframe_version_minor") {
                // Just skip over for now

            } else if (elementName == "airframe_group") {
                if (xmlState != XmlStateFoundVersion) {
                    // We didn't get a version stamp, assume older version we can't read
                    qDebug() << "Parameter version stamp not found, skipping load" << airframeFilename;
                    return;
                }
                xmlState = XmlStateFoundGroup;

                if (!xml.attributes().hasAttribute("name") || !xml.attributes().hasAttribute("image")) {
                    qCWarning(ShenHangAirframeLoaderLog) << "Badly formed XML";
                    return;
                }
                airframeGroup = xml.attributes().value("name").toString();
                image = xml.attributes().value("image").toString();
                qCDebug(ShenHangAirframeLoaderLog) << "Found group: " << airframeGroup;

            } else if (elementName == "airframe") {
                if (xmlState != XmlStateFoundGroup) {
                    qCWarning(ShenHangAirframeLoaderLog) << "Badly formed XML";
                    return;
                }
                xmlState = XmlStateFoundAirframe;

                if (!xml.attributes().hasAttribute("name") || !xml.attributes().hasAttribute("id")) {
                    qCWarning(ShenHangAirframeLoaderLog) << "Badly formed XML";
                    return;
                }

                QString name = xml.attributes().value("name").toString();
                QString id = xml.attributes().value("id").toString();

                qCDebug(ShenHangAirframeLoaderLog) << "Found airframe name:" << name << " type:" << airframeGroup << " id:" << id;

                // Now that we know type we can airframe meta data object and add it to the system
                AirframeComponentAirframes::insert(airframeGroup, image, name, id.toInt());

            } else {
                // We should be getting meta data now
                if (xmlState != XmlStateFoundAirframe) {
                    qCWarning(ShenHangAirframeLoaderLog) << "Badly formed XML";
                    return;
                }
            }
        } else if (xml.isEndElement()) {
            QString elementName = xml.name().toString();

            if (elementName == "airframe") {
                // Reset for next airframe
                xmlState = XmlStateFoundGroup;
            } else if (elementName == "airframe_group") {
                xmlState = XmlStateFoundVersion;
            } else if (elementName == "airframes") {
                xmlState = XmlStateFoundAirframes;
            }
        }
        xml.readNext();
    }

    _airframeMetaDataLoaded = true;
}
