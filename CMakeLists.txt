cmake_minimum_required(VERSION 3.20)

# O CI passa a versão da tag (-DCAMVIEWER_VERSION=1.2.3).
set(CAMVIEWER_VERSION "0.2.0" CACHE STRING "Versão do CamViewer")
project(camviewer VERSION ${CAMVIEWER_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.6 REQUIRED COMPONENTS Core Gui Widgets Network Qml Quick QuickControls2 Multimedia Svg)

# No Linux o app se integra ao KDE (notificações, bandeja e D-Bus, que é por
# onde o widget do Plasma conversa com ele). A versão portátil usa só o Qt:
# bandeja e balões do QSystemTrayIcon e um socket local. É a única opção fora
# do Linux.
if(UNIX AND NOT APPLE)
    option(CAMVIEWER_PORTABLE "Compila sem as bibliotecas do KDE e sem o widget do Plasma" OFF)
else()
    set(CAMVIEWER_PORTABLE ON)
endif()

if(CAMVIEWER_PORTABLE)
    set(CAMVIEWER_KDE OFF)
else()
    find_package(Qt6 REQUIRED COMPONENTS DBus)
    find_package(KF6Notifications REQUIRED)
    find_package(KF6StatusNotifierItem REQUIRED)
    set(CAMVIEWER_KDE ON)
endif()

add_executable(camviewer WIN32
    src/main.cpp
    src/ffmpeg.h
    src/manager.cpp
    src/manager.h
    src/motiondetector.cpp
    src/motiondetector.h
    src/onvif.cpp
    src/onvif.h
    src/app.qrc
)

if(WIN32)
    target_sources(camviewer PRIVATE packaging/windows/camviewer.rc)
    set_target_properties(camviewer PROPERTIES OUTPUT_NAME CamViewer)
endif()

target_compile_definitions(camviewer PRIVATE CAMVIEWER_VERSION="${PROJECT_VERSION}")

target_link_libraries(camviewer PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::Qml Qt6::Quick Qt6::QuickControls2
    Qt6::Multimedia Qt6::Svg
)

if(CAMVIEWER_KDE)
    target_compile_definitions(camviewer PRIVATE CAMVIEWER_KDE)
    target_link_libraries(camviewer PRIVATE Qt6::DBus KF6::Notifications KF6::StatusNotifierItem)
endif()

include(GNUInstallDirs)

install(TARGETS camviewer RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(CAMVIEWER_KDE)
    configure_file(data/org.camviewer.CamViewer.service.in org.camviewer.CamViewer.service @ONLY)
    configure_file(data/org.camviewer.CamViewer.desktop.in org.camviewer.CamViewer.desktop @ONLY)

    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.camviewer.CamViewer.desktop
            DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.camviewer.CamViewer.service
            DESTINATION ${CMAKE_INSTALL_DATADIR}/dbus-1/services)
    install(FILES data/camviewer.notifyrc
            DESTINATION ${CMAKE_INSTALL_DATADIR}/knotifications6)
    install(FILES src/icons/camviewer.svg
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)

    # Widget do Plasma. O pacote precisa ser autossuficiente, então leva uma
    # cópia do player compartilhado com o app.
    set(PLASMOID_DIR ${CMAKE_INSTALL_DATADIR}/plasma/plasmoids/org.camviewer.plasmoid)
    install(DIRECTORY plasmoid/ DESTINATION ${PLASMOID_DIR})
    install(FILES shared/CameraPlayer.qml DESTINATION ${PLASMOID_DIR}/contents/ui)

    # Faz o vídeo do widget (que roda dentro do plasmashell) usar RTSP sobre
    # TCP. Só faz sentido numa instalação de sistema; na instalação por
    # usuário o install.sh grava o equivalente em ~/.config.
    if(CMAKE_INSTALL_PREFIX STREQUAL "/usr")
        install(FILES data/camviewer-env.sh
                DESTINATION /etc/xdg/plasma-workspace/env
                RENAME camviewer.sh)
    endif()

    # Pacote .deb: cmake --build build && (cd build && cpack -G DEB)
    set(CPACK_PACKAGE_NAME camviewer)
    set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
    set(CPACK_PACKAGE_CONTACT "Gui Felipe <guiggff@gmail.com>")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Visualizador de câmeras IP para o KDE Plasma, com gravação e alertas")
    set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/Glerme/CamViewer")
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    set(CPACK_DEBIAN_PACKAGE_SECTION video)
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_PACKAGE_DEPENDS
        "ffmpeg, qml6-module-qtquick, qml6-module-qtquick-controls, qml6-module-qtquick-layouts, qml6-module-qtquick-window, qml6-module-qtquick-templates, qml6-module-qtmultimedia, qml6-module-org-kde-desktop")
    set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "plasma-workspace, qml6-module-org-kde-kirigami")
    include(CPack)
endif()
