cmake_minimum_required(VERSION 2.8)

project(crdc_airi_commom)

add_definitions(-std=c++14 -fPIC)
set(CMAKE_CXX_FLAGS "-std=c++14 -fPIC")
set(CMAKE_BUILD_TYPE "Release")
set(DEPENDENCIES_PATH ${CMAKE_CURRENT_BINARY_DIR}/modules)

if (TEST_ENABLE)
    find_package(GTest MODULE REQUIRED)
    enable_testing()
endif()

add_subdirectory(modules)