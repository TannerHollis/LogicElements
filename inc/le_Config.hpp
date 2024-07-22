#pragma once

// Enables use of element constructors without use of the le_Engine factory
//#define LE_ELEMENT_TEST_MODE

/* 
* Allow engine execution diagnostics
*	User must define le_Engine::GetTime()
*/
#define LE_ENGINE_EXECUTION_DIAG

// Enables the use of the digital elements
//#define LE_ELEMENTS_DIGITAL
 
// Enables the use of the analog elements
//#define LE_ELEMENTS_ANALOG