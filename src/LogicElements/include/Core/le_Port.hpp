#pragma once

#include <string>
#include <cstdint>
#include <complex>

namespace LogicElements {

// Forward declarations
class Element;

/**
 * @brief Enum defining the types of ports available in the system.
 */
enum class PortType : uint8_t {
    DIGITAL,      ///< Boolean (bool) type
    ANALOG,       ///< Floating-point (float) type
    COMPLEX       ///< Complex floating-point (std::complex<float>) type
};

/**
 * @brief Enum defining the direction of a port.
 */
enum class PortDirection : uint8_t {
    INPUT,        ///< Input port
    OUTPUT        ///< Output port
};

/**
 * @brief Base class for all ports (inputs and outputs).
 *        Provides common interface for port identification and type information.
 */
class Port
{
public:
    /**
     * @brief Constructor for Port.
     * @param name The name of the port.
     * @param type The data type of the port.
     * @param direction The direction of the port (input or output).
     * @param owner The element that owns this port.
     */
    Port(const std::string& name, PortType type, PortDirection direction, Element* owner);

    /**
     * @brief Virtual destructor.
     */
    virtual ~Port() = default;

    /**
     * @brief Gets the name of the port.
     * @return The port name.
     */
    const std::string& GetName() const { return sName; }

    /**
     * @brief Gets the type of the port.
     * @return The port type.
     */
    PortType GetType() const { return portType; }

    /**
     * @brief Gets the direction of the port.
     * @return The port direction.
     */
    PortDirection GetDirection() const { return portDirection; }

    /**
     * @brief Gets the owner element of the port.
     * @return Pointer to the owner element.
     */
    Element* GetOwner() const { return pOwner; }

protected:
    std::string sName;                    ///< Name of the port.
    PortType portType;                    ///< Type of data this port handles.
    PortDirection portDirection;          ///< Direction of the port.
    Element* pOwner;                      ///< Owner element.
};

/**
 * @brief Template class for typed output ports.
 * @tparam T The data type of the port (bool, float, std::complex<float>).
 */
template <typename T>
class OutputPort : public Port
{
public:
    /**
     * @brief Constructor for OutputPort.
     * @param name The name of the port.
     * @param type The data type of the port.
     * @param owner The element that owns this port.
     */
    OutputPort(const std::string& name, PortType type, Element* owner);

    /**
     * @brief Gets the current value of the output port.
     * @return The current value.
     */
    T GetValue() const { return value; }

    /**
     * @brief Sets the value of the output port.
     * @param newValue The new value to set.
     */
    void SetValue(T newValue) { value = newValue; }

private:
    T value; ///< The current value of the output port.
};

/**
 * @brief Template class for typed input ports.
 * @tparam T The data type of the port (bool, float, std::complex<float>).
 */
template <typename T>
class InputPort : public Port
{
public:
    /**
     * @brief Constructor for InputPort.
     * @param name The name of the port.
     * @param type The data type of the port.
     * @param owner The element that owns this port.
     */
    InputPort(const std::string& name, PortType type, Element* owner);

    /**
     * @brief Connects this input port to an output port.
     * @param source The output port to connect to.
     */
    void Connect(OutputPort<T>* source);

    /**
     * @brief Disconnects this input port.
     */
    void Disconnect();

    /**
     * @brief Checks if this input port is connected.
     * @return True if connected, false otherwise.
     */
    bool IsConnected() const { return pSource != nullptr; }

    /**
     * @brief Gets the value from the connected output port.
     * @return The value from the source port, or default value if not connected.
     */
    T GetValue() const;

    /**
     * @brief Gets the connected source output port.
     * @return Pointer to the source output port, or nullptr if not connected.
     */
    OutputPort<T>* GetSource() const { return pSource; }

private:
    OutputPort<T>* pSource; ///< Pointer to the connected output port.
};

// ============================================================================
// Template Implementation
// ============================================================================

/**
 * @brief Helper function to get PortType from C++ type.
 */
template <typename T>
constexpr PortType GetPortTypeFromCppType()
{
    if constexpr (std::is_same_v<T, bool>)
        return PortType::DIGITAL;
    else if constexpr (std::is_same_v<T, float>)
        return PortType::ANALOG;
    else if constexpr (std::is_same_v<T, std::complex<float>>)
        return PortType::COMPLEX;
    else
        static_assert(sizeof(T) == 0, "Unsupported port type");
}

// ----------------------------------------------------------------------------
// OutputPort Implementation
// ----------------------------------------------------------------------------

template <typename T>
OutputPort<T>::OutputPort(const std::string& name, PortType type, Element* owner)
    : Port(name, type, PortDirection::OUTPUT, owner), value(static_cast<T>(0))
{
}

// ----------------------------------------------------------------------------
// InputPort Implementation
// ----------------------------------------------------------------------------

template <typename T>
InputPort<T>::InputPort(const std::string& name, PortType type, Element* owner)
    : Port(name, type, PortDirection::INPUT, owner), pSource(nullptr)
{
}

template <typename T>
void InputPort<T>::Connect(OutputPort<T>* source)
{
    pSource = source;
}

template <typename T>
void InputPort<T>::Disconnect()
{
    pSource = nullptr;
}

template <typename T>
T InputPort<T>::GetValue() const
{
    if (pSource != nullptr)
        return pSource->GetValue();
    else
        return static_cast<T>(0); // Return default value if not connected
}

} // namespace LogicElements

