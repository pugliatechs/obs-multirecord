cmake_minimum_required(VERSION 3.28)

project(obs-multi-record VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# --- Find OBS SDK ---
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
    PREFIX ""
)

target_include_directories(obs-multi-record PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(obs-multi-record PRIVATE
    OBS::libobs
    OBS::obs-frontend-api
    ${QT_LIBS}
)

# --- Install (cross-platform) ---
if(COMMAND install_obs_plugin_with_data)
    install_obs_plugin_with_data(obs-multi-record data)
else()
    # Platform-specific install paths
    if(APPLE)
        set(_plugin_dest "obs-plugins")
        set(_data_dest "data/obs-plugins/obs-multi-record")
    elseif(WIN32)
        set(_plugin_dest "obs-plugins/64bit")
        set(_data_dest "data/obs-plugins/obs-multi-record")
    else()
        set(_plugin_dest "lib/obs-plugins")
        set(_data_dest "share/obs/obs-plugins/obs-multi-record")
    endif()

    install(TARGETS obs-multi-record
        LIBRARY DESTINATION ${_plugin_dest}
        RUNTIME DESTINATION ${_plugin_dest}
    )
    install(DIRECTORY data/
        DESTINATION ${_data_dest}
    )
endif()
