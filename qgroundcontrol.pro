################################################################################
#
# (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
#
# QGroundControl is licensed according to the terms in the file
# COPYING.md in the root of the source code directory.
#
################################################################################

QMAKE_PROJECT_DEPTH = 0 # undocumented qmake flag to force absolute paths in makefiles

# These are disabled until proven correct
DEFINES += QGC_GST_TAISYNC_DISABLED
DEFINES += QGC_GST_MICROHARD_DISABLED
#release版本可调试
QMAKE_CXXFLAGS_RELEASE += $$QMAKE_CFLAGS_RELEASE_WITH_DEBUGINFO
#release版也将生成“.pdb”后缀的调试信息文件
QMAKE_LFLAGS_RELEASE = /INCREMENTAL:NO /DEBUG
exists($${OUT_PWD}/qgroundcontrol.pro) {
    error("You must use shadow build (e.g. mkdir build; cd build; qmake ../qgroundcontrol.pro).")
}

message(Qt version $$[QT_VERSION])

!equals(QT_MAJOR_VERSION, 5) | !greaterThan(QT_MINOR_VERSION, 10) {
    error("Unsupported Qt version, 5.11+ is required")
}

include(QGCCommon.pri)

TARGET   = QGroundControl
TEMPLATE = app
QGCROOT  = $$PWD

QML_IMPORT_PATH += $$PWD/src/QmlControls

#
# OS Specific settings
#

MacBuild {
    QMAKE_INFO_PLIST    = Custom-Info.plist
    ICON                = $${SOURCE_DIR}/resources/icons/macx.icns
    OTHER_FILES        += Custom-Info.plist
    LIBS               += -framework ApplicationServices
}

LinuxBuild {
    CONFIG  += qesp_linux_udev
}

WindowsBuild {
    RC_ICONS = resources/icons/qgroundcontrol.ico
    CONFIG += resources_big
    LIBS += -lDbgHelp   # 链接DbgHelp库
}

#
# Branding
#

QGC_APP_NAME        = "QGroundControl"
QGC_ORG_NAME        = "QGroundControl.org"
QGC_ORG_DOMAIN      = "org.qgroundcontrol"
QGC_APP_DESCRIPTION = "Open source ground control app provided by QGroundControl dev team"
QGC_APP_COPYRIGHT   = "Copyright (C) 2019 QGroundControl Development Team. All rights reserved."

WindowsBuild {
    QGC_INSTALLER_ICON          = "$$SOURCE_DIR\\windows\\WindowsQGC.ico"
    QGC_INSTALLER_HEADER_BITMAP = "$$SOURCE_DIR\\windows\\installheader.bmp"
}

# Load additional config flags from user_config.pri
exists(user_config.pri):infile(user_config.pri, CONFIG) {
    CONFIG += $$fromfile(user_config.pri, CONFIG)
    message($$sprintf("Using user-supplied additional config: '%1' specified in user_config.pri", $$fromfile(user_config.pri, CONFIG)))
}

#
# Custom Build
#
# QGC will create a "CUSTOMCLASS" object (exposed by your custom build
# and derived from QGCCorePlugin).
# This is the start of allowing custom Plugins, which will eventually use a
# more defined runtime plugin architecture and not require a QGC project
# file you would have to keep in sync with the upstream repo.
#

# This allows you to ignore the custom build even if the custom build
# is present. It's useful to run "regular" builds to make sure you didn't
# break anything.

contains (CONFIG, QGC_DISABLE_CUSTOM_BUILD) {
    message("Disable custom build override")
} else {
    exists($$PWD/custom/custom.pri) {
        message("Found custom build")
        CONFIG  += CustomBuild
        DEFINES += QGC_CUSTOM_BUILD
        # custom.pri must define:
        # CUSTOMCLASS  = YourIQGCCorePluginDerivation
        # CUSTOMHEADER = \"\\\"YourIQGCCorePluginDerivation.h\\\"\"
        include($$PWD/custom/custom.pri)
    }
}

