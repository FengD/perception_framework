cmake_minimum_required(VERSION 2.8)

project(crdc_airi_library)

set(CMAKE_CXX_FLAGS "-std=c++14 -fPIC -fpermissive")
set(CMAKE_BUILD_TYPE "Release")

if (DO_TEST)
    find_package(GTest MODULE REQUIRED)
    find_package(Threads)
    enable_testing()
endif()

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${DEPENDENCIES_PATH}/apollo/cyber/include  
)

link_directories(
    ${DEPENDENCIES_PATH}/apollo/cyber/lib
)

add_subdirectory(communication)
add_subdirectory(common)
add_subdirectory(tools)
add_subdirectory(framework)
add_subdirectory(viewer)