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
# Host Communications Library (Zero-Qt Serial & Framed Packet Protocol)
# ==============================================================================
if(WIN32)
    set(LE_SERIAL_SOURCE src/cli/comms/src/le_serial_win.c)
else()
    set(LE_SERIAL_SOURCE src/cli/comms/src/le_serial_posix.c)
endif()

set(LE_HOST_COMMS_SOURCES
    src/cli/comms/src/le_host_comms.c
    ${LE_SERIAL_SOURCE}
)

add_library(le_host_comms STATIC ${LE_HOST_COMMS_SOURCES})
target_include_directories(le_host_comms PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cli/comms/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/include
)

# ==============================================================================
# Unified Host CLI Toolsuite (`le`)
# ==============================================================================
set(LE_CLI_SOURCES
    src/cli/main.cpp
    src/cli/commands/cmd_compile.cpp
    src/cli/commands/cmd_disasm.cpp
    src/cli/commands/cmd_board.cpp
    src/cli/commands/cmd_upload.cpp
    src/cli/commands/cmd_control.cpp
    src/cli/commands/cmd_monitor.cpp
    src/cli/commands/cmd_term.cpp
    src/cli/commands/cmd_sim.cpp
)

add_executable(le ${LE_CLI_SOURCES})
target_include_directories(le PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cli/commands
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cli/comms/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/compiler/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/hal
)
target_link_libraries(le PRIVATE le_compiler_static le_host_comms LogicElements)
if(UNIX AND NOT APPLE)
    target_link_libraries(le PRIVATE m)
endif()

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

# Host Comms unit tests
add_executable(test_host_comms tests/test_host_comms.c)
target_link_libraries(test_host_comms PRIVATE le_host_comms LogicElements)
target_include_directories(test_host_comms PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cli/comms/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/include
)
add_test(NAME HostCommsTests COMMAND test_host_comms)

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
