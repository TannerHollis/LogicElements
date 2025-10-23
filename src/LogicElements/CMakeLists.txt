cmake_minimum_required(VERSION 3.11)

# Set the project name and version
project(Logic_Elements VERSION 2.0.0 LANGUAGES CXX)

# Specify C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================================
# Configuration Options - Change these via CMake, not by editing source!
# ============================================================================

option(LE_ELEMENT_TEST_MODE "Enable element constructors without le_Engine factory" ON)
option(LE_ENGINE_EXECUTION_DIAG "Enable engine execution diagnostics" ON)
option(LE_ELEMENTS_ANALOG "Enable analog elements" ON)
option(LE_ELEMENTS_ANALOG_COMPLEX "Enable complex analog elements (requires ANALOG)" ON)
option(LE_ELEMENTS_PID "Enable PID controller (requires ANALOG)" ON)
option(LE_ELEMENTS_MATH "Enable Math expression evaluator (requires ANALOG)" ON)
option(LE_DNP3 "Enable DNP3 protocol support" ON)

# Version information (from project VERSION)
set(LE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(LE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(LE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(LE_VERSION_STRING "${LE_VERSION_MAJOR}.${LE_VERSION_MINOR}.${LE_VERSION_PATCH}")

# Build timestamp
string(TIMESTAMP LE_BUILD_DATE "%Y-%m-%d")
string(TIMESTAMP LE_BUILD_TIME "%H:%M:%S")

# Generate config header from template (now in Core/ subdirectory)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Core/le_Config.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/Core/le_Config.hpp"
    @ONLY
)

# Generate version header and source from templates (now in Core/ subdirectory)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Core/le_Version.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/Core/le_Version.hpp"
    @ONLY
)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Core/le_Version.cpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/src/Core/le_Version.cpp"
    @ONLY
)

# Display configuration
message(STATUS "========================================")
message(STATUS "Logic Elements Configuration")
message(STATUS "========================================")
message(STATUS "Version: ${LE_VERSION_STRING}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Build Date: ${LE_BUILD_DATE} ${LE_BUILD_TIME}")
message(STATUS "----------------------------------------")
message(STATUS "Test Mode: ${LE_ELEMENT_TEST_MODE}")
message(STATUS "Execution Diagnostics: ${LE_ENGINE_EXECUTION_DIAG}")
message(STATUS "Analog Elements: ${LE_ELEMENTS_ANALOG}")
if(LE_ELEMENTS_ANALOG)
    message(STATUS "  Complex Support: ${LE_ELEMENTS_ANALOG_COMPLEX}")
    message(STATUS "  PID Controller: ${LE_ELEMENTS_PID}")
    message(STATUS "  Math Evaluator: ${LE_ELEMENTS_MATH}")
endif()
message(STATUS "DNP3 Support: ${LE_DNP3}")
message(STATUS "========================================")

# Find all .cpp files in the specified directory (excluding templates)
file(GLOB_RECURSE CPP_FILES "./src/*.cpp")

# Remove template files from the list
list(FILTER CPP_FILES EXCLUDE REGEX ".*\\.in$")

# Add generated version source (now in Core/ subdirectory)
list(APPEND CPP_FILES "${CMAKE_CURRENT_BINARY_DIR}/src/Core/le_Version.cpp")

# Add the source files
add_library(${PROJECT_NAME} STATIC ${CPP_FILES})

# Include directories (include generated config and all subdirectories)
target_include_directories(${PROJECT_NAME} PUBLIC 
    ${CMAKE_SOURCE_DIR}/external/tinyexpr
    ${CMAKE_SOURCE_DIR}/external/opendnp3/cpp/lib/include
    ${CMAKE_SOURCE_DIR}/external/Minimal-Socket/src/header
    ${CMAKE_SOURCE_DIR}/external/serialib
    ${CMAKE_SOURCE_DIR}/external/nlohmann_json/include
    ./include                            # Root include directory
    ${CMAKE_CURRENT_BINARY_DIR}/include  # For generated headers (le_Config.hpp, le_Version.hpp)
)

# Link external libraries to the executable
target_link_libraries(${PROJECT_NAME} PRIVATE 
    tinyexpr 
    opendnp3 
    MinimalSocket 
    serialib 
    nlohmann_json
)
