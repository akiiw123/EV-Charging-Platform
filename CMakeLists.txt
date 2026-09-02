cmake_minimum_required(VERSION 3.21)

project(charging-platform VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

option(CHARGING_BUILD_TESTS "Build automated tests" ON)

find_package(Qt6 6.2 REQUIRED COMPONENTS Core Widgets Sql Network Charts WebEngineWidgets)

add_subdirectory(libs/core)
add_subdirectory(apps/user-client)
add_subdirectory(apps/admin-server)

if(CHARGING_BUILD_TESTS)
    enable_testing()
    find_package(Qt6 6.2 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()
