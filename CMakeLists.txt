cmake_minimum_required(VERSION 3.16)
project(WebWallpaperStore VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

set(SOURCES
    store_app/main.cpp
    store_app/configmanager.cpp
    store_app/networkmanager.cpp
    store_app/storewindow.cpp
    store_app/themecard.cpp
)

set(HEADERS
    store_app/configmanager.h
    store_app/networkmanager.h
    store_app/storewindow.h
    store_app/themecard.h
    store_app/themedata.h
)

set(RESOURCES
    resources/resources.qrc
)

# The embedded config.env is gitignored (so real Firebase creds never leak into
# the published source). On a fresh clone it won't exist; create it from the
# committed example so the resource still compiles. Users replace config.env
# with their real values before building.
set(CONFIG_ENV "${CMAKE_CURRENT_SOURCE_DIR}/resources/config/config.env")
set(CONFIG_ENV_EXAMPLE "${CMAKE_CURRENT_SOURCE_DIR}/resources/config/config.env.example")
if(NOT EXISTS "${CONFIG_ENV}")
    file(READ "${CONFIG_ENV_EXAMPLE}" CONFIG_ENV_CONTENT)
    file(WRITE "${CONFIG_ENV}" "${CONFIG_ENV_CONTENT}")
endif()

add_executable(webwallpaper-store ${SOURCES} ${HEADERS} ${RESOURCES})

target_include_directories(webwallpaper-store PRIVATE store_app)

target_link_libraries(webwallpaper-store PRIVATE
    Qt6::Widgets
    Qt6::Network
)

install(TARGETS webwallpaper-store DESTINATION bin)

install(FILES resources/icons/app.png
        DESTINATION share/icons/hicolor/512x512/apps
        RENAME webwallpaper-store.png)

install(FILES store_app/webwallpaper-store.desktop
        DESTINATION share/applications)