#pragma once

#include "Core/le_Element.hpp"

#include <complex>


namespace LogicElements {
/**
 * @brief Template class for Node, representing a node with history.
 *        Nodes act as buffers/storage elements that pass values through while maintaining history.
 * @tparam T The type of the values stored in the node.
 */
template <typename T>
class Node : public Element
{
LE_ELEMENT_ACCESS_MOD:
    /**
     * @brief Constructor that initializes the node with a specified history length.
     * @param type The element type for this node.
     * @param historyLength The length of the history buffer.
     */
    Node(ElementType type, uint16_t historyLength);

    /**
     * @brief Destructor to clean up the node.
     */
    ~Node();

    /**
     * @brief Updates the node with the current input value.
     * @param timeStamp The current timestamp.
     */
    virtual void Update(const Time& timeStamp) override;

public:
    /**
     * @brief Gets the history buffer with an offset from the circular buffer.
     * @param buffer Pointer to store the history buffer.
     * @param startOffset Pointer to store the start offset in the circular buffer.
     */
    void GetHistory(T* buffer, uint16_t* startOffset);

    /**
     * @brief Connects an output port of another element to the input of this node.
     * @param sourceElement The element to connect from.
     * @param sourcePortName The name of the output port on the source element.
     */
    void SetInput(Element* sourceElement, const std::string& sourcePortName);

    /**
     * @brief Overrides the current value of the node with a specified value for a given duration.
     *
     * This function sets an override value that will replace the current value of the node for a specified duration.
     * The override is initiated immediately and will persist until the duration expires.
     *
     * @tparam T The type of the value to be overridden.
     * @param value The value to override the current node value with.
     * @param duration The duration (in seconds) for which the override value should be used.
     */
    void OverrideValue(T value, float duration);

    /**
     * @brief Checks if override is currently active.
     *
     * @return bool if node is currently overridden.
     */
    bool IsOverridden()
    {
        return this->bOverride;
    }

    /**
     * @brief Gets the current value of the node.
     * @return The current value.
     */
    T GetValue() const;

    /**
     * @brief Sets the value of the node (typically called during Update).
     * @param value The new value.
     */
    void SetValue(T value);

    /**
     * @brief Gets the output value.
     * @return The output value.
     */
    T GetOutput() const;

private:
    InputPort<T>* pInput;   ///< The input port.
    OutputPort<T>* pOutput; ///< The output port.

    uint16_t uHistoryLength; ///< The length of the history buffer.
    T* _history;             ///< The history buffer.
    uint16_t uWrite;         ///< The current write position in the history buffer.
    
    float fOverrideTimer;
    float fOverrideDuration;
    T _overrideValue;
    T _overrideOriginalValue;
    bool bOverride;
    Time lastTimeStamp;   ///< The last timestamp when Update was called.