WindowsBuild {
    # Sets up application properties
    QMAKE_TARGET_COMPANY        = "$${QGC_ORG_NAME}"
    QMAKE_TARGET_DESCRIPTION    = "$${QGC_APP_DESCRIPTION}"
    QMAKE_TARGET_COPYRIGHT      = "$${QGC_APP_COPYRIGHT}"
    QMAKE_TARGET_PRODUCT        = "$${QGC_APP_NAME}"
}

#
# Plugin configuration
#
# This allows you to build custom versions of QGC which only includes your
# specific vehicle plugin. To remove support for a firmware type completely,
# disable both the Plugin and PluginFactory entries. To include custom support
# for an existing plugin type disable PluginFactory only. Then provide you own
# implementation of FirmwarePluginFactory and use the FirmwarePlugin and
# AutoPilotPlugin classes as the base clase for your derived plugin
# implementation.

contains (CONFIG, QGC_DISABLE_ShenHang_PLUGIN) {
    message("Disable PX4 Plugin")
} else {
    CONFIG += ShenHangFirmwarePlugin
}

contains (CONFIG, QGC_DISABLE_PX4_PLUGIN_FACTORY) {
    message("Disable PX4 Plugin Factory")
} else {
    CONFIG += PX4FirmwarePluginFactory
}

# USB Camera and UVC Video Sources
contains (DEFINES, QGC_DISABLE_UVC) {
    message("Skipping support for UVC devices (manual override from command line)")
    DEFINES += QGC_DISABLE_UVC
} else:exists(user_config.pri):infile(user_config.pri, DEFINES, QGC_DISABLE_UVC) {
    message("Skipping support for UVC devices (manual override from user_config.pri)")
    DEFINES += QGC_DISABLE_UVC
} else:LinuxBuild {
    contains(QT_VERSION, 5.5.1) {
        message("Skipping support for UVC devices (conflict with Qt 5.5.1 on Ubuntu)")
        DEFINES += QGC_DISABLE_UVC
    }
}

LinuxBuild {
    CONFIG += link_pkgconfig
}

# Qt configuration

CONFIG += qt \
    thread \
    c++11

DebugBuild {
    CONFIG -= qtquickcompiler
} else {
    CONFIG += qtquickcompiler
}

contains(DEFINES, ENABLE_VERBOSE_OUTPUT) {
    message("Enable verbose compiler output (manual override from command line)")
} else:exists(user_config.pri):infile(user_config.pri, DEFINES, ENABLE_VERBOSE_OUTPUT) {
    message("Enable verbose compiler output (manual override from user_config.pri)")
} else {
    CONFIG += silent
}

QT += \
    concurrent \
    gui \
    location \
    network \
    opengl \
    positioning \
    qml \
    quick \
    quickcontrols2 \
    quickwidgets \
    sql \
    svg \
    widgets \
    xml \
    texttospeech \
    core-private

# Multimedia only used if QVC is enabled
!contains (DEFINES, QGC_DISABLE_UVC) {
    QT += \
        multimedia
}

AndroidBuild || iOSBuild {
    # Android and iOS don't unclude these
} else {
    QT += \
        serialport \
}

#  testlib is needed even in release flavor for QSignalSpy support
QT += testlib
ReleaseBuild {
    # We don't need the testlib console in release mode
    QT.testlib.CONFIG -= console
}

#
# Build-specific settings
#

DebugBuild {
!iOSBuild {
    CONFIG += console
}
}

#
# Our QtLocation "plugin"
#

include(src/QtLocationPlugin/QGCLocationPlugin.pri)

# Until pairing can be made to work cleanly on all OS it is turned off
DEFINES+=QGC_DISABLE_PAIRING

# Pairing
contains (DEFINES, QGC_DISABLE_PAIRING) {
    message("Skipping support for Pairing")
} else:exists(user_config.pri):infile(user_config.pri, DEFINES, QGC_DISABLE_PAIRING) {
    message("Skipping support for Pairing (manual override from user_config.pri)")
#} else:AndroidBuild:contains(QT_ARCH, arm64) {
#    # Haven't figured out how to get 64 bit arm OpenSLL yet which pairing requires
#    message("Skipping support for Pairing (Missing Android OpenSSL 64 bit support)")
} else {
    message("Enabling support for Pairing")
    DEFINES += QGC_ENABLE_PAIRING
}

