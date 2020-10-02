cmake_minimum_required(VERSION 3.16)

project(MouseTrackerForCubism_project)

find_library(xdo_LIBS NAMES xdo libxdo PATHS /usr/lib REQUIRED)
find_library(pulse_LIBS NAMES pulse PATHS /usr/lib REQUIRED)

include_directories(include)

add_library(
    MouseTrackerForCubism STATIC
    src/mouse_cursor_tracker.cpp
)

set_target_properties(
    MouseTrackerForCubism PROPERTIES PUBLIC_HEADER
    include/mouse_cursor_tracker.h
)

target_link_libraries(MouseTrackerForCubism ${xdo_LIBS} ${pulse_LIBS} pulse-simple)
