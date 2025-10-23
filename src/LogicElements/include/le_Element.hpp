#pragma once

// Include config
#include "le_Config.hpp"

// Include le_Time
#include "le_Time.hpp"

// Include le_Port
#include "le_Port.hpp"

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
 *          0 : Digital node element (Node<bool>)
 *          10 - 29 : Simple digital elements
 *          30 - 49 : Complex digital elements
 *          40 - 49 : Undefined
 *
 *          50 : Analog node element (Node<float>)
 *          51 : Analog complex node element (Node<std::complex<float>>)
 *          60 - 69 : Utility analog elements
 *          80 - 99 : Analog elements
 *          
 *
 *          100 - 119 : Protective Functions
 *
 *          -1 : Invalid
 */
enum class ElementType : int8_t {
    NodeDigital = 0,
    AND = 10,
    OR = 11,
    NOT = 12,
    RTrig = 13,
    FTrig = 14,
    Timer = 30,
    Counter = 31,
    MuxDigital = 32,
    SER = 49,
    NodeAnalog = 50,
#ifdef LE_ELEMENTS_ANALOG
    Rect2Polar = 60,
    Polar2Rect = 61,
    PhasorShift = 62,
    MuxAnalog = 63,
#ifdef LE_ELEMENTS_ANALOG_COMPLEX
    NodeAnalogComplex = 51,
    Complex2Rect = 64,
    Complex2Polar = 65,
    Rect2Complex = 66,
    Polar2Complex = 67,
    MuxAnalogComplex = 68,
#endif // LE_ELEMENTS_ANALOG_COMPLEX
#ifdef LE_ELEMENTS_MATH
    Math = 80,
#endif // LE_ELEMENTS_MATH
    Analog1PWinding = 81,
    Analog3PWinding = 82,
#ifdef LE_ELEMENTS_PID
    PID = 83,
#endif // LE_ELEMENTS_PID
    Overcurrent = 100,
#endif // LE_ELEMENTS_ANALOG
    Invalid = -1
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