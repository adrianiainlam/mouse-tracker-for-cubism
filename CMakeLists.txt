cmake_minimum_required(VERSION 3.16)

project(MouseTrackerForCubism_project)

find_library(xdo_LIBS NAMES xdo PATHS /usr/lib REQUIRED)
find_library(pulse_LIBS NAMES pulse PATHS /usr/lib REQUIRED)

include(ExternalProject)
ExternalProject_Add(editline
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib/editline/src
    BINARY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib/editline
    CONFIGURE_COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/lib/editline/autogen.sh COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/lib/editline/configure --prefix=<INSTALL_DIR>
    BUILD_COMMAND ${MAKE})

ExternalProject_Get_Property(editline install_dir)
set(editline_INSTALL_DIR ${install_dir})

include_directories(include ${CMAKE_CURRENT_SOURCE_DIR}/lib/editline/include)

add_library(
    MouseTrackerForCubism STATIC
    src/mouse_cursor_tracker.cpp
)

set_target_properties(
    MouseTrackerForCubism PROPERTIES PUBLIC_HEADER
    include/mouse_cursor_tracker.h
)

target_link_libraries(MouseTrackerForCubism ${xdo_LIBS} ${pulse_LIBS} pulse-simple ${editline_INSTALL_DIR}/lib/libeditline.a)
