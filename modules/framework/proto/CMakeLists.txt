project(framework_proto)

find_package(Protobuf REQUIRED)
if(PROTOBUF_FOUND)
    message(STATUS "protobuf library found")
else()
    message(FATAL_ERROR "protobuf library is needed but cant be found")
endif()
file(GLOB proto_srcs *.proto)

set(Protobuf_USE_STATIC_LIBS on)
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${proto_srcs})
protobuf_generate_python(PROTO_PYTHON ${proto_srcs})

add_library(${PROJECT_NAME} STATIC ${PROTO_SRCS} ${PROTO_HDRS} ${PROTO_PYTHON})
target_link_libraries(${PROJECT_NAME} ${PROTOBUF_LIBRARIES} ${PROTOBUF_PROTOC_LIBRARIES})
file(WRITE ${PROJECT_BINARY_DIR}/__init__.py)
list(APPEND PROTO_PYTHON ${PROJECT_BINARY_DIR}/__init__.py)
install(PROGRAMS ${PROTO_PYTHON} DESTINATION python/)

install(FILES ${PROTO_HDRS} DESTINATION include/framework/proto/)
