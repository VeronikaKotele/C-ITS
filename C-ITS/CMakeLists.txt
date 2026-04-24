# CMakeList.txt : CMake project for C-ITS, include source and define
# project specific logic here.
#

# Add source to this project's executable.
add_executable (C-ITS "src/C-ITS.cpp" "src/C-ITS.h" "src/publisher.cpp" "constants.h" "src/subscriber.cpp" )

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET C-ITS PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.
