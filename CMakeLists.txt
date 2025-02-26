cmake_minimum_required(VERSION 3.10)
project(nx_agent_plugin)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find dependencies
find_package(OpenCV REQUIRED)
find_package(nlohmann_json REQUIRED)

# Define paths to Nx SDK
set(NX_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/nx_sdk" CACHE PATH "Path to Nx SDK directory")

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${NX_SDK_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
)

# Source files
set(SOURCE_FILES
    nx_agent_plugin.cpp
    nx_agent_config.cpp
    nx_agent_metadata.cpp
    nx_agent_anomaly.cpp
    nx_agent_main.cpp
)

# Create shared library (plugin)
add_library(nx_agent_plugin SHARED ${SOURCE_FILES})

# Link libraries
target_link_libraries(nx_agent_plugin
    ${OpenCV_LIBS}
    nlohmann_json::nlohmann_json
    ${NX_SDK_DIR}/lib/libnx_sdk.a
)

# Set output name based on platform
if(WIN32)
    set_target_properties(nx_agent_plugin PROPERTIES OUTPUT_NAME "nx_agent_plugin.dll")
elseif(APPLE)
    set_target_properties(nx_agent_plugin PROPERTIES 
        OUTPUT_NAME "nx_agent_plugin.dylib"
        SUFFIX ""
    )
else()
    set_target_properties(nx_agent_plugin PROPERTIES 
        OUTPUT_NAME "nx_agent_plugin.so"
        SUFFIX ""
    )
endif()

# Installation
install(TARGETS nx_agent_plugin
    LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/plugins
)

# Build options
option(BUILD_TESTS "Build test applications" OFF)

if(BUILD_TESTS)
    add_subdirectory(tests)
endif()

# Add README and license files to installation
install(FILES README.md LICENSE DESTINATION ${CMAKE_INSTALL_PREFIX}/docs)
