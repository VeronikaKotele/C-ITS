cmake_minimum_required(VERSION 3.20)
project(C_ITS LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Protobuf REQUIRED)

protobuf_generate_cpp(
    PROTO_SRCS
    PROTO_HDRS
    protobuf/srem.proto
)

find_path(PAHO_INCLUDE
    NAMES mqtt/async_client.h
    REQUIRED
)

find_library(PAHO_CPP_LIB
    NAMES paho-mqttpp3
    REQUIRED
)

find_library(PAHO_C_LIB
    NAMES paho-mqtt3as paho-mqtt3a
    REQUIRED
)

add_executable(C_ITS
    src/C-ITS.cpp
    src/MqttClient.cpp
    src/RoadsideUnit.cpp
    src/Vehicle.cpp
    ${PROTO_SRCS}
    ${PROTO_HDRS}
)

target_include_directories(C_ITS PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${PAHO_INCLUDE}
)

target_link_libraries(C_ITS PRIVATE
    protobuf::libprotobuf
    ${PAHO_CPP_LIB}
    ${PAHO_C_LIB}
    pthread
)

message(STATUS "Protobuf generated headers: ${PROTO_HDRS}")
message(STATUS "Paho include: ${PAHO_INCLUDE}")
message(STATUS "Paho C++ lib: ${PAHO_CPP_LIB}")
message(STATUS "Paho C lib: ${PAHO_C_LIB}")