    // Allow le_Engine to access private members
    friend class Engine;
};

// Helper instantiations for digital and analog nodes

/**
 * @brief Digital node class inheriting from Node with bool type.
 */
class NodeDigital : public Node<bool>
{
public:
    /**
     * @brief Constructor for NodeDigital.
     * @param historyLength The length of the history buffer.
     */
    NodeDigital(uint16_t historyLength) : Node<bool>(ElementType::NodeDigital, historyLength) {}
};

#ifdef LE_ELEMENTS_ANALOG
/**
 * @brief Analog node class inheriting from Node with float type.
 */
class NodeAnalog : public Node<float>
{
public:
    /**
     * @brief Constructor for NodeAnalog.
     * @param historyLength The length of the history buffer.
     */
    NodeAnalog(uint16_t historyLength) : Node<float>(ElementType::NodeAnalog, historyLength) {}
};

#ifdef LE_ELEMENTS_ANALOG_COMPLEX
/**
 * @brief Analog complex node class inheriting from Node with complex float type.
 */
class NodeAnalogComplex : public Node<std::complex<float>>
{
public:
    /**
     * @brief Constructor for NodeAnalogComplex.
     * @param historyLength The length of the history buffer.
     */
    NodeAnalogComplex(uint16_t historyLength) : Node<std::complex<float>>(ElementType::NodeAnalogComplex, historyLength) {}
};
#endif // LE_ELEMENTS_ANALOG_COMPLEX
#endif // LE_ELEMENTS_ANALOG

// ============================================================================
// Implementation of the Node class
// ============================================================================

/**
 * @brief Constructor implementation that initializes the node with a specified history length.
 * @tparam T The type of the values stored in the node.
 * @param type The element type for this node.
 * @param historyLength The length of the history buffer.
 */
template<typename T>
Node<T>::Node(ElementType type, uint16_t historyLength) : Element(type)
{
    // Create input and output ports (using naming macros for consistency)
    pInput = this->template AddInputPort<T>(LE_PORT_INPUT_PREFIX);
    pOutput = this->template AddOutputPort<T>(LE_PORT_OUTPUT_PREFIX);

    // Fixed length
    if (historyLength <= 0)
        historyLength = 1;

    // Set extrinsic variables
    this->uHistoryLength = historyLength;

    // Set intrinsic variables
    this->_history = new T[historyLength];
    this->uWrite = 0;

    // Set override variables
    this->bOverride = false;
    this->fOverrideDuration = 0.0f;
    this->fOverrideTimer = 0.0f;
    this->_overrideOriginalValue = (T)0;
    this->_overrideValue = (T)0;

    // Initialize last timestamp
    this->lastTimeStamp = Time();
}

/**
 * @brief Destructor implementation to clean up the node.
 * @tparam T The type of the values stored in the node.
 */
template<typename T>
Node<T>::~Node()
{
    delete[] this->_history;
}

/**
 * @brief Updates the node with the current input value.
 * @param timeStamp The current timestamp.
 */
template<typename T>
void Node<T>::Update(const Time& timeStamp)
{
    // Calculate timeStep (int64_t to float conversion is intentional - microseconds to seconds)
    float timeStep = static_cast<float>((Time&)timeStamp - this->lastTimeStamp) / 1000000.0f;
    this->lastTimeStamp = timeStamp;

    // If override is set
    if (this->bOverride)
    {
        if (this->fOverrideTimer > this->fOverrideDuration)
        {
            this->SetValue(this->_overrideOriginalValue);
            this->bOverride = false;
        }
        else
        {
            // Set output value
            this->SetValue(this->_overrideValue);
            this->fOverrideTimer += timeStep;
        }
    }

    // Get input value
    if (pInput->IsConnected() && !this->bOverride)
    {
        T inputValue = pInput->GetValue();

        // Set next value
        this->SetValue(inputValue);
    }

    // Write to circular buffer and shift write head
    this->_history[this->uWrite] = this->GetValue();
    this->uWrite = (this->uWrite - 1 + this->uHistoryLength) % this->uHistoryLength;
}

/**
 * @brief Gets the history buffer with an offset from the circular buffer.
 * @param buffer Pointer to store the history buffer.
 * @param startOffset Pointer to store the start offset in the circular buffer.
 */
template<typename T>
void Node<T>::GetHistory(T* buffer, uint16_t* startOffset)
{
    // TODO: Implement this
}

/**
 * @brief Connects an output port of another element to the input of this node.
 * @param sourceElement The element to connect from.
 * @param sourcePortName The name of the output port on the source element.
 */
template<typename T>
void Node<T>::SetInput(Element* sourceElement, const std::string& sourcePortName)
{
    // Use default connection function
    Element::Connect(sourceElement, sourcePortName, this, "input");
}

/**
 * @brief Gets the current value of the node.
 * @return The current value.
 */
template<typename T>
T Node<T>::GetValue() const
{
    return pOutput->GetValue();
}

/**
 * @brief Sets the value of the node (typically called during Update).
 * @param value The new value.
 */
template<typename T>
void Node<T>::SetValue(T value)
{
    pOutput->SetValue(value);
}

/**
 * @brief Gets the output value.
 * @return The output value.
 */
template<typename T>
T Node<T>::GetOutput() const
{
    return pOutput->GetValue();
}

/**
 * @brief Overrides the current value of the node with a specified value for a given duration.
 *
 * This function sets an override value that will replace the current value of the node for a specified duration.
 * The override is initiated immediately and will persist until the duration expires.
 *
 * @tparam T The type of the value to be overridden.
 * @param value The value to override the current node value with.
 * @param duration The duration (in seconds) for which the override value should be used.
 */
template<typename T>
void Node<T>::OverrideValue(T value, float duration)
{
    this->_overrideValue = value; ///< The value to override the current node value with.
    this->_overrideOriginalValue = this->GetValue();
    this->fOverrideDuration = duration; ///< The duration (in seconds) for which the override value should be used.
    this->fOverrideTimer = 0.0f; ///< Timer to track the duration of the override.
    this->bOverride = true; ///< Flag indicating that the override is active.
}

} // namespace LogicElements
