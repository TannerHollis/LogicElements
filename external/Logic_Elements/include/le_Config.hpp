#pragma once

/**
 * Enables use of element constructors without use of the le_Engine factory
 */
#define LE_ELEMENT_TEST_MODE

/**
 * Allow engine execution diagnostics
 *	User must define le_Engine::GetTime()
 */
#define LE_ENGINE_EXECUTION_DIAG

/**
* Enables use of the analog elements in library
*/
#define LE_ELEMENTS_ANALOG

#ifdef LE_ELEMENTS_ANALOG
/**
* Enables use of the complex output of elements in library
*	NOTE: LE_ELEMENTS_ANALOG must be defined as well
*/
#define LE_ELEMENTS_ANALOG_COMPLEX

/**
 * Enable use of the le_PID class
 *  NOTE: LE_ELEMENTS_ANALOG must be defined as well
 */
#define LE_ELEMENTS_PID

/**
* Enables the use of the le_Math class
*/
#define LE_ELEMENTS_MATH

#endif // LE_ELEMENTS_ANALOG

/**
 * Enable DNP3 session via le_DNP3Outstation
 */
//#define LE_DNP3