cmake_minimum_required(VERSION 3.23)
project(MyHttpd)
link_libraries(ws2_32)
set(CMAKE_CXX_STANDARD 14)

add_executable(MyHttpd main.cpp Httpd.cpp)
