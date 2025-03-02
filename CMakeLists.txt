cmake_minimum_required(VERSION 3.10)
project(nalu_board_controller)

# Set the output directories for the build
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Find Pybind11
find_package(pybind11 REQUIRED)

# Specify include directories
include_directories(include ${pybind11_INCLUDE_DIRS})

# Apply the -fvisibility=hidden flag globally for C++ files
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")

# Gather source files
file(GLOB_RECURSE SOURCES "src/*.cpp")

# Create a static library from all source files
add_library(nalu_board_controller STATIC ${SOURCES})

# Link Pybind11 to the library
target_link_libraries(nalu_board_controller PRIVATE pybind11::embed)

# Specify where to install the header files and library
# Install headers into /usr/local/nalu_board_controller/include
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_PREFIX}/include/nalu_board_controller)

# Install the library into /usr/local/nalu_board_controller/lib
install(TARGETS nalu_board_controller DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)

# Create the executable but don't install it
add_executable(main main.cpp)

# Link the executable with the library
target_link_libraries(main PRIVATE nalu_board_controller)

# Do not install the executable
# Uncomment the following line to install the executable if needed:
# install(TARGETS main DESTINATION ${CMAKE_INSTALL_PREFIX}/nalu_board_controller/bin)
