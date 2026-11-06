# SPDX-License-Identifier: MIT
# Copyright (c) 2023 素曜
#
# This source file is licensed under the MIT License.
# See the LICENSE file in the project root for the full license text.

cmake_minimum_required(VERSION 3.21)

project(ImageWithUI VERSION 0.1 LANGUAGES CXX)

# 关键：必须使用动态 CRT(/MD, /MDd)，与 Qt 官方 DLL 保持一致。
# 否则（默认 /MT）会让本程序与 Qt DLL 各自持有一个堆，
# 跨 DLL 传递 std::string/QString 时会发生堆损坏（0xC0000374）。
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
#开启asan
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer")

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets concurrent)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets concurrent)


# 目录结构：
#   core/   图像底层(BMP 读写、图像算子、调试输出、公共头)
#   ui/     主窗口、主题、滑块控件、对话框
#   tools/  独立工具窗口(图片微调、放大眼睛、色彩映射)
#   tests/  自检
set(PROJECT_SOURCES
        # 入口与主窗口
        src/app/main.cpp
        src/ui/mainwindow.cpp
        include/ui/mainwindow.h
        ui/mainwindow.ui
        include/ui/theme.h
        include/ui/slider_field.h
        include/ui/dialogs.h
        src/ui/dialogs.cpp

        # 底层
        src/core/bmp_file.cpp
        include/core/bmp_file.h
        src/core/image_ops.cpp
        include/core/image_ops.h
        src/core/debug.cpp
        include/core/debug.h
        include/core/std_headers.h
        include/core/qt_headers.h

        # 工具窗口
        src/tools/retouch.cpp
        include/tools/retouch.h
        ui/retouch.ui
        src/tools/eye_zoom.cpp
        include/tools/eye_zoom.h
        ui/eye_zoom.ui
        src/tools/color_map.cpp
        include/tools/color_map.h
        ui/color_map.ui

        # 自检与资源
        src/tests/selftest.cpp
        include/tests/selftest.h
        resources/icon.qrc
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(ImageWithUI
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET ImageWithUI APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(ImageWithUI SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(ImageWithUI
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_include_directories(ImageWithUI PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/core
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/include/tools
    ${CMAKE_CURRENT_SOURCE_DIR}/include/tests)

# .ui files live in the ui/ subdir; tell AUTOUIC where to find them
set_property(TARGET ImageWithUI APPEND PROPERTY AUTOUIC_SEARCH_PATHS ${CMAKE_CURRENT_SOURCE_DIR}/ui)

# treat all sources as UTF-8 (Chinese string literals) under MSVC
target_compile_options(ImageWithUI PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

# Debug 构建可选开启 AddressSanitizer（默认关闭：与 MSVC iostream 存在交互问题）
option(IMAGEWITHUI_ASAN "Enable AddressSanitizer for Debug builds" OFF)
if (MSVC AND IMAGEWITHUI_ASAN)
    target_compile_options(ImageWithUI PRIVATE
        $<$<CONFIG:Debug>:/fsanitize=address>
        $<$<CONFIG:Debug>:/Zi>
        $<$<CONFIG:Debug>:/Od>)
    target_link_options(ImageWithUI PRIVATE
        $<$<CONFIG:Debug>:/INCREMENTAL:NO>)
endif()

target_link_libraries(ImageWithUI PRIVATE Qt${QT_VERSION_MAJOR}::Widgets  Qt${QT_VERSION_MAJOR}::Concurrent)

# auto-deploy Qt DLLs (+ MSVC runtime) next to the exe after each build;
# generator expressions pick the matching config (Debug -> *d.dll, Release -> release DLLs)
add_custom_command(TARGET ImageWithUI POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:Qt5::Core>
        $<TARGET_FILE:Qt5::Gui>
        $<TARGET_FILE:Qt5::Widgets>
        $<TARGET_FILE:Qt5::Concurrent>
        $<TARGET_FILE_DIR:ImageWithUI>
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:ImageWithUI>/platforms
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${Qt5_DIR}/../../../plugins/platforms/qwindows.dll"
        "${Qt5_DIR}/../../../plugins/platforms/qwindowsd.dll"
        $<TARGET_FILE_DIR:ImageWithUI>/platforms
    # JPEG 是 Qt 插件格式(BMP/PNG 内建)，必须把 qjpeg 插件放到 imageformats 目录，
    # 否则对话框里虽然能选 .jpg，读写都会失败
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:ImageWithUI>/imageformats
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${Qt5_DIR}/../../../plugins/imageformats/qjpeg.dll"
        "${Qt5_DIR}/../../../plugins/imageformats/qjpegd.dll"
        $<TARGET_FILE_DIR:ImageWithUI>/imageformats
)

set_target_properties(ImageWithUI PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER my.example.com
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

install(TARGETS ImageWithUI
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(ImageWithUI)
endif()
