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
option(LE_GENERATE_MANIFEST "Generate build configuration manifest JSON (requires Python3)" ON)

# Board HAL platform selection
set(LE_BOARD_PLATFORM "Generic" CACHE STRING "Board HAL platform: Generic, STM32, RPI, Arduino")
set_property(CACHE LE_BOARD_PLATFORM PROPERTY STRINGS "Generic" "STM32" "RPI" "Arduino")

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
message(STATUS "Board HAL Platform: ${LE_BOARD_PLATFORM}")
message(STATUS "Generate Manifest: ${LE_GENERATE_MANIFEST}")
message(STATUS "========================================")

# Find all .cpp files in the specified directory (excluding templates)
file(GLOB_RECURSE CPP_FILES "./src/*.cpp")

# Remove template files from the list
list(FILTER CPP_FILES EXCLUDE REGEX ".*\\.in$")

# Remove all HAL implementations (we'll add the selected one)
list(FILTER CPP_FILES EXCLUDE REGEX ".*/HAL/.*")

# Add platform-specific HAL implementation
if(LE_BOARD_PLATFORM STREQUAL "STM32")
    list(APPEND CPP_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/Device/HAL/le_BoardHAL_STM32.cpp")
    set(LE_BOARD_STM32 ON)
elseif(LE_BOARD_PLATFORM STREQUAL "RPI")
    list(APPEND CPP_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/Device/HAL/le_BoardHAL_RPI.cpp")
    set(LE_BOARD_RPI ON)
elseif(LE_BOARD_PLATFORM STREQUAL "Arduino")
    list(APPEND CPP_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/Device/HAL/le_BoardHAL_Arduino.cpp")
    set(LE_BOARD_ARDUINO ON)
else()  # Generic (default)
    list(APPEND CPP_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/Device/HAL/le_BoardHAL_Generic.cpp")
endif()

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

# ============================================================================
# Build Manifest Generation (Optional)
# ============================================================================

# Add manifest generation if enabled (requires Python3)
# Creates LogicElements_manifest.json documenting build configuration
if(LE_GENERATE_MANIFEST)
    # Include manifest generation (sets up variables)
    add_subdirectory(${CMAKE_SOURCE_DIR}/tools/manifest ${CMAKE_BINARY_DIR}/tools/manifest)
    
    # Add POST_BUILD command to Logic_Elements target (must be in same CMakeLists.txt as target)
    add_custom_command(TARGET Logic_Elements POST_BUILD
        COMMAND ${MANIFEST_PYTHON_EXE}
                "${MANIFEST_PYTHON_SCRIPT}"
                --output "${MANIFEST_OUTPUT_PATH}"
                --element-header "${MANIFEST_ELEMENT_HEADER}"
                --project-name "Logic_Elements"
                --version "${MANIFEST_VERSION}"
                --library-output "${MANIFEST_LIB_OUTPUT}"
                --build-type "${MANIFEST_BUILD_TYPE}"
                --cxx-standard "${MANIFEST_CXX_STD}"
                --host-system "${MANIFEST_HOST_SYS}"
                --compiler "${MANIFEST_COMPILER}"
                --board-platform "${MANIFEST_BOARD_PLATFORM}"
                --test-mode "${MANIFEST_TEST_MODE}"
                --execution-diag "${MANIFEST_EXEC_DIAG}"
                --analog "${MANIFEST_ANALOG}"
                --analog-complex "${MANIFEST_ANALOG_COMPLEX}"
                --pid "${MANIFEST_PID}"
                --math "${MANIFEST_MATH}"
                --dnp3 "${MANIFEST_DNP3}"
                --source-count "${MANIFEST_SOURCE_COUNT}"
                --header-count "${MANIFEST_HEADER_COUNT}"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/tools/manifest"
        COMMENT "Generating build configuration manifest..."
        VERBATIM
    )
else()
    message(STATUS "Manifest generation disabled (LE_GENERATE_MANIFEST=OFF)")
endif()
