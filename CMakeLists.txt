cmake_minimum_required(VERSION 3.11)

# Set the project name
project(Offset_Relay VERSION 1.0 LANGUAGES CXX)

# Add external libraries
add_subdirectory(external/opendnp3)
add_subdirectory(external/tinyexpr)
add_subdirectory(external/tiny-json)
add_subdirectory(external/Logic_Elements)

# Specify C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/external/opendnp3/cpp/lib/include
    ${CMAKE_SOURCE_DIR}/external/tinyexpr
    ${CMAKE_SOURCE_DIR}/external/tiny-json
    ${CMAKE_SOURCE_DIR}/external/Logic_Elements/include
    ${CMAKE_SOURCE_DIR}/src
)

# Set the executable
add_executable(${PROJECT_NAME} src/main.cpp)

# Link external libraries to the executable
target_link_libraries(${PROJECT_NAME} PRIVATE Logic_Elements)
