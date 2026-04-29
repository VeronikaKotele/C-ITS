cmake_minimum_required(VERSION 3.20)
project(C_ITS LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Protobuf REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

protobuf_generate_cpp(
    PROTO_SRCS
    PROTO_HDRS
    protobuf/etsi_common.proto
    protobuf/cam.proto
    protobuf/srem.proto
    protobuf/ssem.proto
    protobuf/spatem.proto
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

find_path(NLOHMANN_JSON_INCLUDE
    NAMES nlohmann/json.hpp
    REQUIRED
)

find_path(ASIO_INCLUDE
    NAMES asio.hpp
    REQUIRED
)

message(STATUS "nlohmann json include: ${NLOHMANN_JSON_INCLUDE}")
message(STATUS "Asio include: ${ASIO_INCLUDE}")

add_executable(C_ITS
    src/C-ITS.cpp
    src/MqttClient.cpp
    src/RoadsideUnit.cpp
    src/Vehicle.cpp
    src/InterfacesTranslator.cpp
    src/utils.cpp
    src/WebSocketBridge.cpp
    
    ${PROTO_SRCS}
    ${PROTO_HDRS}
 "src/TrafficController.cpp")

target_compile_definitions(C_ITS PRIVATE
    ASIO_STANDALONE
)

target_include_directories(C_ITS PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party # third_party should go first to have higher priority than system include
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${PAHO_INCLUDE}
    ${NLOHMANN_JSON_INCLUDE}
    ${ASIO_INCLUDE}
)

target_link_libraries(C_ITS PRIVATE
    protobuf::libprotobuf
    nlohmann_json::nlohmann_json
    ${PAHO_CPP_LIB}
    ${PAHO_C_LIB}
    pthread
)

# Original lib doesn't support c++20, so fixed headers stored in third_party/websocketpp
# TIP: If you want to update websocketpp, make sure to apply the same fixes to the new version
# TIP 2: Not all headers were fixed, only those that is currently used, so if you use new headers,
# you may got errors about constructor/destructor with template parameters, like error: template-id not allowed Class<T>() -
# the FIX for such error is to manually remove template parameters from constructor/destructor, e.g. change Class<T>() to Class()
set(LOCAL_WEBSOCKETPP_CORE
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/websocketpp/config/core.hpp"
)
if(NOT EXISTS "${LOCAL_WEBSOCKETPP_CORE}")
    message(FATAL_ERROR "Patched websocketpp not found: ${LOCAL_WEBSOCKETPP_CORE}")
endif()

message(STATUS "Protobuf generated headers: ${PROTO_HDRS}")
message(STATUS "Paho include: ${PAHO_INCLUDE}")
message(STATUS "Paho C++ lib: ${PAHO_CPP_LIB}")
message(STATUS "Paho C lib: ${PAHO_C_LIB}")
message(STATUS "WebSocket++ (patched): ${LOCAL_WEBSOCKETPP_CORE}")