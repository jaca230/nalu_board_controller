cmake_minimum_required(VERSION 3.14)

project(manual_capture VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

option(USE_LOCAL_NALU_BOARD_CONTROLLER "Build against a local nalu_board_controller source tree" ON)
set(NALU_BOARD_CONTROLLER_SOURCE_DIR
    "${CMAKE_CURRENT_LIST_DIR}/../../.."
    CACHE PATH "Path to a local nalu_board_controller source tree")

include(${CMAKE_CURRENT_LIST_DIR}/../../../cmake/CPM.cmake)

set(CPM_NLOHMANN_JSON_VERSION "3.11.3" CACHE STRING "nlohmann_json version")

if(USE_LOCAL_NALU_BOARD_CONTROLLER AND EXISTS "${NALU_BOARD_CONTROLLER_SOURCE_DIR}/CMakeLists.txt")
  CPMAddPackage(
    NAME nalu_board_controller
    SOURCE_DIR "${NALU_BOARD_CONTROLLER_SOURCE_DIR}"
  )
else()
  find_package(nalu_board_controller REQUIRED)
endif()

CPMAddPackage(
  NAME nlohmann_json
  GITHUB_REPOSITORY nlohmann/json
  VERSION ${CPM_NLOHMANN_JSON_VERSION}
)

if(TARGET nalu_board_controller_spdlog)
  set(APP_SPDLOG_TARGET nalu_board_controller_spdlog)
elseif(TARGET spdlog::spdlog)
  set(APP_SPDLOG_TARGET spdlog::spdlog)
elseif(TARGET spdlog::spdlog_header_only)
  set(APP_SPDLOG_TARGET spdlog::spdlog_header_only)
elseif(TARGET spdlog)
  set(APP_SPDLOG_TARGET spdlog)
else()
  find_package(spdlog CONFIG REQUIRED)
  if(TARGET spdlog::spdlog)
    set(APP_SPDLOG_TARGET spdlog::spdlog)
  elseif(TARGET spdlog::spdlog_header_only)
    set(APP_SPDLOG_TARGET spdlog::spdlog_header_only)
  elseif(TARGET spdlog)
    set(APP_SPDLOG_TARGET spdlog)
  else()
    message(FATAL_ERROR "Could not determine the spdlog target to link against")
  endif()
endif()

add_executable(manual_capture main.cpp)
target_link_libraries(manual_capture
  PRIVATE
    nalu_board_controller::nalu_board_controller
    ${APP_SPDLOG_TARGET}
    nlohmann_json::nlohmann_json
)
