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

#include "ShenHangParameterMetaData.h"
#include "QGCApplication.h"
#include "QGCLoggingCategory.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDebug>

static const char* kInvalidConverstion = "Internal Error: No support for string parameters";

QGC_LOGGING_CATEGORY(ShenHangParameterMetaDataLog, "ShenHangParameterMetaDataLog")

ShenHangParameterMetaData::ShenHangParameterMetaData(void)
{

}

/// Converts a string to a typed QVariant
///     @param string String to convert
///     @param type Type for Fact which dictates the QVariant type as well
///     @param convertOk Returned: true: conversion success, false: conversion failure
/// @return Returns the correctly type QVariant
QVariant ShenHangParameterMetaData::_stringToTypedVariant(const QString& string, FactMetaData::ValueType_t type, bool* convertOk)
{
    QVariant var(string);

    int convertTo = QVariant::Int; // keep compiler warning happy
    switch (type) {
    case FactMetaData::valueTypeUint8:
    case FactMetaData::valueTypeUint16:
    case FactMetaData::valueTypeUint32:
    case FactMetaData::valueTypeUint64:
        convertTo = QVariant::UInt;
        break;
    case FactMetaData::valueTypeInt8:
    case FactMetaData::valueTypeInt16:
    case FactMetaData::valueTypeInt32:
    case FactMetaData::valueTypeInt64:
        convertTo = QVariant::Int;
        break;
    case FactMetaData::valueTypeFloat:
        convertTo = QMetaType::Float;
        break;
    case FactMetaData::valueTypeElapsedTimeInSeconds:
    case FactMetaData::valueTypeDouble:
        convertTo = QVariant::Double;
        break;
    case FactMetaData::valueTypeString:
        qWarning() << kInvalidConverstion;
        convertTo = QVariant::String;
        break;
    case FactMetaData::valueTypeBool:
        qWarning() << kInvalidConverstion;
        convertTo = QVariant::Bool;
        break;
    case FactMetaData::valueTypeCustom:
        qWarning() << kInvalidConverstion;
        convertTo = QVariant::ByteArray;
        break;
    }
    
    *convertOk = var.convert(convertTo);
    
    return var;
}

