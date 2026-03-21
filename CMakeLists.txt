cmake_minimum_required(VERSION 3.20)
project(VideoSurveillance LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# CMake autogen (moc/uic/rcc) fails to spawn moc.exe on this system.
# Use explicit qt6_wrap_cpp instead of cmake_autogen.
set(CMAKE_AUTOMOC OFF)
set(CMAKE_AUTORCC OFF)
set(CMAKE_AUTOUIC OFF)

set(FFMPEG_ROOT
    "${CMAKE_SOURCE_DIR}/third_party/ffmpeg-msvc"
    CACHE PATH "Path to FFmpeg root (bin/lib/include)"
)

set(FFMPEG_INCLUDE_DIR
    "${FFMPEG_ROOT}/include"
)

set(FFMPEG_LIB_DIR
    "${FFMPEG_ROOT}/lib"
)

set(QT_ROOT "" CACHE PATH "Path to Qt root (e.g. C:/Qt/6.10.1/msvc2022_64)")
set(_qt_root_hint "")
if (QT_ROOT)
    set(_qt_root_hint "${QT_ROOT}")
elseif(DEFINED ENV{QT_ROOT})
    set(_qt_root_hint "$ENV{QT_ROOT}")
elseif(DEFINED ENV{QTDIR})
    set(_qt_root_hint "$ENV{QTDIR}")
elseif(WIN32)
    file(GLOB _qt_msvc_roots LIST_DIRECTORIES true "C:/Qt/*/msvc2022_64")
    if (_qt_msvc_roots)
        list(SORT _qt_msvc_roots COMPARE NATURAL ORDER DESCENDING)
        list(GET _qt_msvc_roots 0 _qt_root_hint)
        message(STATUS "Auto-detected Qt root: ${_qt_root_hint}")
    endif()
endif()

if (_qt_root_hint)
    list(PREPEND CMAKE_PREFIX_PATH "${_qt_root_hint}")
    if (NOT QT_ROOT)
        set(QT_ROOT "${_qt_root_hint}" CACHE PATH "Path to Qt root (e.g. C:/Qt/6.10.1/msvc2022_64)" FORCE)
    endif()
endif()

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

set_target_properties(VideoSurveillance PROPERTIES
    AUTOMOC OFF
    AUTOUIC OFF
    AUTORCC OFF
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

# Explicit moc sources to bypass cmake_autogen
qt6_wrap_cpp(VideoSurveillance_moc
    TARGET VideoSurveillance
    core/CameraStream.h
    ui/MainWindow.h
)
target_sources(VideoSurveillance PRIVATE ${VideoSurveillance_moc})

if (WIN32)
    option(USE_WINDEPLOYQT "Use windeployqt to deploy Qt runtime" OFF)

    set(_qt_root "")
    if (QT_ROOT)
        set(_qt_root "${QT_ROOT}")
    elseif(DEFINED Qt6_DIR)
        get_filename_component(_qt_root "${Qt6_DIR}/../../.." ABSOLUTE)
    endif()

    if (_qt_root)
        set(_windeployqt "${_qt_root}/bin/windeployqt.exe")
    endif()

    if (USE_WINDEPLOYQT AND EXISTS "${_windeployqt}")
        add_custom_command(TARGET VideoSurveillance POST_BUILD
            COMMAND "${_windeployqt}" --no-translations "$<TARGET_FILE:VideoSurveillance>"
            WORKING_DIRECTORY "${_qt_root}/bin"
            COMMENT "Deploying Qt runtime (windeployqt)..."
        )
    elseif(_qt_root)
        file(GLOB QT_RUNTIME_DLLS
            "${_qt_root}/bin/Qt6*.dll"
            "${_qt_root}/bin/icu*.dll"
            "${_qt_root}/bin/d3dcompiler_47.dll"
            "${_qt_root}/bin/opengl32sw.dll"
        )

        if (QT_RUNTIME_DLLS)
            add_custom_command(TARGET VideoSurveillance POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${QT_RUNTIME_DLLS}
                    $<TARGET_FILE_DIR:VideoSurveillance>
                COMMENT "Copying Qt runtime DLLs..."
            )
        else()
            message(WARNING "No Qt runtime DLLs found under ${_qt_root}/bin")
        endif()

        file(GLOB QT_PLATFORM_DLLS
            "${_qt_root}/plugins/platforms/*.dll"
        )

        if (QT_PLATFORM_DLLS)
            add_custom_command(TARGET VideoSurveillance POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                    "$<TARGET_FILE_DIR:VideoSurveillance>/platforms"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${QT_PLATFORM_DLLS}
                    "$<TARGET_FILE_DIR:VideoSurveillance>/platforms"
                COMMENT "Copying Qt platform plugins..."
            )
        else()
            message(WARNING "No Qt platform plugins found under ${_qt_root}/plugins/platforms")
        endif()

        file(GLOB QT_STYLE_DLLS
            "${_qt_root}/plugins/styles/*.dll"
        )

        if (QT_STYLE_DLLS)
            add_custom_command(TARGET VideoSurveillance POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                    "$<TARGET_FILE_DIR:VideoSurveillance>/styles"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${QT_STYLE_DLLS}
                    "$<TARGET_FILE_DIR:VideoSurveillance>/styles"
                COMMENT "Copying Qt style plugins..."
            )
        endif()
    else()
        message(WARNING "Qt root not set; Qt runtime won't be deployed automatically.")
    endif()

    file(GLOB FFMPEG_DLLS "${FFMPEG_ROOT}/bin/*.dll")
    if (FFMPEG_DLLS)
        add_custom_command(TARGET VideoSurveillance POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${FFMPEG_DLLS}
                $<TARGET_FILE_DIR:VideoSurveillance>
            COMMENT "Copying FFmpeg DLLs..."
        )
    else()
        message(WARNING "No FFmpeg DLLs found under ${FFMPEG_ROOT}/bin")
    endif()
endif()
