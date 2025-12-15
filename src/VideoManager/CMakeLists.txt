target_sources(${CMAKE_PROJECT_NAME}
    PRIVATE
        SubtitleWriter.cc
        SubtitleWriter.h
        VideoManager.cc
        VideoManager.h
        MediaFileModel.cc
        MediaFileModel.h
        MediaManager.cc
        MediaManager.h
        MediaDownload.h
        MediaDownload.cpp
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(VideoReceiver)
