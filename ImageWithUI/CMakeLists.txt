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


set(PROJECT_SOURCES
        src/main.cpp
        src/mainwindow.cpp
        src/selftest.cpp
        include/mainwindow.h
        ui/mainwindow.ui
        include/BMPFile.h
        src/BMPFile.cpp
        include/Debug.h
        src/Debug.cpp
        include/stdcpp.h
        include/QTHeader.h
        include/Theme.h
        include/Function.h
        src/Function.cpp
        #create a new ui include cpp and head file
        include/blendwindow.h
        src/blendwindow.cpp
        ui/blendwindow.ui
        include/eye.h
        src/eye.cpp
        ui/eye.ui
        include/interpolation.h
        src/interpolation.cpp

        include/face.h
        src/face.cpp
        ui/face.ui
        resources/icon.qrc
        include/colormap.h
        src/colormap.cpp
        ui/colormap.ui

        include/editdialogs.h
        src/editdialogs.cpp


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

target_include_directories(ImageWithUI PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

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
