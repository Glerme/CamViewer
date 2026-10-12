cmake_minimum_required(VERSION 3.20)
project(camviewer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.6 REQUIRED COMPONENTS Core Gui Widgets Qml Quick QuickControls2 Multimedia DBus)
find_package(KF6Notifications REQUIRED)
find_package(KF6StatusNotifierItem REQUIRED)

add_executable(camviewer
    src/main.cpp
    src/manager.cpp
    src/manager.h
    src/motiondetector.cpp
    src/motiondetector.h
    src/app.qrc
)

target_compile_definitions(camviewer PRIVATE CAMVIEWER_VERSION="${PROJECT_VERSION}")

target_link_libraries(camviewer PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Qml Qt6::Quick Qt6::QuickControls2
    Qt6::Multimedia Qt6::DBus
    KF6::Notifications KF6::StatusNotifierItem
)

include(GNUInstallDirs)

configure_file(data/org.camviewer.CamViewer.service.in org.camviewer.CamViewer.service @ONLY)
configure_file(data/org.camviewer.CamViewer.desktop.in org.camviewer.CamViewer.desktop @ONLY)

install(TARGETS camviewer RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.camviewer.CamViewer.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.camviewer.CamViewer.service
        DESTINATION ${CMAKE_INSTALL_DATADIR}/dbus-1/services)
install(FILES data/camviewer.notifyrc
        DESTINATION ${CMAKE_INSTALL_DATADIR}/knotifications6)
