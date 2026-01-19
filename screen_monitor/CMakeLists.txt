cmake_minimum_required(VERSION 3.10)
project(ScreenMonitor)

set(CMAKE_CXX_STANDARD 17)

find_package(CURL REQUIRED)

add_executable(screen_monitor main.cpp)

if(WIN32)
    target_link_libraries(screen_monitor PRIVATE CURL::libcurl gdiplus gdi32 user32)
    set_target_properties(screen_monitor PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS")
else()
    target_link_libraries(screen_monitor PRIVATE CURL::libcurl)
endif()