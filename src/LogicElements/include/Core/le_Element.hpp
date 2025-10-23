#pragma once

// Include config
#include "Core/le_Config.hpp"

// Include le_Time
#include "Core/le_Time.hpp"

// Include le_Port
#include "Core/le_Port.hpp"

// Include standard C++ libraries
#include <cstdint>
#include <cstdlib>
#include <vector>
#include <map>
#include <string>

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

namespace LogicElements {

/**
 * @brief Enum class to define the types of elements.
 *
 */
enum class ElementType : int8_t {
    // ========================================
    // NODE ELEMENTS (0-9)
    // Data storage and buffering elements
    // ========================================
    NodeDigital = 0,         ///< Digital node - stores boolean values with history
    
#ifdef LE_ELEMENTS_ANALOG
    NodeAnalog = 50,         ///< Analog node - stores float values with history
#ifdef LE_ELEMENTS_ANALOG_COMPLEX
    NodeAnalogComplex = 51,  ///< Complex analog node - stores complex<float> values with history
#endif // LE_ELEMENTS_ANALOG_COMPLEX
#endif // LE_ELEMENTS_ANALOG

    // ========================================
    // BASIC DIGITAL LOGIC (10-19)
    // Simple combinational logic gates
    // ========================================
    AND = 10,                ///< Logical AND gate - all inputs must be true
    OR = 11,                 ///< Logical OR gate - any input can be true
    NOT = 12,                ///< Logical NOT gate - inverts input
    RTrig = 13,              ///< Rising edge trigger - detects false-to-true transitions
    FTrig = 14,              ///< Falling edge trigger - detects true-to-false transitions

    // ========================================
    // ADVANCED DIGITAL LOGIC (30-49)
    // Complex digital elements with state
    // ========================================
    Timer = 30,              ///< Timer - generates timed pulses with configurable delays
    Counter = 31,            ///< Counter - counts pulses with preset and reset
    MuxDigital = 32,         ///< Digital multiplexer - selects between multiple boolean inputs
    SER = 49,                ///< Sequence of Events Recorder - records and timestamps digital events

#ifdef LE_ELEMENTS_ANALOG
    // ========================================
    // CONVERSION ELEMENTS (60-68)
    // Coordinate and type transformations
    // ========================================
    Rect2Polar = 60,         ///< Rectangular to polar conversion (x,y → r,θ)
    Polar2Rect = 61,         ///< Polar to rectangular conversion (r,θ → x,y)
#ifdef LE_ELEMENTS_ANALOG_COMPLEX
    Complex2Rect = 64,       ///< Complex to rectangular conversion (complex → x,y) [HETEROGENEOUS]
    Complex2Polar = 65,      ///< Complex to polar conversion (complex → r,θ) [HETEROGENEOUS]
    Rect2Complex = 66,       ///< Rectangular to complex conversion (x,y → complex) [HETEROGENEOUS]
    Polar2Complex = 67,      ///< Polar to complex conversion (r,θ → complex) [HETEROGENEOUS]
#endif // LE_ELEMENTS_ANALOG_COMPLEX

    // ========================================
    // FLOAT ARITHMETIC (69-74)
    // Basic arithmetic operations on float
    // ========================================
    Add = 69,                ///< Addition - computes input_0 + input_1
    Subtract = 70,           ///< Subtraction - computes input_0 - input_1
    Multiply = 71,           ///< Multiplication - computes input_0 * input_1
    Divide = 72,             ///< Division - computes input_0 / input_1 (with zero protection)
    Negate = 73,             ///< Negation - computes -input (unary minus)
    Abs = 74,                ///< Absolute value - computes |input|

#ifdef LE_ELEMENTS_ANALOG_COMPLEX
    // ========================================
    // COMPLEX ARITHMETIC (75-79, 84)
    // Basic arithmetic operations on complex
    // ========================================
    AddComplex = 75,         ///< Complex addition - computes input_0 + input_1
    SubtractComplex = 76,    ///< Complex subtraction - computes input_0 - input_1
    MultiplyComplex = 77,    ///< Complex multiplication - computes input_0 * input_1
    DivideComplex = 78,      ///< Complex division - computes input_0 / input_1 (with zero protection)
    NegateComplex = 79,      ///< Complex negation - computes -input (unary minus)
    Magnitude = 84,          ///< Complex magnitude - computes |complex| → float [HETEROGENEOUS]
#endif // LE_ELEMENTS_ANALOG_COMPLEX

    // ========================================
    // MULTIPLEXERS (32, 63, 68)
    // Signal routing and selection
    // ========================================
    MuxAnalog = 63,          ///< Analog multiplexer - selects between multiple float inputs
#ifdef LE_ELEMENTS_ANALOG_COMPLEX
    MuxAnalogComplex = 68,   ///< Complex multiplexer - selects between multiple complex inputs
#endif // LE_ELEMENTS_ANALOG_COMPLEX

    // ========================================
    // ADVANCED ANALOG PROCESSING (62, 80-83)
    // Specialized analog signal processing
    // ========================================
    PhasorShift = 62,        ///< Phasor phase shifter - rotates complex phasor by angle
#ifdef LE_ELEMENTS_MATH
    Math = 80,               ///< Mathematical expression evaluator - runtime equation parser
#endif // LE_ELEMENTS_MATH
    Analog1PWinding = 81,    ///< Single-phase winding model - power system transformer model
    Analog3PWinding = 82,    ///< Three-phase winding model - 3-phase power system transformer
#ifdef LE_ELEMENTS_PID
    PID = 83,                ///< PID controller - proportional-integral-derivative feedback control
#endif // LE_ELEMENTS_PID

