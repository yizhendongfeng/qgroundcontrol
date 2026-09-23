# DjiBridge module - 大疆云 API JSBridge + 上云 MQTT 客户端
target_sources(${CMAKE_PROJECT_NAME}
    PRIVATE
        DjiBridgeServer.h
        DjiBridgeServer.cc
        DjiCloudClient.h
        DjiCloudClient.cc
        DjiDrcClient.h
        DjiDrcClient.cc
        DjiDrcControlMapper.h
        DjiDrcControlMapper.cc
        DjiFlightModeNames.h
        DjiCloudProperties.h
        DjiCloudProperties.cc
        DjiWsClient.h
        DjiWsClient.cc
        DjiCloudMapClient.h
        DjiCloudMapClient.cc
        DjiWaylineManager.h
        DjiWaylineManager.cc
        DjiWaylineListModel.h
        DjiWaylineListModel.cc
        djibridge.qrc
)

include(planconvert/CMakeLists.txt)

# 其他模块都有这一行（见 src/CMakeLists.txt、src/Comms/CMakeLists.txt）。本目录原来
# 靠"同目录引用"侥幸能编，但 test/DjiBridge 要跨目录 include 这些头文件，必须显式加上。
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
