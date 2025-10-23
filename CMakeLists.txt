cmake_minimum_required(VERSION 3.11)

# Set the project name
project(LogicElements VERSION 1.0 LANGUAGES CXX)

# Determine the operating system
if(WIN32)
    message(STATUS "Configuring for Windows")
    set(CONFIG_FILE "example_configs/example_config_windows.json")
elseif(UNIX AND NOT APPLE)
    message(STATUS "Configuring for Linux")
    set(CONFIG_FILE "example_configs/example_config_linux.json")
elseif(RASPBERRY_PI)
    message(STATUS "Configuring for Raspberry Pi")
    set(CONFIG_FILE "example_configs/example_config_rpi.json")
else()
    message(FATAL_ERROR "Unsupported operating system")
endif()

# Add external libraries
add_subdirectory(external/opendnp3)
add_subdirectory(external/tinyexpr)
add_subdirectory(external/nlohmann_json)
add_subdirectory(external/Minimal-Socket)
add_subdirectory(external/serialib)

# Add main LogicElements library
add_subdirectory(src/LogicElements)

# Specify C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/external/opendnp3/cpp/lib/include
    ${CMAKE_SOURCE_DIR}/external/tinyexpr
    ${CMAKE_SOURCE_DIR}/external/nlohmann_json/include
    ${CMAKE_SOURCE_DIR}/src/LogicElements/include
    ${CMAKE_SOURCE_DIR}/external/Minimal-Socket/src/header
    ${CMAKE_SOURCE_DIR}/external/serialib
    ${CMAKE_SOURCE_DIR}/src
)

# Set the executable
add_executable(${PROJECT_NAME} src/main.cpp)

# Link external libraries to the executable
target_link_libraries(${PROJECT_NAME} PRIVATE Logic_Elements)

# Option to build socket client (enabled by default)
option(BUILD_SOCKET_CLIENT "Build the socket client" ON)

if(BUILD_SOCKET_CLIENT)
    add_executable(socket_client src/socket_client.cpp)
    target_link_libraries(socket_client PRIVATE MinimalSocket Logic_Elements)
endif()

# Option to build serial client (enabled by default)
option(BUILD_SERIAL_CLIENT "Build the serial client" OFF)

if(BUILD_SERIAL_CLIENT)
    add_executable(serial_client src/serial_client.cpp)
    target_link_libraries(serial_client PRIVATE serialib Logic_Elements)
endif()

# Option to build tests (disabled by default)
option(BUILD_TESTS "Build the Logic Elements test suite" ON)

if(BUILD_TESTS)
    message(STATUS "Building Logic Elements tests")
    add_subdirectory(tests)
endif()

# Check if the config file exists before copying
if(EXISTS ${CMAKE_SOURCE_DIR}/${CONFIG_FILE})
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/${CONFIG_FILE} $<TARGET_FILE_DIR:${PROJECT_NAME}>
    )
else()
    message(FATAL_ERROR "Config file ${CONFIG_FILE} not found.")
endif()