void ShenHangParameterMetaData::loadParameterFactMetaDataFile(const QString& metaDataFile)
{
    qCDebug(ParameterManagerLog) << "ShenHangParameterMetaData::loadParameterFactMetaDataFile" << metaDataFile;

    if (_parameterMetaDataLoaded) {
        qWarning() << "Internal error: parameter meta data loaded more than once";
        return;
    }
    _parameterMetaDataLoaded = true;

    qCDebug(ShenHangParameterMetaDataLog) << "Loading parameter meta data:" << metaDataFile;

    QFile xmlFile(metaDataFile);

    if (!xmlFile.exists()) {
        qWarning() << "Internal error: metaDataFile mission" << metaDataFile;
        return;
    }
    
    if (!xmlFile.open(QIODevice::ReadOnly)) {
        qWarning() << "Internal error: Unable to open parameter file:" << metaDataFile << xmlFile.errorString();
        return;
    }
    
    QXmlStreamReader xml(xmlFile.readAll());
    xmlFile.close();
    if (xml.hasError()) {
        qWarning() << "Badly formed XML" << xml.errorString();
        return;
    }
    
    QString         factGroup;
    QString         errorString;
    FactMetaData*   metaData = nullptr;
    int             xmlState = XmlStateNone;
    
    while (!xml.atEnd()) {
        if (xml.isStartElement()) {
            QString elementName = xml.name().toString();
            
            if (elementName == "parameters") {
                if (xmlState != XmlStateNone) {
                    qWarning() << "Badly formed XML";
                    return;
                }
                xmlState = XmlStateFoundParameters;
                
            } else if (elementName == "group") {
                if (xmlState != XmlStateFoundParameters) {
                    // We didn't get a version stamp, assume older version we can't read
                    qDebug() << "Parameter version stamp not found, skipping load" << metaDataFile;
                    return;
                }
                xmlState = XmlStateFoundGroup;
                
                if (!xml.attributes().hasAttribute("name")) {
                    qWarning() << "Badly formed XML";
                    return;
                }
                factGroup = xml.attributes().value("name").toString();
                MetaDataGroup metaDataGroup;
                metaDataGroup.groupName = factGroup;
                _metaDataGroups[++_groupId] = metaDataGroup;
                _addrOffset = 0;
                qCDebug(ShenHangParameterMetaDataLog) << "Found group: " << factGroup;
                
            } else if (elementName == "entry") {
                if (xmlState != XmlStateFoundGroup) {
                    qWarning() << "Badly formed XML";
                    return;
                }
                xmlState = XmlStateFoundParameter;
                
                if (!xml.attributes().hasAttribute("name") || !xml.attributes().hasAttribute("val_type")) {
                    qWarning() << "Badly formed XML";
                    return;
                }
                
                QString strTag = xml.attributes().value("tag").toString();
                QString strName = xml.attributes().value("name").toString();
                QString strType = xml.attributes().value("val_type").toString();
                QString strDefault = xml.attributes().value("val_dflt").toString();
                QString strMax = xml.attributes().value("val_max").toString();
                QString strMin = xml.attributes().value("val_min").toString();
                QString strUnit = xml.attributes().value("val_unit").toString();
                QString strDec = xml.attributes().value("val_dec").toString();
                QString strInc = xml.attributes().value("val_inc").toString();
                QString strAuth = xml.attributes().value("auth").toString();
                
                QString category = xml.attributes().value("category").toString();
                if (category.isEmpty()) {
                    category = QStringLiteral("Standard");
                }

                bool volatileValue = false;
                bool readOnly = false;
                QString volatileStr = xml.attributes().value("volatile").toString();
                if (volatileStr.compare(QStringLiteral("true")) == 0) {
                    volatileValue = true;
                    readOnly = true;
                }
                if (!volatileValue) {
                    QString readOnlyStr = xml.attributes().value("readonly").toString();
                    if (readOnlyStr.compare(QStringLiteral("true")) == 0) {
                        readOnly = true;
                    }
                }

                qCDebug(ShenHangParameterMetaDataLog) << "Found parameter name:" << strName << " type:" << strType << " default:" << strDefault;

                // Convert type from string to FactMetaData::ValueType_t
                bool unknownType;
                FactMetaData::ValueType_t foundType = FactMetaData::stringToType(strType, unknownType);
                if (unknownType) {
                    qWarning() << "Parameter meta data with bad type:" << strType << " name:" << strName;
                    return;
                }
                
                // Now that we know type we can create meta data object and add it to the system
                metaData = new FactMetaData(foundType, this);
                _metaDataGroups[_groupId].mapMetaData[_addrOffset] = metaData;
                _addrOffset += FactMetaData::typeToSize(foundType);
                _metaDataCount++;

                _mapParameterName2FactMetaData[strName] = metaData;
                metaData->setTag(strTag);
                metaData->setName(strName);
                metaData->setCategory(category);
                metaData->setGroup(factGroup);
                metaData->setReadOnly(readOnly);
                metaData->setVolatileValue(volatileValue);
                if (xml.attributes().hasAttribute("val_dflt") && !strDefault.isEmpty()) {
                    QVariant varDefault;

                    if (metaData->convertAndValidateRaw(strDefault, false, varDefault, errorString)) {
                        metaData->setRawDefaultValue(varDefault);
                    } else {
                        qCWarning(ShenHangParameterMetaDataLog) << "Invalid default value, name:" << strName << " type:" << strType << " val_dflt:" << strDefault << " error:" << errorString;
                    }
                }
                if (xml.attributes().hasAttribute("val_max") && !strMax.isEmpty()) {
                    QVariant varMax;

                    if (metaData->convertAndValidateRaw(strMax, false, varMax, errorString)) {
                        metaData->setRawMax(varMax);
                    } else {
                        qCWarning(ShenHangParameterMetaDataLog) << "Invalid default value, name:" << strName << " type:" << strType << "foundType:" << foundType << " val_max:" << strMax << " error:" << errorString;
                    }
                }
                if (xml.attributes().hasAttribute("val_Min") && !strMin.isEmpty()) {
                    QVariant varMin;

                    if (metaData->convertAndValidateRaw(strMin, false, varMin, errorString)) {
                        metaData->setRawDefaultValue(varMin);
                    } else {
                        qCWarning(ShenHangParameterMetaDataLog) << "Invalid default value, name:" << strName << " type:" << strType << " val_Min:" << strMin << " error:" << errorString;
                    }
                }
                qCDebug(ShenHangParameterMetaDataLog) << "Unit:" << strUnit;
                metaData->setRawUnits(strUnit);


                bool    ok;
                double  decrement;
                decrement = strDec.toDouble(&ok);
                if (ok) {
                    metaData->setRawDecrement(decrement);
                } else {
                    qCWarning(ShenHangParameterMetaDataLog) << "Invalid value for decrement, name:" << metaData->name() << " decrement:" << strDec;
                }

                double  increment;
                increment = strInc.toDouble(&ok);
                if (ok) {
                    metaData->setRawIncrement(increment);
                } else {
                    qCWarning(ShenHangParameterMetaDataLog) << "Invalid value for increment, name:" << metaData->name() << " increment:" << strInc;
                }

                metaData->setAuth(strAuth);

                
            } else {
                // We should be getting meta data now
                if (xmlState != XmlStateFoundParameter) {
                    qWarning() << "Badly formed XML";
                    return;
                }

                if (metaData) {
                    if (elementName == "desc_b") {
                        QString text = xml.readElementText();
                        text = text.replace("\n", " ");
                        qCDebug(ShenHangParameterMetaDataLog) << "Short description:" << text;
                        metaData->setShortDescription(text);

                    } else if (elementName == "desc_d") {
                        QString text = xml.readElementText();
                        text = text.replace("\n", " ");
                        qCDebug(ShenHangParameterMetaDataLog) << "Long description:" << text;
                        metaData->setLongDescription(text);

                    } else if (elementName == "decimal") {
                        QString text = xml.readElementText();
                        qCDebug(ShenHangParameterMetaDataLog) << "Decimal:" << text;

                        bool convertOk;
                        QVariant varDecimals = QVariant(text).toUInt(&convertOk);
                        if (convertOk) {
                            metaData->setDecimalPlaces(varDecimals.toInt());
                        } else {
                            qCWarning(ShenHangParameterMetaDataLog) << "Invalid decimals value, name:" << metaData->name() << " type:" << metaData->type() << " decimals:" << text << " error: invalid number";
                        }

                    } else if (elementName == "reboot_required") {
                        QString text = xml.readElementText();
                        qCDebug(ShenHangParameterMetaDataLog) << "RebootRequired:" << text;
                        if (text.compare("true", Qt::CaseInsensitive) == 0) {
                            metaData->setVehicleRebootRequired(true);
                        }

                    } else if (elementName == "values") {
                        // doing nothing individual value will follow anyway. May be used for sanity checking.

                    } else if (elementName == "value") {
                        QString enumValueStr = xml.attributes().value("code").toString();
                        QString enumString = xml.readElementText();
                        qCDebug(ShenHangParameterMetaDataLog) << "parameter value:"
                                                              << "value desc:" << enumString << "code:" << enumValueStr;

                        QVariant    enumValue;
                        QString     errorString;
                        if (metaData->convertAndValidateRaw(enumValueStr, false /* validate */, enumValue, errorString)) {
                            metaData->addEnumInfo(enumString, enumValue);
                        } else {
                            qCDebug(ShenHangParameterMetaDataLog) << "Invalid enum value, name:" << metaData->name()
                                                                  << " type:" << metaData->type() << " value:" << enumValueStr
                                                                  << " error:" << errorString;
                        }
                    } else if (elementName == "boolean") {
                        QVariant    enumValue;
                        metaData->convertAndValidateRaw(1, false /* validate */, enumValue, errorString);
                        metaData->addEnumInfo(tr("Enabled"), enumValue);
                        metaData->convertAndValidateRaw(0, false /* validate */, enumValue, errorString);
                        metaData->addEnumInfo(tr("Disabled"), enumValue);

                    } else if (elementName == "bitmask") {
                        // doing nothing individual bits will follow anyway. May be used for sanity checking.

                    } else {
                        qCDebug(ShenHangParameterMetaDataLog) << "Unknown element in XML: " << elementName;
                    }
                } else {
                    qWarning() << "Internal error";
                }
            }

        } else if (xml.isEndElement()) {
            QString elementName = xml.name().toString();

            if (elementName == "entry") {
                // Done loading this parameter, validate default value
                if (metaData->defaultValueAvailable()) {
                    QVariant var;

                    if (!metaData->convertAndValidateRaw(metaData->rawDefaultValue(), false /* convertOnly */, var, errorString)) {
                        qCWarning(ShenHangParameterMetaDataLog) << "Invalid default value, name:" << metaData->name() << " type:" << metaData->type() << " default:" << metaData->rawDefaultValue() << " error:" << errorString;
                    }
                }

                // Reset for next parameter
                metaData = nullptr;
                xmlState = XmlStateFoundGroup;
            } else if (elementName == "group") {
                xmlState = XmlStateFoundParameters;
            }
        }
        xml.readNext();
    }

#ifdef GENERATE_PARAMETER_JSON
    _generateParameterJson();
#endif
}

