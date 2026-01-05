cmake_minimum_required(VERSION 3.20)
project(VideoSurveillance LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(FFMPEG_ROOT
    "${CMAKE_SOURCE_DIR}/third_party/ffmpeg-msvc"
)

set(FFMPEG_INCLUDE_DIR
    "${FFMPEG_ROOT}/include"
)

set(FFMPEG_LIB_DIR
    "${FFMPEG_ROOT}/lib"
)

set(CMAKE_PREFIX_PATH "C:/Qt/6.10.1/msvc2022_64")

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(VideoSurveillance
    app/main.cpp
    core/FfmpegTest.cpp
    core/DotEnv.h
    core/DotEnv.cpp
    core/CameraStream.h
    core/CameraStream.cpp
    ui/MainWindow.cpp
    ui/MainWindow.h
)

target_include_directories(VideoSurveillance
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${FFMPEG_INCLUDE_DIR}
)

target_link_libraries(VideoSurveillance
    Qt6::Widgets

    "${FFMPEG_LIB_DIR}/avformat.lib"
    "${FFMPEG_LIB_DIR}/avcodec.lib"
    "${FFMPEG_LIB_DIR}/avutil.lib"
    "${FFMPEG_LIB_DIR}/avdevice.lib"
    "${FFMPEG_LIB_DIR}/avfilter.lib"
    "${FFMPEG_LIB_DIR}/swscale.lib"
    "${FFMPEG_LIB_DIR}/swresample.lib"
)