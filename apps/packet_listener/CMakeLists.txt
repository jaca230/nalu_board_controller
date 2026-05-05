add_executable(nalu_board_packet_listener main.cpp)
target_include_directories(nalu_board_packet_listener
  PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(nalu_board_packet_listener
  PRIVATE
    ${PROJECT_NAME}
    nalu_board_controller_spdlog
    nalu_board_controller_nlohmann_json
)
set_target_properties(nalu_board_packet_listener PROPERTIES OUTPUT_NAME packet_listener)
