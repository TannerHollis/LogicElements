# tiny-expr CMake file
cmake_minimum_required(VERSION 3.11)

# Set the project name
project(tiny-json)

# Add the source files
add_library(tiny-json STATIC tiny-json.c tiny-json.h)