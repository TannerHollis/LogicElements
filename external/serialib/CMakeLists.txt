# serialib CMake file
cmake_minimum_required(VERSION 3.11)

# Set the project name
project(serialib)

# Add the source files
add_library(serialib STATIC serialib.cpp serialib.h)