int ShenHangParameterMetaData::getFactMetaDataCount()
{
    return _metaDataCount;
}

#ifdef GENERATE_PARAMETER_JSON
void _jsonWriteLine(QFile& file, int indent, const QString& line)
{
    while (indent--) {
        file.write("  ");
    }
    file.write(line.toLocal8Bit().constData());
    file.write("\n");
}

void ShenHangParameterMetaData::_generateParameterJson()
{
    qCDebug(ParameterManagerLog) << "ShenHangParameterMetaData::_generateParameterJson";

    int indentLevel = 0;
    QFile jsonFile(QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).absoluteFilePath("parameter.json"));
    jsonFile.open(QFile::WriteOnly | QFile::Truncate | QFile::Text);

    _jsonWriteLine(jsonFile, indentLevel++, "{");
    _jsonWriteLine(jsonFile, indentLevel, "\"version\": 1,");
    _jsonWriteLine(jsonFile, indentLevel, "\"uid\": 1,");
    _jsonWriteLine(jsonFile, indentLevel, "\"scope\": \"Firmware\",");
    _jsonWriteLine(jsonFile, indentLevel++, "\"parameters\": [");

    int keyIndex = 0;
    for (const QString& paramName: _mapParameterName2FactMetaData.keys()) {
        const FactMetaData* metaData = _mapParameterName2FactMetaData[paramName];
        _jsonWriteLine(jsonFile, indentLevel++, "{");
        _jsonWriteLine(jsonFile, indentLevel, QStringLiteral("\"name\": \"%1\",").arg(paramName));
        _jsonWriteLine(jsonFile, indentLevel, QStringLiteral("\"type\": \"%1\",").arg(metaData->typeToString(metaData->type())));
        if (!metaData->group().isEmpty()) {
            _jsonWriteLine(jsonFile, indentLevel, QStringLiteral("\"group\": \"%1\",").arg(metaData->group()));
        }
        if (!metaData->category().isEmpty()) {
            _jsonWriteLine(jsonFile, indentLevel, QStringLiteral("\"category\": \"%1\",").arg(metaData->category()));
        }
        if (!metaData->shortDescription().isEmpty()) {
            QString text = metaData->shortDescription();
            text.replace("\"", "\\\"");
            _jsonWriteLine(jsonFile, indentLevel, QStringLiteral("\"shortDescription\": \"%1\",").arg(text));
        }
        if (!metaData->longDescription().isEmpty()) {
            QString text = metaData->longDescription();
            text.replace("\"", "\\\"");
            _jsonWriteLine(jsonFile, indentLevel, QStringLiteral("\"longDescription\": \"%1\",").arg(text));
        }
        if (!metaData->rawUnits().isEmpty()) {
            _jsonWriteLine(jsonFile, indentLevel, QStringLiteral("\"units\": \"%1\",").arg(metaData->rawUnits()));
        }
        if (metaData->defaultValueAvailable()) {
            _jsonWriteLine(jsonFile, indentLevel, QStringLiteral("\"defaultValue\": %1,").arg(metaData->rawDefaultValue().toDouble()));
        }
        if (!qIsNaN(metaData->rawIncrement())) {
            _jsonWriteLine(jsonFile, indentLevel, QStringLiteral("\"increment\": %1,").arg(metaData->rawIncrement()));
        }
        if (metaData->enumValues().count()) {
            _jsonWriteLine(jsonFile, indentLevel++, "\"values\": [");
            for (int i=0; i<metaData->enumValues().count(); i++) {
                _jsonWriteLine(jsonFile, indentLevel++, "{");
                _jsonWriteLine(jsonFile, indentLevel, QStringLiteral("\"value\": %1,").arg(metaData->enumValues()[i].toDouble()));
                QString text = metaData->enumStrings()[i];
                text.replace("\"", "\\\"");
                _jsonWriteLine(jsonFile, indentLevel, QStringLiteral("\"description\": \"%1\"").arg(text));
                _jsonWriteLine(jsonFile, --indentLevel, QStringLiteral("}%1").arg(i == metaData->enumValues().count() - 1 ? "" : ","));
            }
            _jsonWriteLine(jsonFile, --indentLevel, "],");
        }
        if (metaData->vehicleRebootRequired()) {
            _jsonWriteLine(jsonFile, indentLevel, "\"rebootRequired\": true,");
        }
        if (metaData->volatileValue()) {
            _jsonWriteLine(jsonFile, indentLevel, "\"volatile\": true,");
        }
        _jsonWriteLine(jsonFile, indentLevel, QStringLiteral("\"decimalPlaces\": %1,").arg(metaData->decimalPlaces()));
        _jsonWriteLine(jsonFile, indentLevel, QStringLiteral("\"minValue\": %1,").arg(metaData->rawMin().toDouble()));
        _jsonWriteLine(jsonFile, indentLevel, QStringLiteral("\"maxValue\": %1").arg(metaData->rawMax().toDouble()));
        _jsonWriteLine(jsonFile, --indentLevel, QStringLiteral("}%1").arg(++keyIndex == _mapParameterName2FactMetaData.keys().count() ? "" : ","));
    }

    _jsonWriteLine(jsonFile, --indentLevel, "]");
    _jsonWriteLine(jsonFile, --indentLevel, "}");
}
#endif

