add_executable(nalu_board_manual_capture main.cpp)
target_link_libraries(nalu_board_manual_capture
  PRIVATE
    ${PROJECT_NAME}
    nalu_board_controller_spdlog
    nalu_board_controller_nlohmann_json
)
set_target_properties(nalu_board_manual_capture PROPERTIES OUTPUT_NAME manual_capture)
