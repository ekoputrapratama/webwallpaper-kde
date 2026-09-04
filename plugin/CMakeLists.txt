cmake_minimum_required(VERSION 3.16)
project(webwallpaper-wallpaper VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

option(INSTALL_USER_LOCAL "Install the QML package to the user plasma wallpapers dir (package only, no root needed)" OFF)
set(QML_INSTALL_DIR "" CACHE PATH "Override the Qt6 QML module install directory (auto-detected if empty)")

set(PLUGIN_ID webwallpaper)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Qml Quick Gui QmlModels)
find_package(KF6WindowSystem REQUIRED)
find_package(KWayland REQUIRED)

# ---------------------------------------------------------------------------
# C++ QML module "WebWallpaper" exposing ThemeModel + WindowMonitor.
# Installed to the Qt QML module dir so plasmashell can import it.
# ---------------------------------------------------------------------------
qt_add_qml_module(webwallpaper_thememodule
    URI WebWallpaper
    VERSION 1.0
    OUTPUT_DIRECTORY "WebWallpaper"
    SOURCES
        src/thememodel.h
        src/thememodel.cpp
        src/windowmonitor.h
        src/windowmonitor.cpp
)

target_link_libraries(webwallpaper_thememodule PRIVATE KF6::WindowSystem Plasma::KWaylandClient)

target_include_directories(webwallpaper_thememodule PRIVATE src)

# The plugin launcher (libwebwallpaper_thememoduleplugin.so) links against the
# module's shared library (libwebwallpaper_thememodule.so, which holds
# ThemeModel). CMake bakes the build-tree path into the plugin's RUNPATH at
# compile time, so at install time the plugin must locate the shared lib next
# to itself (#rpath $ORIGIN). Without this, the lib is never found and
# `import WebWallpaper` fails, which blanked the theme grid in the wallpaper
# config dialog. Both artifacts must land in the same QML module dir.
set_target_properties(webwallpaper_thememodule PROPERTIES
    INSTALL_RPATH "$ORIGIN"
    SKIP_BUILD_RPATH ON
    BUILD_WITH_INSTALL_RPATH ON
)
# qt_add_qml_module also generates a separate plugin loader target
# (webwallpaper_thememoduleplugin.so) that links the shared lib above and
# bakes the build lib dir into its RUNPATH; that same baked path would break
# the plugin after install (and after the build dir is removed). Force it to
# resolve the shared lib from its own directory at runtime too.
set_target_properties(webwallpaper_thememoduleplugin PROPERTIES
    INSTALL_RPATH "$ORIGIN"
    SKIP_BUILD_RPATH ON
    BUILD_WITH_INSTALL_RPATH ON
)

# Install the compiled QML module so plasmashell can resolve `import WebWallpaper`.
# - user-local: goes to $HOME/.local/lib/qt6/qml/WebWallpaper; the install script
#   adds $HOME/.local/lib/qt6/qml to QML2_IMPORT_PATH for plasmashell.
# - system:     goes to the Qt QML module dir (needs root).
if(INSTALL_USER_LOCAL)
    set(QML_MODULE_DEST "$ENV{HOME}/.local/lib/qt6/qml/WebWallpaper")
elseif(QML_INSTALL_DIR)
    set(QML_MODULE_DEST "${QML_INSTALL_DIR}")
else()
    set(QML_MODULE_DEST "${Qt6_DIR}/../../../lib/qt6/qml/WebWallpaper")
endif()
install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/WebWallpaper/"
        DESTINATION "${QML_MODULE_DEST}"
        COMPONENT module)
# The shared lib with the ThemeModel implementation lives one level UP from the
# QML module dir (not inside it), so `install(DIRECTORY .../WebWallpaper/)`
# misses it. Ship it into the module dir explicitly.
install(FILES "$<TARGET_FILE:webwallpaper_thememodule>"
        DESTINATION "${QML_MODULE_DEST}"
        COMPONENT module)

# ---------------------------------------------------------------------------
# The QML wallpaper package itself.
# ---------------------------------------------------------------------------
if(INSTALL_USER_LOCAL)
    set(WALLPAPERS_DEST "$ENV{HOME}/.local/share/plasma/wallpapers/${PLUGIN_ID}")
    install(DIRECTORY package/
            DESTINATION ${WALLPAPERS_DEST}
            COMPONENT package
            PATTERN CMakeLists.txt EXCLUDE)
    # themes scanned by ThemeModel from ~/.local/share/webwallpaper/themes
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/../resources/themes/
            DESTINATION "$ENV{HOME}/.local/share/webwallpaper/themes"
            COMPONENT package)
    message(STATUS "WebWallpaper QML package -> ${WALLPAPERS_DEST} (user-local)")
else()
    find_package(ECM REQUIRED NO_MODULE)
    set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
    include(KDEInstallDirs)
    find_package(Plasma 6.0 REQUIRED)
    plasma_install_package(package ${PLUGIN_ID} wallpapers)
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/../resources/themes/
            DESTINATION share/webwallpaper/themes)
    message(STATUS "WebWallpaper QML package -> system wallpapers, + C++ module to Qt QML dir")
endif()

message(STATUS "WebWallpaper Plasma wallpaper plugin id: ${PLUGIN_ID}")
