cmake_minimum_required(VERSION 3.16)

project(BeatBangerAuto VERSION 1.0 LANGUAGES CXX)

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s -static -static-libgcc -static-libstdc++")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Quick Widgets)

qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(BeatBangerAuto
    src/main.cpp
    resources/appicon.rc
)

qt_add_qml_module(BeatBangerAuto
    URI BeatBangerAuto
    VERSION 1.0
    QML_FILES
        qml/Main.qml
    SOURCES
        src/core/appcontroller.h
        src/core/appcontroller.cpp
        src/core/memoryscanner.h
        src/core/memoryscanner.cpp
        src/core/patternmatcher.h
        src/core/patternmatcher.cpp
        src/platform/windows/processmanager.h
        src/platform/windows/processmanager.cpp
        src/utils/constants.h
        src/utils/configmanager.h
        src/utils/configmanager.cpp
        src/utils/updatemanager.h
        src/utils/updatemanager.cpp
    RESOURCES
        resources/resources.qrc
)

add_custom_command(TARGET BeatBangerAuto POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/config.json"
        $<TARGET_FILE_DIR:BeatBangerAuto>
)

set_target_properties(BeatBangerAuto PROPERTIES
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(BeatBangerAuto
    PRIVATE Qt6::Quick
    PRIVATE Qt6::Widgets
)

include(GNUInstallDirs)
install(TARGETS BeatBangerAuto
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
