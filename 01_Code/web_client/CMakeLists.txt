add_executable(web_client src/main.cpp src/campus_map.cpp)
target_include_directories(web_client PRIVATE src "${PROJECT_SOURCE_DIR}/third_party")
target_link_libraries(web_client PRIVATE mgw_client nlohmann_headers ws2_32)
