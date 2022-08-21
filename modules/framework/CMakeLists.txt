project(framework)

FILE(GLOB SRCS *.cpp)
file(GLOB HEADERS *.h)

find_package(Protobuf REQUIRED)
if(PROTOBUF_FOUND)
    message(STATUS "protobuf library found")
else()
    message(FATAL_ERROR "protobuf library is needed but cant be found")
endif()

if(DEFINED ENV{SDKTARGETSYSROOT})
    link_directories($ENV{SDKTARGETSYSROOT}/usr/lib)
    include_directories($ENV{SDKTARGETSYSROOT}/usr/include)
    set(Protobuf_INCLUDE_DIRS $ENV{SDKTARGETSYSROOT}/usr/include CACHE INTERNAL "Protobuf_include")
    set(Protobuf_LIBRARIES $ENV{SDKTARGETSYSROOT}/usr/lib/libprotobuf.so CACHE INTERNAL "Protobuf_libraries")
    set(Protobuf_PROTOC_LIBRARIES $ENV{SDKTARGETSYSROOT}/usr/lib/libprotoc.so CACHE INTERNAL "Protobuf_protoc_libraries")
endif()


add_subdirectory(proto)
add_subdirectory(production)
add_subdirectory(main)
add_subdirectory(tools)

add_library(${PROJECT_NAME} SHARED ${SRCS})
add_dependencies(${PROJECT_NAME} framework_proto)
target_link_libraries(${PROJECT_NAME} -Wl,--whole-archive
    framework_proto
    pthread -Wl,--no-whole-archive
)

install(FILES ${HEADERS} DESTINATION include/framework/)
install(TARGETS ${PROJECT_NAME} DESTINATION lib/)