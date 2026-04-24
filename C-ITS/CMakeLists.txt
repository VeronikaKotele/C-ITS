# CMakeList.txt : CMake project for C-ITS, include source and define
# project specific logic here.
#

# Add source to this project's executable.
add_executable (C-ITS "src/C-ITS.cpp" "src/C-ITS.h" "src/Vehicle.cpp" "src/RoadsideUnit.cpp"  "src/RoadsideUnit.h")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET C-ITS PROPERTY CXX_STANDARD 20)
endif()

project(C_ITS LANGUAGES CXX)

find_package(Protobuf REQUIRED)

protobuf_generate_cpp(
    PROTO_SRCS
    PROTO_HDRS
    protobuf/srem.proto
)

# try pkg-config first
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(PAHO_MQTTPP QUIET paho-mqttpp3)
endif()

# fallback: try common locations
if(NOT PAHO_MQTTPP_FOUND)
  find_path(PAHO_MQTTPP_INCLUDE
    NAMES mqtt/async_client.h
    PATHS /usr/include /usr/local/include /usr/include/paho
  )
  find_library(PAHO_MQTTPP_LIB
    NAMES paho-mqttpp3 paho-mqttpp3.so
    PATHS /usr/lib /usr/local/lib /usr/lib/x86_64-linux-gnu
  )
endif()

if(PAHO_MQTTPP_INCLUDE)
  target_include_directories(C_ITS PRIVATE ${PAHO_MQTTPP_INCLUDE})
endif()
if(PAHO_MQTTPP_LIB)
  target_link_libraries(C_ITS PRIVATE ${PAHO_MQTTPP_LIB} paho-mqtt3a)
else()
  # fall back to user-specified names
  target_link_libraries(C_ITS PRIVATE paho-mqttpp3 paho-mqtt3a)
endif()

add_executable(C_ITS
    src/C-ITS.cpp
    src/MqttClient.cpp
    src/RoadsideUnit.cpp
    src/Vehicle.cpp
    ${PROTO_SRCS}
    ${PROTO_HDRS})

target_compile_features(C_ITS PUBLIC cxx_std_20)
target_compile_options(C_ITS PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/std:c++20>)

target_include_directories(C_ITS PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(C_ITS PRIVATE
    protobuf::libprotobuf
    paho-mqttpp3
    paho-mqtt3a
)