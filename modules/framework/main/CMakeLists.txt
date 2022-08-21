project(framework_main)

FILE(GLOB SRCS *.cpp operator/test/*.cpp)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_executable(${PROJECT_NAME} ${SRCS})
add_dependencies(${PROJECT_NAME} framework)
target_link_libraries(${PROJECT_NAME}
    framework
    common
    glog
    cyber
    gflags
)

install(TARGETS ${PROJECT_NAME} DESTINATION bin/ PERMISSIONS WORLD_EXECUTE)
install(FILES execute.sh DESTINATION bin/ PERMISSIONS WORLD_EXECUTE)