/**
 * @file test_main.cpp
 * @brief Main test runner for ALL Logic Elements
 * 
 * Tests ALL 37+ elements using Engine factory pattern
 * Each element has individual test file
 * 
 * Validates:
 * - Named ports
 * - Heterogeneous port types (mixed bool/float/complex)
 * - Type-safe connections  
 * - Element functionality
 * - Factory instantiation via Engine
 */

#include "test_framework.hpp"

using namespace LogicElements;

// Forward declarations for all element test suites

// Simple Digital Elements (5 elements)
extern void RunANDTests();
extern void RunORTests();
extern void RunNOTTests();
extern void RunRTrigTests();
extern void RunFTrigTests();

// Complex Digital Elements (4 elements)
extern void RunTimerTests();
extern void RunCounterTests();
extern void RunMuxTests();
extern void RunSERTests();

// Node Elements (3 types)
extern void RunNodeTests();

// Arithmetic Elements (12 elements)
#ifdef LE_ELEMENTS_ANALOG
extern void RunAddTests();
extern void RunSubtractTests();
extern void RunMultiplyTests();
extern void RunDivideTests();
extern void RunNegateTests();
extern void RunAbsTests();
#ifdef LE_ELEMENTS_ANALOG_COMPLEX
extern void RunAddComplexTests();
extern void RunSubtractComplexTests();
extern void RunMultiplyComplexTests();
extern void RunDivideComplexTests();
extern void RunNegateComplexTests();
extern void RunMagnitudeTests();
#endif
#endif

// Conversion Elements (8 elements)
#ifdef LE_ELEMENTS_ANALOG
extern void RunRect2PolarTests();
extern void RunPolar2RectTests();
#ifdef LE_ELEMENTS_ANALOG_COMPLEX
extern void RunComplex2RectTests();
extern void RunRect2ComplexTests();
extern void RunComplex2PolarTests();
extern void RunPolar2ComplexTests();
#endif
#endif

// Control Elements (3 elements)
#ifdef LE_ELEMENTS_ANALOG
#ifdef LE_ELEMENTS_MATH
extern void RunMathTests();
#endif
#ifdef LE_ELEMENTS_PID
extern void RunPIDTests();
#endif
extern void RunOvercurrentTests();
#endif

// Phase Processing Elements
#ifdef LE_ELEMENTS_ANALOG
extern void RunPhasorShiftTests();
extern void RunAnalog1PWindingTests();
extern void RunAnalog3PWindingTests();
#endif

int main(int argc, char* argv[])
{
    std::cout << "========================================" << std::endl;
    std::cout << "Logic Elements Test Suite" << std::endl;
    std::cout << "Port-Based Architecture" << std::endl;
    std::cout << "Using Engine Factory Pattern" << std::endl;
    std::cout << "ALL Elements Tested Individually" << std::endl;
    std::cout << "========================================" << std::endl;

    // Simple Digital Elements
    std::cout << "\n>>> SIMPLE DIGITAL ELEMENTS <<<" << std::endl;
    RunANDTests();           // AND
    RunORTests();            // OR
    RunNOTTests();           // NOT
    RunRTrigTests();         // RTrig
    RunFTrigTests();         // FTrig

    // Complex Digital Elements
    std::cout << "\n>>> COMPLEX DIGITAL ELEMENTS <<<" << std::endl;
    RunTimerTests();         // Timer
    RunCounterTests();       // Counter
    RunMuxTests();           // le_Mux (HETEROGENEOUS!)
    RunSERTests();           // SER

    // Node Elements
    std::cout << "\n>>> NODE ELEMENTS <<<" << std::endl;
    RunNodeTests();          // le_Node (all variants)

#ifdef LE_ELEMENTS_ANALOG
    // Conversion Elements
    std::cout << "\n>>> CONVERSION ELEMENTS <<<" << std::endl;
    RunRect2PolarTests();    // Rect2Polar
    RunPolar2RectTests();    // Polar2Rect
    
#ifdef LE_ELEMENTS_ANALOG_COMPLEX
    RunComplex2RectTests();  // Complex2Rect (HETEROGENEOUS!)
    RunRect2ComplexTests();  // Rect2Complex (HETEROGENEOUS!)
    RunComplex2PolarTests(); // Complex2Polar (HETEROGENEOUS!)
    RunPolar2ComplexTests(); // Polar2Complex (HETEROGENEOUS!)
    RunMagnitudeTests();     // Magnitude (HETEROGENEOUS!)
#endif

    // Arithmetic Elements
    std::cout << "\n>>> ARITHMETIC ELEMENTS <<<" << std::endl;
    RunAddTests();           // Add
    RunSubtractTests();      // Subtract
    RunMultiplyTests();      // Multiply
    RunDivideTests();        // Divide
    RunNegateTests();        // Negate
    RunAbsTests();           // Abs
#ifdef LE_ELEMENTS_ANALOG_COMPLEX
    RunAddComplexTests();    // AddComplex
    RunSubtractComplexTests(); // SubtractComplex
    RunMultiplyComplexTests(); // MultiplyComplex
    RunDivideComplexTests(); // DivideComplex
    RunNegateComplexTests(); // NegateComplex
#endif

    // Control Elements
    std::cout << "\n>>> CONTROL ELEMENTS <<<" << std::endl;
#ifdef LE_ELEMENTS_MATH
    RunMathTests();          // Math
#endif
#ifdef LE_ELEMENTS_PID
    RunPIDTests();           // PID
#endif
    RunOvercurrentTests();   // Overcurrent (HETEROGENEOUS!)

    // Phase Processing Elements
    std::cout << "\n>>> PHASE PROCESSING ELEMENTS <<<" << std::endl;
    RunPhasorShiftTests();        // PhasorShift
    RunAnalog1PWindingTests();    // Analog1PWinding
    RunAnalog3PWindingTests();    // Analog3PWinding
#endif

    // Print summary
    std::cout << "\n";
    TestFramework::PrintSummary();

    std::cout << "\n========================================" << std::endl;
    std::cout << "Elements Tested:" << std::endl;
    std::cout << "  Simple Digital: 5 elements" << std::endl;
    std::cout << "  Complex Digital: 4 elements" << std::endl;
    std::cout << "  Nodes: 3 types (Digital, Analog, Complex)" << std::endl;
#ifdef LE_ELEMENTS_ANALOG
    std::cout << "  Arithmetic: 12 elements (6 float, 6 complex, Magnitude HETEROGENEOUS!)" << std::endl;
    std::cout << "  Conversions: 8 elements (6 HETEROGENEOUS!)" << std::endl;
    std::cout << "  Control: 3 elements (Overcurrent HETEROGENEOUS!)" << std::endl;
    std::cout << "  Phase/Winding: 3 elements" << std::endl;
    std::cout << "\nTotal: 38+ elements tested" << std::endl;
    std::cout << "Heterogeneous Elements: 9+ tested" << std::endl;
#else
    std::cout << "\nTotal: 12 digital elements tested" << std::endl;
#endif
    std::cout << "All tests use Engine factory pattern!" << std::endl;
    std::cout << "========================================" << std::endl;

    // Return exit code
    return TestFramework::AllPassed() ? 0 : 1;
}
