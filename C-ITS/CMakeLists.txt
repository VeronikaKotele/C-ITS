# CMakeList.txt : CMake project for C-ITS, include source and define
# project specific logic here.
#

# Add source to this project's executable.
add_executable (C-ITS "src/C-ITS.cpp" "src/C-ITS.h" "src/publisher.cpp" "src/subscriber.cpp" )

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET C-ITS PROPERTY CXX_STANDARD 20)
endif()

project(C_ITS LANGUAGES CXX)

find_package(Protobuf REQUIRED)

protobuf_generate_cpp(
    PROTO_SRCS
    PROTO_HDRS
    proto/srem.proto
)

add_executable(C_ITS
    src/C-ITS.cpp
    ${PROTO_SRCS}
    ${PROTO_HDRS}
)

target_include_directories(C_ITS PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(C_ITS PRIVATE
    protobuf::libprotobuf
    paho-mqttpp3
    paho-mqtt3a
)