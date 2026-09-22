# DjiBridge module - 大疆云 API JSBridge + 上云 MQTT 客户端
target_sources(${CMAKE_PROJECT_NAME}
    PRIVATE
        DjiBridgeServer.h
        DjiBridgeServer.cc
        DjiCloudClient.h
        DjiCloudClient.cc
        DjiDrcClient.h
        DjiDrcClient.cc
        DjiCloudProperties.h
        DjiCloudProperties.cc
        DjiWsClient.h
        DjiWsClient.cc
        DjiCloudMapClient.h
        DjiCloudMapClient.cc
        djibridge.qrc
)