    // ========================================
    // PROTECTION ELEMENTS (100+)
    // Protective relay and monitoring functions
    // ========================================
    Overcurrent = 100,       ///< Overcurrent protection - time-inverse overcurrent relay [HETEROGENEOUS]

#endif // LE_ELEMENTS_ANALOG

    // ========================================
    // SPECIAL VALUES
    // ========================================
    Invalid = -1             ///< Invalid/uninitialized element type
};

/**
 * @brief Base class for Element which represents an element with inputs and an update mechanism.
 *        Now supports named, heterogeneous ports.
 */
class Element
{
protected:
    /**
     * @brief Constructor that initializes the element.
     * @param type The type of the element.
     */
    Element(ElementType type);

    /**
     * @brief Virtual destructor to allow proper cleanup of derived classes.
     */
    virtual ~Element();

    /**
     * @brief Virtual function to update the element. Can be overridden by derived classes.
     * @param timeStamp The current timestamp.
     */
    virtual void Update(const class Time& timeStamp) = 0;

    /**
     * @brief Adds a typed input port to the element.
     * @tparam T The data type of the port.
     * @param name The name of the port.
     * @return Pointer to the created input port.
     */
    template<typename T>
    InputPort<T>* AddInputPort(const std::string& name);

    /**
     * @brief Adds a typed output port to the element.
     * @tparam T The data type of the port.
     * @param name The name of the port.
     * @return Pointer to the created output port.
     */
    template<typename T>
    OutputPort<T>* AddOutputPort(const std::string& name);

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
    static bool CompareElementOrders(Element* left, Element* right);

    /**
     * @brief Static function to connect the output of one element to the input of another element using port names.
     * @param outputElement The output element.
     * @param outputPortName The name of the output port.
     * @param inputElement The input element.
     * @param inputPortName The name of the input port.
     * @return True if connection was successful, false if type mismatch or port not found.
     */
    static bool Connect(Element* outputElement, const std::string& outputPortName,
                        Element* inputElement, const std::string& inputPortName);

    /**
     * @brief Gets the type of the specified element.
     * @param e The element to get the type of.
     * @return The type of the element.
     */
    static ElementType GetType(Element* e);

    /**
     * @brief Gets an input port by name.
     * @param name The name of the port.
     * @return Pointer to the port, or nullptr if not found.
     */
    Port* GetInputPort(const std::string& name);

    /**
     * @brief Gets an output port by name.
     * @param name The name of the port.
     * @return Pointer to the port, or nullptr if not found.
     */
    Port* GetOutputPort(const std::string& name);

    /**
     * @brief Gets a typed input port by name.
     * @tparam T The expected type of the port.
     * @param name The name of the port.
     * @return Pointer to the typed port, or nullptr if not found or type mismatch.
     */
    template<typename T>
    InputPort<T>* GetInputPortTyped(const std::string& name);

    /**
     * @brief Gets a typed output port by name.
     * @tparam T The expected type of the port.
     * @param name The name of the port.
     * @return Pointer to the typed port, or nullptr if not found or type mismatch.
     */
    template<typename T>
    OutputPort<T>* GetOutputPortTyped(const std::string& name);

    /**
     * @brief Gets the number of input ports.
     * @return Number of input ports.
     */
    size_t GetInputPortCount() const { return _inputPorts.size(); }

    /**
     * @brief Gets the number of output ports.
     * @return Number of output ports.
     */
    size_t GetOutputPortCount() const { return _outputPorts.size(); }

    /**
     * @brief Gets all input ports.
     * @return Vector of input port pointers.
     */
    const std::vector<Port*>& GetInputPorts() const { return _inputPorts; }

    /**
     * @brief Gets all output ports.
     * @return Vector of output port pointers.
     */
    const std::vector<Port*>& GetOutputPorts() const { return _outputPorts; }

private:
    /**
     * @brief Finds the order of the element recursively.
     * @param original The original element to start from.
     * @param order Pointer to store the order.
     */
    void FindOrder(Element* original, uint16_t* order);

    uint16_t iOrder;              ///< The update order of the element.

protected:
    ElementType type;                         ///< The type of the element.
    std::vector<Port*> _inputPorts;            ///< Vector of input ports.
    std::vector<Port*> _outputPorts;           ///< Vector of output ports.
    std::map<std::string, Port*> _inputPortsByName;   ///< Map of input ports by name.
    std::map<std::string, Port*> _outputPortsByName;  ///< Map of output ports by name.

    friend class Engine;
};

// ============================================================================
// Template Implementation
// ============================================================================

template<typename T>
InputPort<T>* Element::AddInputPort(const std::string& name)
{
    PortType portType = GetPortTypeFromCppType<T>();
    InputPort<T>* port = new InputPort<T>(name, portType, this);
    _inputPorts.push_back(port);
    _inputPortsByName[name] = port;
    return port;
}

template<typename T>
OutputPort<T>* Element::AddOutputPort(const std::string& name)
{
    PortType portType = GetPortTypeFromCppType<T>();
    OutputPort<T>* port = new OutputPort<T>(name, portType, this);
    _outputPorts.push_back(port);
    _outputPortsByName[name] = port;
    return port;
}

template<typename T>
InputPort<T>* Element::GetInputPortTyped(const std::string& name)
{
    auto it = _inputPortsByName.find(name);
    if (it != _inputPortsByName.end())
    {
        return dynamic_cast<InputPort<T>*>(it->second);
    }
    return nullptr;
}

template<typename T>
OutputPort<T>* Element::GetOutputPortTyped(const std::string& name)
{
    auto it = _outputPortsByName.find(name);
    if (it != _outputPortsByName.end())
    {
        return dynamic_cast<OutputPort<T>*>(it->second);
    }
    return nullptr;
}

} // namespace LogicElements