#
# External library configuration
#

include(QGCExternalLibs.pri)

#
# Resources (custom code can replace them)
#

CustomBuild {
    exists($$PWD/custom/qgroundcontrol.qrc) {
        message("Using custom qgroundcontrol.qrc")
        RESOURCES += $$PWD/custom/qgroundcontrol.qrc
    } else {
        RESOURCES += $$PWD/qgroundcontrol.qrc
    }
    exists($$PWD/custom/qgcresources.qrc) {
        message("Using custom qgcresources.qrc")
        RESOURCES += $$PWD/custom/qgcresources.qrc
    } else {
        RESOURCES += $$PWD/qgcresources.qrc
    }
    exists($$PWD/custom/qgcimages.qrc) {
        message("Using custom qgcimages.qrc")
        RESOURCES += $$PWD/custom/qgcimages.qrc
    } else {
        RESOURCES += $$PWD/qgcimages.qrc
    }
    exists($$PWD/custom/InstrumentValueIcons.qrc) {
        message("Using custom InstrumentValueIcons.qrc")
        RESOURCES += $$PWD/custom/InstrumentValueIcons.qrc
    } else {
        RESOURCES += $$PWD/resources/InstrumentValueIcons/InstrumentValueIcons.qrc
    }
} else {
    DEFINES += QGC_APPLICATION_NAME=\"\\\"QGroundControl\\\"\"
    DEFINES += QGC_ORG_NAME=\"\\\"QGroundControl.org\\\"\"
    DEFINES += QGC_ORG_DOMAIN=\"\\\"org.qgroundcontrol\\\"\"
    RESOURCES += \
        $$PWD/qgroundcontrol.qrc \
        $$PWD/qgcresources.qrc \
        $$PWD/qgcimages.qrc \
        $$PWD/resources/InstrumentValueIcons/InstrumentValueIcons.qrc \
}

#
# Main QGroundControl portion of project file
#

DebugBuild {
    # Unit Test resources
    RESOURCES += UnitTest.qrc
}

DEPENDPATH += \
    . \
    plugins

INCLUDEPATH += .

INCLUDEPATH += \
    include/ui \
    src \
    src/api \
    src/Camera \
    src/AutoPilotPlugins \
    src/FlightDisplay \
    src/FlightMap \
    src/FlightMap/Widgets \
    src/Geo \
    src/GPS \
    src/PlanView \
    src/MissionManager \
    src/PositionManager \
    src/QmlControls \
    src/QtLocationPlugin \
    src/QtLocationPlugin/QMLControl \
    src/Settings \
    src/Vehicle \
    src/Audio \
    src/comm \
    src/input \
    src/lib/qmapcontrol \
    src/ui \
    src/ui/linechart \
    src/ui/map \
    src/ui/mapdisplay \
    src/ui/mission \
    src/ui/px4_configuration \
    src/ui/toolbar \

contains (DEFINES, QGC_ENABLE_PAIRING) {
    INCLUDEPATH += \
        src/PairingManager \
}

#
# Plugin API
#

HEADERS += \
    src/MissionManager/ItemBank.h \
    src/MissionManager/ItemWaypoint.h \
    src/MissionManager/ItemInfoSlot.h \
    src/QmlControls/QmlUnitsConversion.h \
    src/Vehicle/ShenHangVehicleData.h \
    src/api/QGCCorePlugin.h \
    src/api/QGCOptions.h \
    src/api/QGCSettings.h \
    src/api/QmlComponentInfo.h \
    src/GPS/Drivers/src/base_station.h \
    src/comm/ShenHangProtocol.h

contains (DEFINES, QGC_ENABLE_PAIRING) {
    HEADERS += \
        src/PairingManager/aes.h
}

SOURCES += \
    src/MissionManager/ItemBank.cpp \
    src/MissionManager/ItemWaypoint.cpp \
    src/MissionManager/ItemInfoSlot.cpp \
    src/api/QGCCorePlugin.cc \
    src/api/QGCOptions.cc \
    src/api/QGCSettings.cc \
    src/api/QmlComponentInfo.cc \
    src/comm/ShenHangProtocol.cc

