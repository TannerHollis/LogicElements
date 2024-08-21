cmake_minimum_required(VERSION 3.11)

# Set the project name
project(Logic_Elements VERSION 1.0 LANGUAGES CXX)

# Specify C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find all .cpp files in the specified directory
file(GLOB_RECURSE CPP_FILES "./src/*.cpp")

# Add the source files
add_library(${PROJECT_NAME} STATIC ${CPP_FILES})

# Include directories
target_include_directories(${PROJECT_NAME} PUBLIC 
	"../tiny-json"
	"../tinyexpr"
	"../opendnp3/cpp/lib/include"
	"./include")

# Link external libraries to the executable
target_link_libraries(${PROJECT_NAME} PRIVATE tiny-json tinyexpr opendnp3)