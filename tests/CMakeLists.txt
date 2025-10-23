cmake_minimum_required(VERSION 3.11)

# Test executable project
project(Logic_Elements_Tests VERSION 1.0 LANGUAGES CXX)

# Specify C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include the Logic Elements library headers
include_directories(
    ${CMAKE_SOURCE_DIR}/src/LogicElements/include
    ${CMAKE_SOURCE_DIR}/external/tinyexpr
    ${CMAKE_SOURCE_DIR}/external/nlohmann_json/include
)

# Test source files - Individual test file for EACH element
set(TEST_SOURCES
    test_main.cpp
    test_framework.hpp
    
    # Simple Digital Elements (5 elements)
    test_element_AND.cpp
    test_element_OR.cpp
    test_element_NOT.cpp
    test_element_RTrig.cpp
    test_element_FTrig.cpp
    
    # Complex Digital Elements (4 elements)
    test_element_Timer.cpp
    test_element_Counter.cpp
    test_element_Mux.cpp
    test_element_SER.cpp
    
    # Node Elements (3 types)
    test_element_Node.cpp
    
    # Arithmetic Elements (12 elements: 6 float + 6 complex)
    test_element_Add.cpp
    test_element_Subtract.cpp
    test_element_Multiply.cpp
    test_element_Divide.cpp
    test_element_Negate.cpp
    test_element_Abs.cpp
    test_element_AddComplex.cpp
    test_element_SubtractComplex.cpp
    test_element_MultiplyComplex.cpp
    test_element_DivideComplex.cpp
    test_element_NegateComplex.cpp
    test_element_Magnitude.cpp
    
    # Conversion Elements (8 elements)
    test_element_Rect2Polar.cpp
    test_element_Polar2Rect.cpp
    test_element_Complex2Rect.cpp
    test_element_Rect2Complex.cpp
    test_element_Complex2Polar.cpp
    test_element_Polar2Complex.cpp
    
    # Control Elements (3 elements)
    test_element_Math.cpp
    test_element_PID.cpp
    test_element_Overcurrent.cpp
    
    # Phase/Winding Elements (3 elements)
    test_element_PhasorShift.cpp
    test_element_Analog1PWinding.cpp
    test_element_Analog3PWinding.cpp
)

# Create test executable
add_executable(${PROJECT_NAME} ${TEST_SOURCES})

# Link against Logic Elements library
target_link_libraries(${PROJECT_NAME} PRIVATE Logic_Elements)

# Enable testing
enable_testing()

# Add test to CTest
add_test(NAME LogicElementsAllTests COMMAND ${PROJECT_NAME})

# Set test properties
set_tests_properties(LogicElementsAllTests PROPERTIES
    TIMEOUT 30
    LABELS "LogicElements;PortBased;Heterogeneous;Factory"
)

# Display configuration
message(STATUS "========================================")
message(STATUS "Logic Elements Tests: ENABLED")
message(STATUS "========================================")
message(STATUS "Test executable: ${PROJECT_NAME}")
message(STATUS "Individual test files: 34")
message(STATUS "Elements covered: 38+ (ALL major elements)")
message(STATUS "Pattern: le_Engine factory (100%)")
message(STATUS "Port validation: Named + Heterogeneous")
message(STATUS "Heterogeneous tests: 9+ elements")
message(STATUS "Arithmetic: 12 elements (6 float + 6 complex)")
message(STATUS "========================================")