contains (DEFINES, QGC_ENABLE_PAIRING) {
    SOURCES += \
        src/PairingManager/aes.cpp
}

#
# Unit Test specific configuration goes here (requires full debug build with all plugins)
#

DebugBuild { PX4FirmwarePlugin { PX4FirmwarePluginFactory { APMFirmwarePlugin { APMFirmwarePluginFactory { !MobileBuild {
#    DEFINES += UNITTEST_BUILD

    INCLUDEPATH += \
#        src/qgcunittest

    HEADERS += \
#        src/Audio/AudioOutputTest.h \
#        src/FactSystem/FactSystemTestBase.h \
#        src/FactSystem/FactSystemTestGeneric.h \
#        src/FactSystem/FactSystemTestPX4.h \
#        src/FactSystem/ParameterManagerTest.h \
#        src/MissionManager/CorridorScanComplexItemTest.h \
#        src/MissionManager/FWLandingPatternTest.h \
#        src/MissionManager/MissionControllerManagerTest.h \
#        src/MissionManager/MissionControllerTest.h \
#        src/MissionManager/MissionManagerTest.h \
#        src/MissionManager/MissionSettingsTest.h \
#        src/MissionManager/PlanMasterControllerTest.h \
#        src/MissionManager/QGCMapPolygonTest.h \
#        src/MissionManager/QGCMapPolylineTest.h \
#        src/qgcunittest/GeoTest.h \
#        src/qgcunittest/MavlinkLogTest.h \
#        src/qgcunittest/MultiSignalSpy.h \
#        src/qgcunittest/MultiSignalSpyV2.h \
#        src/qgcunittest/UnitTest.h \
#        src/Vehicle/RequestMessageTest.h \
#        src/Vehicle/SendMavCommandWithHandlerTest.h \
#        src/Vehicle/SendMavCommandWithSignallingTest.h \
#        src/Vehicle/VehicleLinkManagerTest.h \
        #src/qgcunittest/RadioConfigTest.h \
        #src/AnalyzeView/LogDownloadTest.h \
        #src/qgcunittest/FileDialogTest.h \
        #src/qgcunittest/FileManagerTest.h \
        #src/qgcunittest/MainWindowTest.h \
        #src/qgcunittest/MessageBoxTest.h \

    SOURCES += \
#        src/Audio/AudioOutputTest.cc \
#        src/FactSystem/FactSystemTestBase.cc \
#        src/FactSystem/FactSystemTestGeneric.cc \
#        src/FactSystem/FactSystemTestPX4.cc \
#        src/FactSystem/ParameterManagerTest.cc \
#        src/MissionManager/CameraCalcTest.cc \
#        src/MissionManager/CorridorScanComplexItemTest.cc \
#        src/MissionManager/FWLandingPatternTest.cc \
#        src/MissionManager/MissionControllerManagerTest.cc \
#        src/MissionManager/MissionControllerTest.cc \
#        src/MissionManager/MissionManagerTest.cc \
#        src/MissionManager/MissionSettingsTest.cc \
#        src/MissionManager/PlanMasterControllerTest.cc \
#        src/MissionManager/QGCMapPolygonTest.cc \
#        src/MissionManager/QGCMapPolylineTest.cc \
#        src/qgcunittest/GeoTest.cc \
#        src/qgcunittest/MavlinkLogTest.cc \
#        src/qgcunittest/MultiSignalSpy.cc \
#        src/qgcunittest/MultiSignalSpyV2.cc \
#        src/qgcunittest/UnitTest.cc \
#        src/qgcunittest/UnitTestList.cc \
#        src/Vehicle/RequestMessageTest.cc \
#        src/Vehicle/SendMavCommandWithHandlerTest.cc \
#        src/Vehicle/SendMavCommandWithSignallingTest.cc \
#        src/Vehicle/VehicleLinkManagerTest.cc \
        #src/qgcunittest/RadioConfigTest.cc \
        #src/AnalyzeView/LogDownloadTest.cc \
        #src/qgcunittest/FileDialogTest.cc \
        #src/qgcunittest/FileManagerTest.cc \
        #src/qgcunittest/MainWindowTest.cc \
        #src/qgcunittest/MessageBoxTest.cc \

} } } } } }

