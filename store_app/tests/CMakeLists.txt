set(CMAKE_AUTOMOC ON)

get_filename_component(STORE_APP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)

find_package(Qt6 6.5 REQUIRED COMPONENTS Test)

function(webwallpaper_add_store_test name)
    cmake_parse_arguments(ARG "" "" "SOURCES;LINKS" ${ARGN})
    add_executable(${name} ${name}.cpp ${ARG_SOURCES})
    target_include_directories(${name} PRIVATE "${STORE_APP_DIR}")
    target_link_libraries(${name} PRIVATE Qt6::Core Qt6::Test ${ARG_LINKS})
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES TIMEOUT 60)
endfunction()

webwallpaper_add_store_test(tst_configmanager
    SOURCES ../configmanager.cpp
)

webwallpaper_add_store_test(tst_firestore
    SOURCES ../firestore.cpp
)

webwallpaper_add_store_test(tst_themecard
    SOURCES ../themecard.cpp
    LINKS Qt6::Gui Qt6::Widgets
)
set_tests_properties(tst_themecard PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")