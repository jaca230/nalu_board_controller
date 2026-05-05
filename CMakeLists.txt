cmake_minimum_required(VERSION 3.14)

project(nalu_board_controller VERSION 0.2 LANGUAGES CXX)

if(NOT DEFINED PROJECT_NAMESPACE)
  set(PROJECT_NAMESPACE nalu_board_controller)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

option(USE_EXTERNAL_SPDLOG "Use system-installed spdlog" OFF)
option(USE_EXTERNAL_PYBIND11 "Use system-installed pybind11" OFF)
option(USE_EXTERNAL_NLOHMANN_JSON "Use system-installed nlohmann_json" OFF)
option(BUILD_APPS "Build standalone apps" OFF)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-Wno-stringop-overflow)
  endif()
endif()

set(CPM_SPDLOG_VERSION "1.13.0" CACHE STRING "spdlog version")
set(CPM_PYBIND11_VERSION "2.13.6" CACHE STRING "pybind11 version")
set(CPM_NLOHMANN_JSON_VERSION "3.11.3" CACHE STRING "nlohmann_json version")

if(USE_EXTERNAL_SPDLOG)
  find_package(spdlog REQUIRED)
  add_library(nalu_board_controller_spdlog INTERFACE)
  target_link_libraries(nalu_board_controller_spdlog INTERFACE spdlog::spdlog)
else()
  CPMAddPackage(
    NAME spdlog
    GITHUB_REPOSITORY gabime/spdlog
    VERSION ${CPM_SPDLOG_VERSION}
    OPTIONS
      "SPDLOG_BUILD_EXAMPLES OFF"
      "SPDLOG_BUILD_TESTS OFF"
      "SPDLOG_FMT_EXTERNAL OFF"
    DOWNLOAD_ONLY YES
  )

  add_library(nalu_board_controller_spdlog INTERFACE)
  target_include_directories(nalu_board_controller_spdlog INTERFACE
    $<BUILD_INTERFACE:${spdlog_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  )
endif()

if(USE_EXTERNAL_PYBIND11)
  find_package(pybind11 CONFIG REQUIRED)
else()
  CPMAddPackage(
    NAME pybind11
    GITHUB_REPOSITORY pybind/pybind11
    VERSION ${CPM_PYBIND11_VERSION}
    OPTIONS
      "PYBIND11_TEST OFF"
      "PYBIND11_INSTALL OFF"
  )
endif()

if(BUILD_APPS)
  if(USE_EXTERNAL_NLOHMANN_JSON)
    find_package(nlohmann_json CONFIG REQUIRED)
    add_library(nalu_board_controller_nlohmann_json INTERFACE)
    target_link_libraries(nalu_board_controller_nlohmann_json INTERFACE nlohmann_json::nlohmann_json)
  else()
    CPMAddPackage(
      NAME nlohmann_json
      GITHUB_REPOSITORY nlohmann/json
      VERSION ${CPM_NLOHMANN_JSON_VERSION}
      DOWNLOAD_ONLY YES
    )

    add_library(nalu_board_controller_nlohmann_json INTERFACE)
    target_include_directories(nalu_board_controller_nlohmann_json INTERFACE
      $<BUILD_INTERFACE:${nlohmann_json_SOURCE_DIR}/include>
      $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )
  endif()
endif()

file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

add_library(${PROJECT_NAME} SHARED ${SRC_FILES})
add_library(${PROJECT_NAMESPACE}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})
add_library(${PROJECT_NAMESPACE}::controller ALIAS ${PROJECT_NAME})

set_target_properties(${PROJECT_NAME} PROPERTIES
  VERSION ${PROJECT_VERSION}
  SOVERSION 0
)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)

target_include_directories(${PROJECT_NAME}
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(${PROJECT_NAME}
  PRIVATE
    pybind11::embed
    nalu_board_controller_spdlog
)

if(BUILD_APPS)
  add_subdirectory(apps)
endif()

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
  install(TARGETS ${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}Targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )

  install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )

  install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/resources/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}
  )

  install(EXPORT ${PROJECT_NAME}Targets
    FILE ${PROJECT_NAME}Targets.cmake
    NAMESPACE ${PROJECT_NAMESPACE}::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
  )

  write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
  )

  configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
  )

  install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
  )

  export(PACKAGE ${PROJECT_NAME})
endif()