# Main QGC Headers and Source files

HEADERS += \
    src/Audio/AudioOutput.h \
    src/CmdLineOptParser.h \
    src/FirmwarePlugin/PX4/px4_custom_mode.h \
    src/JsonHelper.h \
    src/MissionManager/BlankPlanCreator.h \
    src/MissionManager/GeoFenceController.h \
    src/MissionManager/GeoFenceManager.h \
    src/MissionManager/MissionController.h \
    src/MissionManager/MissionManager.h \
    src/MissionManager/PlanElementController.h \
    src/MissionManager/PlanCreator.h \
    src/MissionManager/PlanManager.h \
    src/MissionManager/PlanMasterController.h \
    src/MissionManager/QGCFenceCircle.h \
    src/MissionManager/QGCFencePolygon.h \
    src/MissionManager/QGCMapCircle.h \
    src/MissionManager/QGCMapPolygon.h \
    src/MissionManager/QGCMapPolyline.h \
#    src/MissionManager/SurveyPlanCreator.h \
    src/PositionManager/PositionManager.h \
    src/PositionManager/SimulatedPosition.h \
    src/Geo/QGCGeo.h \
    src/Geo/Constants.hpp \
    src/Geo/Math.hpp \
    src/Geo/Utility.hpp \
    src/Geo/UTMUPS.hpp \
    src/Geo/MGRS.hpp \
    src/Geo/TransverseMercator.hpp \
    src/Geo/PolarStereographic.hpp \
    src/QGC.h \
    src/QGCApplication.h \
    src/QGCComboBox.h \
    src/QGCConfig.h \
    src/QGCFileDownload.h \
    src/QGCLoggingCategory.h \
    src/QGCMapPalette.h \
    src/QGCPalette.h \
    src/QGCQGeoCoordinate.h \
    src/QGCTemporaryFile.h \
    src/QGCToolbox.h \
    src/QGCZlib.h \
    src/QmlControls/AppMessages.h \
    src/QmlControls/EditPositionDialogController.h \
    src/QmlControls/FlightPathSegment.h \
    src/QmlControls/HorizontalFactValueGrid.h \
    src/QmlControls/InstrumentValueData.h \
    src/QmlControls/FactValueGrid.h \
    src/QmlControls/ParameterEditorController.h \
    src/QmlControls/QGCFileDialogController.h \
    src/QmlControls/QGCImageProvider.h \
    src/QmlControls/QGroundControlQmlGlobal.h \
    src/QmlControls/QmlObjectListModel.h \
    src/QmlControls/QGCGeoBoundingCube.h \
    src/QmlControls/RCToParamDialogController.h \
    src/QmlControls/ScreenToolsController.h \
    src/QmlControls/ToolStripAction.h \
    src/QmlControls/ToolStripActionList.h \
    src/QtLocationPlugin/QMLControl/QGCMapEngineManager.h \
    src/Settings/AppSettings.h \
    src/Settings/AutoConnectSettings.h \
    src/Settings/BrandImageSettings.h \
    src/Settings/FirmwareUpgradeSettings.h \
    src/Settings/FlightMapSettings.h \
    src/Settings/FlyViewSettings.h \
    src/Settings/OfflineMapsSettings.h \
    src/Settings/PlanViewSettings.h \
    src/Settings/RTKSettings.h \
    src/Settings/SettingsGroup.h \
    src/Settings/SettingsManager.h \
    src/Settings/UnitsSettings.h \
    src/Settings/VideoSettings.h \
    src/TerrainTile.h \
    src/Vehicle/GPSRTKFactGroup.h \
    src/Vehicle/InitialConnectStateMachine.h \
    src/Vehicle/MultiVehicleManager.h \
    src/Vehicle/StateMachine.h \
    src/Vehicle/TrajectoryPoints.h \
    src/Vehicle/Vehicle.h \
