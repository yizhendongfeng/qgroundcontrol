# planconvert - DJI WPML(.kmz) 与 QGC plan(.plan) 双向转换器
# 独立转换器项目并入主工程的转换逻辑；本文件由 DjiBridge/CMakeLists.txt include()。
target_sources(${CMAKE_PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/waypointmodel.h
        ${CMAKE_CURRENT_LIST_DIR}/waypointmodel.cpp
        ${CMAKE_CURRENT_LIST_DIR}/mavlinkdefs.h
        ${CMAKE_CURRENT_LIST_DIR}/mavlinkdefs.cpp
        ${CMAKE_CURRENT_LIST_DIR}/qgcplanparser.h
        ${CMAKE_CURRENT_LIST_DIR}/qgcplanparser.cpp
        ${CMAKE_CURRENT_LIST_DIR}/qgcplanwriter.h
        ${CMAKE_CURRENT_LIST_DIR}/qgcplanwriter.cpp
        ${CMAKE_CURRENT_LIST_DIR}/djiwpmlparser.h
        ${CMAKE_CURRENT_LIST_DIR}/djiwpmlparser.cpp
        ${CMAKE_CURRENT_LIST_DIR}/djiwpmlwriter.h
        ${CMAKE_CURRENT_LIST_DIR}/djiwpmlwriter.cpp
        ${CMAKE_CURRENT_LIST_DIR}/planconverter.h
        ${CMAKE_CURRENT_LIST_DIR}/planconverter.cpp
        ${CMAKE_CURRENT_LIST_DIR}/ziputil.h
        ${CMAKE_CURRENT_LIST_DIR}/ziputil.cpp
)
