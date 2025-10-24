cmake_minimum_required(VERSION 3.11)

# Manifest Generation for LogicElements
# This file generates LogicElements_manifest.json documenting the build configuration

# Find Python3
find_package(Python3 COMPONENTS Interpreter)

if(NOT Python3_FOUND)
    message(WARNING "Python3 not found - manifest generation will be skipped")
    message(WARNING "Install Python 3 to enable build manifest generation")
    return()
endif()

# Convert boolean CMake options to ON/OFF strings for Python
if(LE_ELEMENT_TEST_MODE)
    set(TEST_MODE_STR "ON")
else()
    set(TEST_MODE_STR "OFF")
endif()

if(LE_ENGINE_EXECUTION_DIAG)
    set(EXEC_DIAG_STR "ON")
else()
    set(EXEC_DIAG_STR "OFF")
endif()

if(LE_ELEMENTS_ANALOG)
    set(ANALOG_STR "ON")
else()
    set(ANALOG_STR "OFF")
endif()

if(LE_ELEMENTS_ANALOG_COMPLEX)
    set(ANALOG_COMPLEX_STR "ON")
else()
    set(ANALOG_COMPLEX_STR "OFF")
endif()

if(LE_ELEMENTS_PID)
    set(PID_STR "ON")
else()
    set(PID_STR "OFF")
endif()

if(LE_ELEMENTS_MATH)
    set(MATH_STR "ON")
else()
    set(MATH_STR "OFF")
endif()

if(LE_DNP3)
    set(DNP3_STR "ON")
else()
    set(DNP3_STR "OFF")
endif()

# Count source and header files
file(GLOB_RECURSE SOURCE_FILES "${CMAKE_SOURCE_DIR}/src/LogicElements/src/*.cpp")
list(FILTER SOURCE_FILES EXCLUDE REGEX ".*\\.in$")
list(LENGTH SOURCE_FILES SOURCE_FILE_COUNT)

file(GLOB_RECURSE HEADER_FILES "${CMAKE_SOURCE_DIR}/src/LogicElements/include/*.hpp")
list(FILTER HEADER_FILES EXCLUDE REGEX ".*\\.in$")
list(LENGTH HEADER_FILES HEADER_FILE_COUNT)

# Determine library output path and name
if(WIN32)
    set(LIB_EXT ".lib")
    set(LIB_PREFIX "")
elseif(APPLE)
    set(LIB_EXT ".a")
    set(LIB_PREFIX "lib")
else()
    set(LIB_EXT ".a")
    set(LIB_PREFIX "lib")
endif()

set(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}/src/LogicElements/${LIB_PREFIX}Logic_Elements${LIB_EXT}")

# Export variables to parent scope for POST_BUILD command
set(MANIFEST_PYTHON_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/generate_manifest.py" PARENT_SCOPE)
set(MANIFEST_OUTPUT_PATH "${CMAKE_BINARY_DIR}/LogicElements_manifest.json" PARENT_SCOPE)
set(MANIFEST_ELEMENT_HEADER "${CMAKE_SOURCE_DIR}/src/LogicElements/include/Core/le_Element.hpp" PARENT_SCOPE)
set(MANIFEST_VERSION "${LE_VERSION_STRING}" PARENT_SCOPE)
set(MANIFEST_LIB_OUTPUT "${LIBRARY_OUTPUT_PATH}" PARENT_SCOPE)
set(MANIFEST_BUILD_TYPE "${CMAKE_BUILD_TYPE}" PARENT_SCOPE)
set(MANIFEST_CXX_STD "${CMAKE_CXX_STANDARD}" PARENT_SCOPE)
set(MANIFEST_HOST_SYS "${CMAKE_SYSTEM_NAME}" PARENT_SCOPE)
set(MANIFEST_COMPILER "${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}" PARENT_SCOPE)
set(MANIFEST_BOARD_PLATFORM "${LE_BOARD_PLATFORM}" PARENT_SCOPE)
set(MANIFEST_TEST_MODE "${TEST_MODE_STR}" PARENT_SCOPE)
set(MANIFEST_EXEC_DIAG "${EXEC_DIAG_STR}" PARENT_SCOPE)
set(MANIFEST_ANALOG "${ANALOG_STR}" PARENT_SCOPE)
set(MANIFEST_ANALOG_COMPLEX "${ANALOG_COMPLEX_STR}" PARENT_SCOPE)
set(MANIFEST_PID "${PID_STR}" PARENT_SCOPE)
set(MANIFEST_MATH "${MATH_STR}" PARENT_SCOPE)
set(MANIFEST_DNP3 "${DNP3_STR}" PARENT_SCOPE)
set(MANIFEST_SOURCE_COUNT "${SOURCE_FILE_COUNT}" PARENT_SCOPE)
set(MANIFEST_HEADER_COUNT "${HEADER_FILE_COUNT}" PARENT_SCOPE)
set(MANIFEST_PYTHON_EXE "${Python3_EXECUTABLE}" PARENT_SCOPE)

# Display status
message(STATUS "========================================")
message(STATUS "Manifest Generation: AUTOMATIC")
message(STATUS "========================================")
message(STATUS "Python: ${Python3_EXECUTABLE}")
message(STATUS "Output: ${CMAKE_BINARY_DIR}/LogicElements_manifest.json")
message(STATUS "Will be generated automatically after library build")
message(STATUS "========================================")