#    src/Vehicle/VehicleBatteryFactGroup.h \
    src/Vehicle/VehicleClockFactGroup.h \
    src/Vehicle/VehicleGPSFactGroup.h \
    src/Vehicle/VehicleLinkManager.h \
    src/Vehicle/VehicleSetpointFactGroup.h \
    src/comm/LinkConfiguration.h \
    src/comm/LinkInterface.h \
    src/comm/LinkManager.h \
    src/comm/QGCMAVLink.h \
    src/comm/TCPLink.h \
    src/comm/UDPLink.h \
    src/comm/UdpIODevice.h \

contains (DEFINES, QGC_ENABLE_PAIRING) {
    HEADERS += \
        src/PairingManager/PairingManager.h \
}

DebugBuild {
HEADERS += \
#    src/comm/MockLink.h \
#    src/comm/MockLinkFTP.h \
}

WindowsBuild {
    PRECOMPILED_HEADER += src/stable_headers.h
    HEADERS += src/stable_headers.h
    CONFIG -= silent
    OTHER_FILES += .appveyor.yml
}

contains(DEFINES, QGC_ENABLE_BLUETOOTH) {
    HEADERS += \
    src/comm/BluetoothLink.h \
}


!NoSerialBuild {
HEADERS += \
    src/comm/QGCSerialPortInfo.h \
    src/comm/SerialLink.h \
}

!MobileBuild {
HEADERS += \
    src/RunGuard.h \
}

SOURCES += \
    src/Audio/AudioOutput.cc \
    src/CmdLineOptParser.cc \
    src/JsonHelper.cc \
    src/MissionManager/BlankPlanCreator.cc \
    src/MissionManager/GeoFenceController.cc \
    src/MissionManager/GeoFenceManager.cc \
    src/MissionManager/MissionController.cc \
    src/MissionManager/MissionManager.cc \
    src/MissionManager/PlanElementController.cc \
    src/MissionManager/PlanCreator.cc \
    src/MissionManager/PlanManager.cc \
    src/MissionManager/PlanMasterController.cc \
    src/MissionManager/QGCFenceCircle.cc \
    src/MissionManager/QGCFencePolygon.cc \
    src/MissionManager/QGCMapCircle.cc \
    src/MissionManager/QGCMapPolygon.cc \
    src/MissionManager/QGCMapPolyline.cc \
#    src/MissionManager/SurveyPlanCreator.cc \
    src/PositionManager/PositionManager.cpp \
    src/PositionManager/SimulatedPosition.cc \
    src/Geo/QGCGeo.cc \
    src/Geo/Math.cpp \
    src/Geo/Utility.cpp \
    src/Geo/UTMUPS.cpp \
    src/Geo/MGRS.cpp \
    src/Geo/TransverseMercator.cpp \
    src/Geo/PolarStereographic.cpp \
    src/QGC.cc \
    src/QGCApplication.cc \
    src/QGCComboBox.cc \
    src/QGCFileDownload.cc \
    src/QGCLoggingCategory.cc \
    src/QGCMapPalette.cc \
    src/QGCPalette.cc \
    src/QGCQGeoCoordinate.cc \
    src/QGCTemporaryFile.cc \
    src/QGCToolbox.cc \
    src/QGCZlib.cc \
    src/QmlControls/AppMessages.cc \
    src/QmlControls/EditPositionDialogController.cc \
    src/QmlControls/FlightPathSegment.cc \
    src/QmlControls/HorizontalFactValueGrid.cc \
    src/QmlControls/InstrumentValueData.cc \
    src/QmlControls/FactValueGrid.cc \
    src/QmlControls/ParameterEditorController.cc \
    src/QmlControls/QGCFileDialogController.cc \
    src/QmlControls/QGCImageProvider.cc \
    src/QmlControls/QGroundControlQmlGlobal.cc \
    src/QmlControls/QmlObjectListModel.cc \
    src/QmlControls/QGCGeoBoundingCube.cc \
    src/QmlControls/RCToParamDialogController.cc \
    src/QmlControls/ScreenToolsController.cc \
    src/QmlControls/ToolStripAction.cc \
    src/QmlControls/ToolStripActionList.cc \
    src/QtLocationPlugin/QMLControl/QGCMapEngineManager.cc \
    src/Settings/AppSettings.cc \
    src/Settings/AutoConnectSettings.cc \
    src/Settings/BrandImageSettings.cc \
    src/Settings/FirmwareUpgradeSettings.cc \
    src/Settings/FlightMapSettings.cc \
    src/Settings/FlyViewSettings.cc \
    src/Settings/OfflineMapsSettings.cc \
    src/Settings/PlanViewSettings.cc \
    src/Settings/RTKSettings.cc \
    src/Settings/SettingsGroup.cc \
    src/Settings/SettingsManager.cc \
    src/Settings/UnitsSettings.cc \
    src/Settings/VideoSettings.cc \
    src/TerrainTile.cc\
    src/Vehicle/GPSRTKFactGroup.cc \
    src/Vehicle/InitialConnectStateMachine.cc \
    src/Vehicle/MultiVehicleManager.cc \
    src/Vehicle/StateMachine.cc \
    src/Vehicle/TrajectoryPoints.cc \
    src/Vehicle/Vehicle.cc \
