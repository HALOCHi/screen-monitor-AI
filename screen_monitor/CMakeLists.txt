cmake_minimum_required(VERSION 3.15)
project(ScreenMonitor)

set(CMAKE_CXX_STANDARD 17)

include(FetchContent)
FetchContent_Declare(
  curl
  URL https://github.com/curl/curl/releases/download/curl-8_11_0/curl-8.11.0.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(BUILD_CURL_EXE OFF CACHE INTERNAL "")
set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "")
set(CURL_USE_LIBPSL OFF CACHE INTERNAL "")
set(CURL_ENABLE_SSL ON CACHE INTERNAL "")
set(CURL_DISABLE_LDAP ON CACHE INTERNAL "")
set(CURL_DISABLE_LDAPS ON CACHE INTERNAL "")
FetchContent_MakeAvailable(curl)

add_executable(screen_monitor main.cpp)

if(WIN32)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    
    target_link_libraries(screen_monitor PRIVATE libcurl gdiplus gdi32 user32 Ws2_32 Crypt32 Wldap32 Normaliz)
    
    set_target_properties(screen_monitor PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS")

else()
    find_package(X11 REQUIRED)
    
    target_link_libraries(screen_monitor PRIVATE libcurl ${X11_LIBRARIES} pthread)
    
    target_compile_definitions(screen_monitor PRIVATE IS_LINUX)
endif()