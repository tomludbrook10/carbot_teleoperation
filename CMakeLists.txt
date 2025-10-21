cmake_minimum_required(VERSION 3.16)
project(Carbot_Teleop C CXX)
set(CMAKE_CXX_STANDARD 17)

## add so that cmake can find the locally installed packages.
list(APPEND CMAKE_PREFIX_PATH "$ENV{HOME}/.local")


find_package(absl CONFIG REQUIRED)
set(protobuf_MODULE_COMPATIBLE TRUE)
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)

### set variables.
set(_PROTOBUF_PROTOC_EXECUTABLE $<TARGET_FILE:protobuf::protoc>) # path to protoc executable
set(_GRPC_GRPCPP gRPC::grpc++)
set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:gRPC::grpc_cpp_plugin>) # path to grpc_cpp_plugin executable
#set(_PROTOBUF_LIBPROTOBUF Protobuf::libprotobuf)
#set(_REFLECTION gRPC::reflection)

###
# Proto generation code.
###



get_filename_component(ct_proto "protos/carbot_teleop.proto" ABSOLUTE)
get_filename_component(ct_proto_path "${ct_proto}" PATH)

set(ct_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/carbot_teleop.pb.cc")
set(ct_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/carbot_teleop.pb.h")
set(ct_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/carbot_teleop.grpc.pb.cc")
set(ct_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/carbot_teleop.grpc.pb.h")

## Generate the protobuf and gRPC code for Carbot Teleop.
add_custom_command(
      OUTPUT "${ct_proto_srcs}" "${ct_proto_hdrs}" "${ct_grpc_srcs}" "${ct_grpc_hdrs}"
      COMMAND ${_PROTOBUF_PROTOC_EXECUTABLE} ## run the protoc compiler
        ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
            --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
            --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
            -I "${ct_proto_path}"
            "${ct_proto}"
        DEPENDS "${ct_proto}")

#include headers.
include_directories("${CMAKE_CURRENT_BINARY_DIR}")

add_library(ct_grpc_proto
    ${ct_proto_srcs}
    ${ct_proto_hdrs}
    ${ct_grpc_srcs}
    ${ct_grpc_hdrs}
)

target_link_libraries(ct_grpc_proto
    absl::base
    ${_GRPC_GRPCPP}
)

add_executable(basic_server src/basic_server.cpp)
target_link_libraries(basic_server
    ct_grpc_proto
    absl::base
    ${_GRPC_GRPCPP}
)

add_executable(basic_client src/basic_client.cpp)
target_link_libraries(basic_client
    ct_grpc_proto
    absl::base
    ${_GRPC_GRPCPP}
)
