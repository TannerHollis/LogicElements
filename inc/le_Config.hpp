#pragma once

// Allow element constructors without use of the le_Engine factory
//#define LE_ELEMENT_TEST_MODE

/* 
* Allow engine execution diagnostics
*	User must define le_Engine::GetTime()
*/
#define LE_ENGINE_EXECUTION_DIAG

// Define a macro to handle weak attribute based on the compiler
#if defined(_MSC_VER)
#define WEAK_ATTR 
#elif defined(__GNUC__) || defined(__clang__)
#define WEAK_ATTR __attribute__((weak))
#endif