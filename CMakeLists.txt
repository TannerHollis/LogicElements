cmake_minimum_required(VERSION 3.15)
project(LogicElements VERSION 2.0.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/hal
    ${CMAKE_CURRENT_SOURCE_DIR}/src/compiler/include
)

# Core Runtime Library (Zero-heap ANSI C PLC engine)
set(LOGIC_ELEMENTS_SOURCES
    src/runtime/src/le_rt.c
    src/runtime/src/le_process_image.c
    src/runtime/src/le_opcodes.c
    src/runtime/src/le_vm.c
    src/runtime/src/le_loader.c
    src/runtime/src/le_storage.c
    src/runtime/src/le_cli.c
    src/runtime/src/le_comms.c
    src/hal/le_hal_sim.c
)

add_library(LogicElements STATIC ${LOGIC_ELEMENTS_SOURCES})

if(UNIX AND NOT APPLE)
    target_link_libraries(LogicElements PRIVATE m)
endif()

# Simulator CLI Tool
add_executable(le_sim tools/sim/main.c)
target_link_libraries(le_sim PRIVATE LogicElements)
if(UNIX AND NOT APPLE)
    target_link_libraries(le_sim PRIVATE m)
endif()

# ==============================================================================
# Canonical Compiler Library (C++17 Core with C ABI)
# ==============================================================================
set(LE_COMPILER_SOURCES
    src/compiler/src/le_optimizer.cpp
    src/compiler/src/le_compiler_core.cpp
    src/compiler/src/le_disasm_core.cpp
    src/compiler/src/le_compiler_c_api.cpp
)

# Shared library for Python ctypes wrapper and external dynamic linking
add_library(le_compiler SHARED ${LE_COMPILER_SOURCES})
target_include_directories(le_compiler PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/compiler/include
)
target_include_directories(le_compiler PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/compiler/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/include
)
target_compile_definitions(le_compiler PRIVATE LE_COMPILER_EXPORTS)

# Static library for direct embedding without runtime DLL dependencies
add_library(le_compiler_static STATIC ${LE_COMPILER_SOURCES})
target_include_directories(le_compiler_static PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/compiler/include
)
target_include_directories(le_compiler_static PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/compiler/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/include
)
target_compile_definitions(le_compiler_static PUBLIC LE_COMPILER_STATIC)

# Standalone Compiler CLI Executable
add_executable(le_compile src/compiler/tools/le_compile_main.cpp)
target_link_libraries(le_compile PRIVATE le_compiler_static)
target_include_directories(le_compile PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/compiler/include
)

# ==============================================================================
# Automated Test Suite
# ==============================================================================
enable_testing()

# C Runtime unit tests
add_executable(test_c_runtime tests/test_c_runtime.c)
target_link_libraries(test_c_runtime PRIVATE LogicElements)
if(UNIX AND NOT APPLE)
    target_link_libraries(test_c_runtime PRIVATE m)
endif()
add_test(NAME CRuntimeTests COMMAND test_c_runtime)

# Compiler C++ unit tests
add_executable(test_compiler tests/test_compiler.cpp)
target_link_libraries(test_compiler PRIVATE le_compiler_static LogicElements)
target_include_directories(test_compiler PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/compiler/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/include
)
if(UNIX AND NOT APPLE)
    target_link_libraries(test_compiler PRIVATE m)
endif()
add_test(NAME CompilerTests COMMAND test_compiler)

# Comprehensive per-opcode test (needs compiler API for bytecode + disassembly)
add_executable(test_each_opcode tests/test_each_opcode.c)
target_link_libraries(test_each_opcode PRIVATE le_compiler_static LogicElements)
target_include_directories(test_each_opcode PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/compiler/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/include
)
if(UNIX AND NOT APPLE)
    target_link_libraries(test_each_opcode PRIVATE m)
endif()
add_test(NAME EachOpcodeTests COMMAND test_each_opcode)

# Per-element assembly verification: compiles one circuit fixture per element
# type and inspects the emitted disassembly for the expected opcode(s).
add_executable(test_element_assembly tests/test_element_assembly.c)
target_link_libraries(test_element_assembly PRIVATE le_compiler_static LogicElements)
target_include_directories(test_element_assembly PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/compiler/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/include
)
if(UNIX AND NOT APPLE)
    target_link_libraries(test_element_assembly PRIVATE m)
endif()
add_test(NAME ElementAssemblyTests
         COMMAND test_element_assembly ${CMAKE_CURRENT_SOURCE_DIR}/tests/element_fixtures)
