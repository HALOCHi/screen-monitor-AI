cmake_minimum_required(VERSION 3.10)
project(ScreenMonitor)

set(CMAKE_CXX_STANDARD 17)

# Ищем библиотеку libcurl
find_package(CURL REQUIRED)

# Создаем исполняемый файл
add_executable(screen_monitor main.cpp)

# Подключаем curl к нашей программе
target_link_libraries(screen_monitor PRIVATE CURL::libcurl)
