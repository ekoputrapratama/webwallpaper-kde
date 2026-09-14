set(CMAKE_AUTOMOC ON)

get_filename_component(PLUGIN_BIN_DIR "${CMAKE_CURRENT_BINARY_DIR}/.." ABSOLUTE)
get_filename_component(PLUGIN_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)

find_package(Qt6 6.5 REQUIRED COMPONENTS Test Qml Quick QuickTest)

# ---------------------------------------------------------------------------
# C++ regression tests for ThemeModel (theme scanning, key casing, defaults,
# dedup). Runs the model source directly against a scratch theme directory
# (WEBWALLPAPER_THEMES_DIR hook), so results don't depend on what themes are
# installed on the machine.
# ---------------------------------------------------------------------------
add_executable(tst_thememodel tst_thememodel.cpp ../src/thememodel.cpp)
target_include_directories(tst_thememodel PRIVATE "${PLUGIN_SRC_DIR}/src")
target_link_libraries(tst_thememodel PRIVATE Qt6::Core Qt6::Test Qt6::Qml)
add_test(NAME tst_thememodel COMMAND tst_thememodel)
set_tests_properties(tst_thememodel PROPERTIES TIMEOUT 60)

# ---------------------------------------------------------------------------
# QtQuickTest integration: load the REAL WebWallpaper QML module through the
# normal QML import machinery (import WebWallpaper) and instantiate ThemeModel.
# Catches the module-load regressions that historically blanked the wallpaper
# config grid (shared lib not shipped / wrong rpath). Links the module target
# and points QML2_IMPORT_PATH/LD_LIBRARY_PATH at the plugin build dir.
# ---------------------------------------------------------------------------
set(TEST_THEME_DIR "${CMAKE_CURRENT_BINARY_DIR}/testthemes")
file(MAKE_DIRECTORY "${TEST_THEME_DIR}/demo")
file(WRITE "${TEST_THEME_DIR}/demo/demo.theme"
     "[Theme]\nName=demo\nDescription=QML test fixture\nThumbnail=thumbnail.png\nEntry=index.html\n")
file(WRITE "${TEST_THEME_DIR}/demo/index.html" "<html></html>\n")

add_executable(tst_webwallpaper tst_webwallpaper.cpp)
target_compile_definitions(tst_webwallpaper PRIVATE
    QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(tst_webwallpaper PRIVATE
    webwallpaper_thememodule
    Qt6::Test
    Qt6::QuickTest
)
add_test(NAME tst_webwallpaper_qml COMMAND tst_webwallpaper)
set_tests_properties(tst_webwallpaper_qml PROPERTIES
    TIMEOUT 60
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;WEBWALLPAPER_THEMES_DIR=${TEST_THEME_DIR};QML2_IMPORT_PATH=${PLUGIN_BIN_DIR};LD_LIBRARY_PATH=${PLUGIN_BIN_DIR}"
)