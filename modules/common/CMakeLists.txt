project(common)


if (DO_TEST)
    add_subdirectory(test)
endif()

add_subdirectory(io)

file(GLOB SRCS *.cc)
file(GLOB HEADERS *.h)
add_library(${PROJECT_NAME} SHARED ${SRCS})
target_link_libraries(${PROJECT_NAME} -Wl,--whole-archive
    common_io pthread -Wl,--no-whole-archive)

install(FILES ${HEADERS} DESTINATION include/common/)
install(TARGETS ${PROJECT_NAME} DESTINATION lib/)