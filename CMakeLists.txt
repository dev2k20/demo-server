cmake_minimum_required(VERSION 3.15)
project(demo_server)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++11 -pthread")

find_package(Poco REQUIRED COMPONENTS Foundation Util Net XML JSON Data DataMySQL)
find_package(Boost 1.65.1 REQUIRED system)

SET(MySQL_INCLUDE_DIRS "/usr/include/mysql")
add_executable(demo_server src/demo.cpp include/Worker.h)
include_directories(
        SYSTEM
        include
        ${MySQL_INCLUDE_DIRS}
        ${Boost_INCLUDE_DIRS}
)

target_link_libraries(demo_server PocoFoundation PocoData PocoDataMySQL PocoJSON ${Boost_LIBRARIES})
