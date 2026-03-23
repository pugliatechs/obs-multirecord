cmake_minimum_required(VERSION 3.28)

project(obs-multi-record VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# --- Find OBS SDK ---
# When building standalone (not inside OBS source tree), set OBS_SOURCE_DIR
# or CMAKE_PREFIX_PATH to point at your OBS install/build.

if(NOT TARGET OBS::libobs)
    find_package(libobs REQUIRED)
endif()

if(NOT TARGET OBS::obs-frontend-api)
    find_package(obs-frontend-api REQUIRED)
endif()

find_package(Qt6 COMPONENTS Widgets QUIET)
if(NOT Qt6_FOUND)
    find_package(Qt5 COMPONENTS Widgets REQUIRED)
    set(QT_LIBS Qt5::Widgets)
else()
    set(QT_LIBS Qt6::Widgets)
endif()

# --- Plugin library ---
add_library(obs-multi-record MODULE
    src/plugin-main.c
    src/record-pipeline.c
    src/record-pipeline.h
    src/multi-record-dock.cpp
    src/multi-record-dock.hpp
    src/source-combo-delegate.cpp
    src/source-combo-delegate.hpp
)

set_target_properties(obs-multi-record PROPERTIES
    AUTOMOC ON
    PREFIX ""               # no 'lib' prefix on Linux
)

target_include_directories(obs-multi-record PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(obs-multi-record PRIVATE
    OBS::libobs
    OBS::obs-frontend-api
    ${QT_LIBS}
)

# --- Install (obs-plugintemplate style) ---
if(COMMAND install_obs_plugin_with_data)
    install_obs_plugin_with_data(obs-multi-record data)
else()
    # Fallback manual install
    if(CMAKE_INSTALL_PREFIX)
        install(TARGETS obs-multi-record
            LIBRARY DESTINATION lib/obs-plugins
            RUNTIME DESTINATION lib/obs-plugins
        )
        install(DIRECTORY data/
            DESTINATION share/obs/obs-plugins/obs-multi-record
        )
    endif()
endif()
