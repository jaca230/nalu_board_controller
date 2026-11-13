cmake_minimum_required(VERSION 3.14)
project(firmware_960_validation VERSION 0.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
include(${CMAKE_CURRENT_LIST_DIR}/../../../cmake/CPM.cmake)
CPMAddPackage(NAME nalu_board_controller SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../../..")
CPMAddPackage(NAME nlohmann_json GITHUB_REPOSITORY nlohmann/json VERSION 3.11.3)
add_executable(firmware_960_validation main.cpp)
target_link_libraries(firmware_960_validation PRIVATE
  nalu_board_controller::nalu_board_controller
  nalu_board_controller_spdlog
  nlohmann_json::nlohmann_json)