#    src/Vehicle/VehicleBatteryFactGroup.cc \
    src/Vehicle/VehicleClockFactGroup.cc \
    src/Vehicle/VehicleGPSFactGroup.cc \
    src/Vehicle/VehicleLinkManager.cc \
    src/Vehicle/VehicleSetpointFactGroup.cc \
    src/comm/LinkConfiguration.cc \
    src/comm/LinkInterface.cc \
    src/comm/LinkManager.cc \
    src/comm/QGCMAVLink.cc \
    src/comm/TCPLink.cc \
    src/comm/UDPLink.cc \
    src/comm/UdpIODevice.cc \
    src/main.cc \

contains (DEFINES, QGC_ENABLE_PAIRING) {
    SOURCES += \
        src/PairingManager/PairingManager.cc \
}

DebugBuild {
SOURCES += \
#    src/comm/MockLink.cc \
#    src/comm/MockLinkFTP.cc \
}

!NoSerialBuild {
SOURCES += \
    src/comm/QGCSerialPortInfo.cc \
    src/comm/SerialLink.cc \
}

contains(DEFINES, QGC_ENABLE_BLUETOOTH) {
    SOURCES += \
    src/comm/BluetoothLink.cc \
}

!MobileBuild {
SOURCES += \
    src/RunGuard.cc \
}

#
# Firmware Plugin Support
#

INCLUDEPATH += \
    src/AutoPilotPlugins/Common \
    src/FirmwarePlugin \
    src/VehicleSetup \

HEADERS+= \
    src/AutoPilotPlugins/AutoPilotPlugin.h \
    src/AutoPilotPlugins/Generic/GenericAutoPilotPlugin.h \
    src/FirmwarePlugin/CameraMetaData.h \
    src/FirmwarePlugin/FirmwarePlugin.h \
    src/FirmwarePlugin/FirmwarePluginManager.h \
    src/VehicleSetup/VehicleComponent.h \

!MobileBuild { !NoSerialBuild {
    HEADERS += \
        src/VehicleSetup/Bootloader.h \
        src/VehicleSetup/FirmwareImage.h \
        src/VehicleSetup/FirmwareUpgradeController.h \
        src/VehicleSetup/PX4FirmwareUpgradeThread.h \
}}

SOURCES += \
    src/AutoPilotPlugins/AutoPilotPlugin.cc \
    src/AutoPilotPlugins/Generic/GenericAutoPilotPlugin.cc \
    src/FirmwarePlugin/CameraMetaData.cc \
    src/FirmwarePlugin/FirmwarePlugin.cc \
    src/FirmwarePlugin/FirmwarePluginManager.cc \
    src/VehicleSetup/VehicleComponent.cc \

!MobileBuild { !NoSerialBuild {
    SOURCES += \
        src/VehicleSetup/Bootloader.cc \
        src/VehicleSetup/FirmwareImage.cc \
        src/VehicleSetup/FirmwareUpgradeController.cc \
        src/VehicleSetup/PX4FirmwareUpgradeThread.cc \
}}

