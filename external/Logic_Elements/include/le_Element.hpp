#pragma once

// Include config
#include "le_Config.hpp"

// Include le_Time
#include "le_Time.hpp"

// Include standard C++ libraries
#include <cstdint>
#include <cstdlib>

#ifdef LE_ELEMENTS_ANALOG
#include <cmath>
#ifdef LE_ELEMENTS_ANALOG_COMPLEX
#include <complex>
#endif // LE_ELEMENTS_ANALOG_COMPLEX
#endif // LE_ELEMENTS_ANALOG

// Define M_PI
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Unused assert parameters
#ifndef UNUSED
#define UNUSED(x) (void)(x)
#endif

// Whether or not test mode is enabled, public functions will be available
#ifdef LE_ELEMENT_TEST_MODE
#define LE_ELEMENT_ACCESS_MOD public
#else
#define LE_ELEMENT_ACCESS_MOD protected
#endif

/**
 * @brief Enum class to define the types of elements.
 *          0 : Base digital element le_Base<bool>
 *          10 - 29 : Simple digital elements
 *          30 - 49 : Complex digital elements
 *          40 - 49 : Undefined
 *
 *          50 : Base analog element le_Base<float>
 *          51 : Base analog complex element le_Base<std::complex<float>>
 *          60 - 69 : Utility analog elements
 *          80 - 99 : Analog elements
 *          
 *
 *          100 - 119 : Protective Functions
 *
 *          -1 : Invalid
 */
enum class le_Element_Type : int8_t {
    LE_NODE_DIGITAL = 0,
    LE_AND = 10,
    LE_OR = 11,
    LE_NOT = 12,
    LE_RTRIG = 13,
    LE_FTRIG = 14,
    LE_TIMER = 30,
    LE_COUNTER = 31,
    LE_MUX_DIGITAL = 32,
    LE_SER = 49,
    LE_NODE_ANALOG = 50,
#ifdef LE_ELEMENTS_ANALOG
    LE_R2P = 60,
    LE_P2R = 61,
    LE_PHASOR_SHIFT = 62,
    LE_MUX_ANALOG = 63,
#ifdef LE_ELEMENTS_ANALOG_COMPLEX
    LE_NODE_ANALOG_COMPLEX = 51,
    LE_C2R = 64,
    LE_C2P = 65,
    LE_R2C = 66,
    LE_P2C= 67,
    LE_MUX_ANALOG_COMPLEX = 68,
#endif // LE_ELEMENTS_ANALOG_COMPLEX
#ifdef LE_ELEMENTS_MATH
    LE_MATH = 80,
#endif // LE_ELEMENTS_MATH
    LE_ANALOG_1P = 81,
    LE_ANALOG_3P = 82,
#ifdef LE_ELEMENTS_PID
    LE_PID = 83,
#endif // LE_ELEMENTS_PID
    LE_OVERCURRENT = 100,
#endif // LE_ELEMENTS_ANALOG
    LE_INVALID = -1
};

/**
 * @brief Base class for le_Element which represents an element with inputs and an update mechanism.
 */
class le_Element
{
protected:
    /**
     * @brief Constructor that initializes the element with a specified number of inputs.
     * @param nInputs Number of inputs for the element.
     */
    le_Element(le_Element_Type type, uint8_t nInputs);

    /**
     * @brief Virtual destructor to allow proper cleanup of derived classes.
     */
    virtual ~le_Element();

    /**
     * @brief Virtual function to update the element. Can be overridden by derived classes.
     * @param timeStamp The current timestamp.
     */
    virtual void Update(const le_Time& timeStamp) = 0;

public:
    /**
     * @brief Gets the update order of the element.
     * @return The update order.
     */
    uint16_t GetOrder();

    /**
     * @brief Comparison operator for ordering elements by their update order.
     * @param left The left-hand element to compare with.
     * @param right The right-hand element to compare with.
     * @return True if this element has a smaller update order than the other element.
     */
    static bool CompareElementOrders(le_Element* left, le_Element* right);

    /**
     * @brief Static function to connect the output of one element to the input of another element.
     * @param output The output element.
     * @param outputSlot The slot of the output element.
     * @param input The input element.
     * @param inputSlot The slot of the input element.
     */
    static void Connect(le_Element* output, uint8_t outputSlot, le_Element* input, uint8_t inputSlot);

    /**
     * @brief Gets the type of the specified element.
     * @param e The element to get the type of.
     * @return The type of the element.
     */
    static le_Element_Type GetType(le_Element* e);

private:
    /**
     * @brief Finds the order of the element recursively.
     * @param original The original element to start from.
     * @param order Pointer to store the order.
     */
    void FindOrder(le_Element* original, uint16_t* order);

    uint16_t iOrder;              ///< The update order of the element.

protected:
    le_Element_Type type;
    uint8_t nInputs;             ///< Number of inputs.
    le_Element** _inputs;         ///< Array of pointers to input elements.
    uint8_t* _outputSlots;       ///< Array of output slots.

    friend class le_Engine;
};