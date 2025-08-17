cmake_minimum_required(VERSION 3.16)

project(BeatBangerAuto VERSION 1.0 LANGUAGES CXX)

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s -static -static-libgcc -static-libstdc++")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Quick)

qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(BeatBangerAuto
    main.cpp
    images/appicon.rc
)

qt_add_qml_module(BeatBangerAuto
    URI BeatBangerAuto
    VERSION 1.0
    QML_FILES
        Main.qml
        SOURCES
            memoryscanner.h
            memoryscanner.cpp
        RESOURCES
            resources.qrc
)

set_target_properties(BeatBangerAuto PROPERTIES
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(BeatBangerAuto
    PRIVATE Qt6::Quick
)

include(GNUInstallDirs)
install(TARGETS BeatBangerAuto
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
