/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "ShenHangFirmwarePluginFactory.h"
#include "ShenHang/ShenHangFirmwarePlugin.h"

ShenHangFirmwarePluginFactory ShenHangFirmwarePluginFactory;

ShenHangFirmwarePluginFactory::ShenHangFirmwarePluginFactory(void)
    : _pluginInstance(nullptr)
{

}

QList<QGCMAVLink::FirmwareClass_t> ShenHangFirmwarePluginFactory::supportedFirmwareClasses(void) const
{
    QList<QGCMAVLink::FirmwareClass_t> list;
    list.append(QGCMAVLink::FirmwareClassShenHang);
    return list;
}

FirmwarePlugin* ShenHangFirmwarePluginFactory::firmwarePluginForAutopilot(MAV_AUTOPILOT autopilotType, MAV_TYPE /*vehicleType*/)
{
    if (autopilotType == MAV_AUTOPILOT_SHEN_HANG) {
        if (!_pluginInstance) {
            _pluginInstance = new ShenHangFirmwarePlugin();
        }
        return _pluginInstance;
    }
    return nullptr;
}
