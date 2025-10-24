cmake_minimum_required(VERSION 3.16)
project(Carbot_Teleop C CXX)
set(CMAKE_CXX_STANDARD 17)

## add so that cmake can find the locally installed packages.
list(APPEND CMAKE_PREFIX_PATH "$ENV{HOME}/.local")

# Fix for Homebrew GStreamer linking on macOS
if(APPLE)
    # Explicitly tell the linker where to find GStreamer libs
    link_directories(
        /opt/homebrew/lib
        /opt/homebrew/Cellar/gstreamer/1.26.7/lib
        /opt/homebrew/Cellar/glib/2.86.0/lib
        /opt/homebrew/opt/gettext/lib
    )
endif()

# Find GStreamer
find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0)

find_package(SDL2 REQUIRED CONFIG)
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
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include/server")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include/client")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include/common")

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

# Server-side library
add_library(carbot_server_lib
    src/server/server_streamer.cpp
    src/server/teleop_reactor.cpp
    src/server/carbot_teleop_rpc_server.cpp
    src/server/teleoperation_server.cpp
)

target_link_libraries(carbot_server_lib
    ct_grpc_proto
    absl::base
    ${_GRPC_GRPCPP}
    ${GSTREAMER_LIBRARIES}
)

target_include_directories(carbot_server_lib PUBLIC
    ${GSTREAMER_INCLUDE_DIRS}
)

# Client-side library
add_library(carbot_client_lib
    src/client/client_streamer.cpp
    src/client/teleop_client_reactor.cpp
    src/client/carbot_teleop_rpc_client.cpp
    src/client/keyboard_controller.cpp
    src/client/teleoperation_client.cpp
    src/client/ps4_controller.cpp
)

target_link_libraries(carbot_client_lib
PRIVATE ct_grpc_proto
    absl::base
    ${_GRPC_GRPCPP}
    ${GSTREAMER_LIBRARIES}
    SDL2::SDL2
)

target_include_directories(carbot_client_lib PUBLIC
    ${GSTREAMER_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIRS}
)

# Server executable
add_executable(run_server src/server/run_server.cpp)
target_link_libraries(run_server
    carbot_server_lib
    absl::base
    ${_GRPC_GRPCPP}
    ct_grpc_proto
)

# Client executable
add_executable(run_client src/client/run_client.cpp)
target_link_libraries(run_client
    carbot_client_lib
    ct_grpc_proto
    absl::base
    ${_GRPC_GRPCPP}
)

set(CMAKE_INSTALL_PREFIX $ENV{HOME}/.local)
#### 
# Installation rules
####

target_include_directories(carbot_server_lib PUBLIC
    ## For the build tree, specify the source directory for includes
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/server>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/common>
    ## For the install tree, specify the install directory for includes
    $<INSTALL_INTERFACE:include/carbot_server/server>
    $<INSTALL_INTERFACE:include/carbot_server/common>
)

include(GNUInstallDirs)

# allows projects to find the package via find_package()
install(TARGETS carbot_server_lib
                ct_grpc_proto
    EXPORT carbot_server_libTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install public headers:
install(DIRECTORY
    ${CMAKE_CURRENT_SOURCE_DIR}/include/server
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/carbot_server
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h"
)

install(DIRECTORY
    ${CMAKE_CURRENT_SOURCE_DIR}/include/common
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/carbot_server
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h"
)

include(CMakePackageConfigHelpers)
# Export so other projects can use the target:
install(EXPORT carbot_server_libTargets
    FILE carbot_server_libTargets.cmake
    NAMESPACE carbot_server_lib::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/carbot_server_lib
)

configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Config.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/carbot_server_libConfig.cmake"
  INSTALL_DESTINATION lib/cmake/carbot_server_lib
)

install(FILES
          "${CMAKE_CURRENT_BINARY_DIR}/carbot_server_libConfig.cmake"
        DESTINATION lib/cmake/carbot_server_lib
)