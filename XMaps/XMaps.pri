include($$PWD/Io.pri)

win32 {
    copy(msvc2019_64/plugins/geoservices, geoservices)
    CONFIG(debug, debug|release): deleteFile(geoservices/XMaps.dll)
    CONFIG(release, debug|release): deleteFile(geoservices/XMapsd.dll)
}

else: unix:!android: copy(gcc_64/plugins/geoservices, geoservices)

ANDROID_EXTRA_LIBS += $$[QT_INSTALL_LIBS]/libcrypto_1_1.so $$[QT_INSTALL_LIBS]/libssl_1_1.so
ANDROID_EXTRA_LIBS += $$PWD/android/plugins/geoservices/libplugins_geoservices_XMaps_armeabi-v7a.so

RESOURCES += $$PWD/XMaps.qrc
