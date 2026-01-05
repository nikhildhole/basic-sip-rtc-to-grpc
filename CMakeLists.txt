cmake_minimum_required(VERSION 3.15)
project(sip_gateway)

# Default to 0.0.0 if not set externally
if(NOT DEFINED PROJECT_VERSION)
    set(PROJECT_VERSION "0.0.0")
endif()

project(sip_gateway VERSION ${PROJECT_VERSION} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dependencies
find_package(Protobuf REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(Threads REQUIRED)

# Protobuf/gRPC generation
get_target_property(grpc_cpp_plugin_location gRPC::grpc_cpp_plugin LOCATION)

set(PROTO_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/protos")
set(PROTO_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY ${PROTO_BINARY_DIR})

set(PROTO_FILES "${PROTO_SRC_DIR}/audio_gateway.proto")

# Generate Protobuf sources
add_custom_command(
    OUTPUT "${PROTO_BINARY_DIR}/audio_gateway.pb.cc"
           "${PROTO_BINARY_DIR}/audio_gateway.pb.h"
           "${PROTO_BINARY_DIR}/audio_gateway.grpc.pb.cc"
           "${PROTO_BINARY_DIR}/audio_gateway.grpc.pb.h"
    COMMAND ${Protobuf_PROTOC_EXECUTABLE}
    ARGS --grpc_out="${PROTO_BINARY_DIR}"
         --cpp_out="${PROTO_BINARY_DIR}"
         --plugin=protoc-gen-grpc="${grpc_cpp_plugin_location}"
         -I "${PROTO_SRC_DIR}"
         "${PROTO_FILES}"
    DEPENDS "${PROTO_FILES}"
)

add_library(grpc_proto
    "${PROTO_BINARY_DIR}/audio_gateway.pb.cc"
    "${PROTO_BINARY_DIR}/audio_gateway.pb.h"
    "${PROTO_BINARY_DIR}/audio_gateway.grpc.pb.cc"
    "${PROTO_BINARY_DIR}/audio_gateway.grpc.pb.h"
)
target_link_libraries(grpc_proto PUBLIC gRPC::grpc++ protobuf::libprotobuf)
target_include_directories(grpc_proto PUBLIC "${PROTO_BINARY_DIR}")

# Main Executable
add_executable(sip_gateway
    src/main.cpp
    src/sip_server.cpp
    src/rtp_receiver.cpp
    src/grpc_service.cpp
    src/config.cpp
)

target_link_libraries(sip_gateway PRIVATE grpc_proto Threads::Threads)

if(WIN32)
    target_link_libraries(sip_gateway PRIVATE ws2_32)
endif()

target_include_directories(sip_gateway PRIVATE src)

# Installation (Required for CPack)
install(TARGETS sip_gateway DESTINATION bin)
install(FILES "README.md" DESTINATION share/doc/sip_gateway OPTIONAL)

# CPack Configuration
set(CPACK_PACKAGE_NAME "sip-gateway")
set(CPACK_PACKAGE_VENDOR "MyCompany")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "SIP to gRPC Audio Gateway")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_INSTALL_DIRECTORY "sip_gateway")

# DEB
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "maintainer@example.com")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)

# RPM
set(CPACK_RPM_PACKAGE_LICENSE "MIT")

# WIX (Windows MSI) - Optional, using ZIP for simplicity as requested "ZIP"
# But user can use NSIS if installed. defaulting to zip for simple windows archive.

# DMG
set(CPACK_DMG_VOLUME_NAME "SipGateway")

include(CPack)