#ShenHang FirmwarePlugin
ShenHangFirmwarePlugin {
    RESOURCES *= src/FirmwarePlugin/ShenHang/ShenHangResources.qrc

    INCLUDEPATH += \
        src/AutoPilotPlugins/ShenHang \
        src/FirmwarePlugin/ShenHang \

    HEADERS += \
        src/AutoPilotPlugins/ShenHang/ShenHangAutoPilotPlugin.h \
        src/FirmwarePlugin/ShenHang/ShenHangFirmwarePlugin.h \
        src/FirmwarePlugin/ShenHang/ShenHangFirmwarePluginFactory.h \
        src/FirmwarePlugin/ShenHang/ShenHangParameterMetaData.h \

    SOURCES += \
        src/AutoPilotPlugins/ShenHang/ShenHangAutoPilotPlugin.cc \
        src/FirmwarePlugin/ShenHang/ShenHangFirmwarePlugin.cc \
        src/FirmwarePlugin/ShenHang/ShenHangFirmwarePluginFactory.cc \
        src/FirmwarePlugin/ShenHang/ShenHangParameterMetaData.cc \
}

# Fact System code

INCLUDEPATH += \
    src/FactSystem \
    src/FactSystem/FactControls \

HEADERS += \
    src/FactSystem/Fact.h \
    src/FactSystem/FactControls/FactPanelController.h \
    src/FactSystem/FactGroup.h \
    src/FactSystem/FactMetaData.h \
    src/FactSystem/FactSystem.h \
    src/FactSystem/FactValueSliderListModel.h \
    src/FactSystem/ParameterManager.h \
    src/FactSystem/SettingsFact.h \

SOURCES += \
    src/FactSystem/Fact.cc \
    src/FactSystem/FactControls/FactPanelController.cc \
    src/FactSystem/FactGroup.cc \
    src/FactSystem/FactMetaData.cc \
    src/FactSystem/FactSystem.cc \
    src/FactSystem/FactValueSliderListModel.cc \
    src/FactSystem/ParameterManager.cc \
    src/FactSystem/SettingsFact.cc \

#-------------------------------------------------------------------------------------
# Video Streaming

INCLUDEPATH += \
    src/VideoManager

HEADERS += \
    src/VideoManager/SubtitleWriter.h \
    src/VideoManager/VideoManager.h

SOURCES += \
    src/VideoManager/SubtitleWriter.cc \
    src/VideoManager/VideoManager.cc

contains (CONFIG, DISABLE_VIDEOSTREAMING) {
    message("Skipping support for video streaming (manual override from command line)")
# Otherwise the user can still disable this feature in the user_config.pri file.
} else:exists(user_config.pri):infile(user_config.pri, DEFINES, DISABLE_VIDEOSTREAMING) {
    message("Skipping support for video streaming (manual override from user_config.pri)")
} else {
    include(src/VideoReceiver/VideoReceiver.pri)
}

!VideoEnabled {
    INCLUDEPATH += \
        src/VideoReceiver

    HEADERS += \
        src/VideoManager/GLVideoItemStub.h \
        src/VideoReceiver/VideoReceiver.h

    SOURCES += \
        src/VideoManager/GLVideoItemStub.cc
}

#-------------------------------------------------------------------------------------
#
# Localization
#

TRANSLATIONS += $$files($$PWD/translations/qgc_*.ts)
CONFIG+=lrelease embed_translations

#-------------------------------------------------------------------------------------
#
# Post link configuration
#

contains (CONFIG, QGC_DISABLE_BUILD_SETUP) {
    message("Disable standard build setup")
} else {
    include(QGCPostLinkCommon.pri)
}

#
# Installer targets
#

contains (CONFIG, QGC_DISABLE_INSTALLER_SETUP) {
    message("Disable standard installer setup")
} else {
    include(QGCPostLinkInstaller.pri)
}

DISTFILES += \
    src/AutoPilotPlugins/ShenHang/CMakeLists.txt \
    src/FirmwarePlugin/ShenHang/ShenHangParameterFactMetaData.xml \
    src/FirmwarePlugin/ShenHang/V1.4.OfflineEditing.params \
    src/FirmwarePlugin/ShenHang/cfg_prm.xml \
    src/QmlControls/QGroundControl/Specific/qmldir
