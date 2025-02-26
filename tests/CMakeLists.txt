cmake_minimum_required(VERSION 3.10)

# Test executable
add_executable(nx_agent_test
    nx_agent_test.cpp
)

# Link dependencies
target_link_libraries(nx_agent_test
    ${OpenCV_LIBS}
    nlohmann_json::nlohmann_json
    ${NX_SDK_DIR}/lib/libnx_sdk.a
)

# Add include directories
target_include_directories(nx_agent_test PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Copy test data
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