FactMetaData* ShenHangParameterMetaData::getMetaDataForFact(uint8_t groupId, uint16_t addrOffset)
{
    if (_metaDataGroups.contains(groupId) && _metaDataGroups[groupId].mapMetaData.contains(addrOffset))
    {
//        FactMetaData* metaData = new FactMetaData(type, this);
//        _mapParameterName2FactMetaData[idGroup] = metaData;
//        _metaDataGroups[idGroup].mapMetaData.append(metaData);
        return _metaDataGroups[groupId].mapMetaData[addrOffset];
    }
    qCDebug(ShenHangParameterMetaDataLog) << "No metaData for group" << groupId << "using generic metadata addrOffset" << addrOffset;
    return nullptr;       //_mapParameterName2FactMetaData[idGroup];
}

QMap<uint8_t, MetaDataGroup> ShenHangParameterMetaData::getMapMetaDataGroup()
{
    return _metaDataGroups;
}

void ShenHangParameterMetaData::getParameterMetaDataVersionInfo(const QString& metaDataFile, int& majorVersion, int& minorVersion)
{
    QFile xmlFile(metaDataFile);
    QString errorString;

    majorVersion = -1;
    minorVersion = -1;

    if (!xmlFile.exists()) {
        _outputFileWarning(metaDataFile, QStringLiteral("Does not exist"), QString());
        return;
    }

    if (!xmlFile.open(QIODevice::ReadOnly)) {
        _outputFileWarning(metaDataFile, QStringLiteral("Unable to open file"), xmlFile.errorString());
        return;
    }

    QXmlStreamReader xml(xmlFile.readAll());
    xmlFile.close();
    if (xml.hasError()) {
        _outputFileWarning(metaDataFile, QStringLiteral("Badly formed XML"), xml.errorString());
        return;
    }

    while (!xml.atEnd() && (majorVersion == -1 || minorVersion == -1)) {
        if (xml.isStartElement()) {
            QString elementName = xml.name().toString();

            if (elementName == "parameter_version_major") {
                bool convertOk;
                QString strVersion = xml.readElementText();
                majorVersion = strVersion.toInt(&convertOk);
                if (!convertOk) {
                    _outputFileWarning(metaDataFile, QStringLiteral("Unable to convert parameter_version_major value to int"), QString());
                    return;
                }
            } else if (elementName == "parameter_version_minor") {
                bool convertOk;
                QString strVersion = xml.readElementText();
                minorVersion = strVersion.toInt(&convertOk);
                if (!convertOk) {
                    _outputFileWarning(metaDataFile, QStringLiteral("Unable to convert parameter_version_minor value to int"), QString());
                    return;
                }
            }
        }
        xml.readNext();
    }

    if (majorVersion == -1) {
        _outputFileWarning(metaDataFile, QStringLiteral("parameter_version_major is missing"), QString());
    }
    if (minorVersion == -1) {
        _outputFileWarning(metaDataFile, QStringLiteral("parameter_version_minor tag is missing"), QString());
    }
}

void ShenHangParameterMetaData::_outputFileWarning(const QString& metaDataFile, const QString& error1, const QString& error2)
{
    qWarning() << QStringLiteral("Internal Error: Parameter meta data file '%1'. %2. error: %3").arg(metaDataFile).arg(error1).arg(error2);
}
