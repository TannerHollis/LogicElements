# tiny-expr CMake file
cmake_minimum_required(VERSION 3.11)

# Set the project name
project(tinyexpr)

# Add the source files
add_library(tinyexpr STATIC tinyexpr.c tinyexpr.h)