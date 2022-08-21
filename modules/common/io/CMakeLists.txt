project(common_io)

file(GLOB SRCS *.cc)
file(GLOB HEADERS *.h)
add_library(${PROJECT_NAME} STATIC ${SRCS})

install(FILES ${HEADERS} DESTINATION include/common/io)