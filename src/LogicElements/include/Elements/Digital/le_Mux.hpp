#pragma once

#include "Core/le_Element.hpp"


namespace LogicElements {
/**
 * @brief Class representing a multiplexer.
 *        Selects between multiple signal sets based on a boolean selector.
 *        This demonstrates HETEROGENEOUS PORTS - bool selector + typed signals!
 *
 * @tparam T The data type of the signal (e.g., bool, float, complex<float>).
 */
template <typename T>
class Mux : public Element
{
LE_ELEMENT_ACCESS_MOD:
    /**
     * @brief Constructor that initializes the Mux with specified signal width and number of inputs.
     * @param type The element type.
     * @param signalWidth The width of each signal set.
     * @param nInputs The number of input signal sets (typically 2).
     */
    Mux(ElementType type, uint8_t signalWidth, uint8_t nInputs);

    /**
     * @brief Destructor to clean up the multiplexer.
     */
    ~Mux();

    /**
     * @brief Updates the multiplexer with the current input value.
     * @param timeStamp The current timestamp.
     */
    void Update(const Time& timeStamp) override;

public:
    /**
     * @brief Connects an output port of another element to a signal input port.
     * @param sourceElement The element to connect from.
     * @param sourcePortName The name of the output port on the source element.
     * @param inputPortName The name of the input port on this multiplexer.
     */
    void SetInput(Element* sourceElement, const std::string& sourcePortName, const std::string& inputPortName);

    /**
     * @brief Connects an output port of another element to the select input.
     * @param sourceElement The element to connect from.
     * @param sourcePortName The name of the output port on the source element.
     */
    void SetInput_Select(Element* sourceElement, const std::string& sourcePortName);

    /**
     * @brief Gets the value of a specific output.
     * @param outputIndex The index of the output (0 to signalWidth-1).
     * @return The value of the output.
     */
    T GetOutput(uint8_t outputIndex) const;

private:
    uint8_t uSignalWidth; ///< The width of each signal set.
    uint8_t uNumInputSets;      ///< Number of input signal sets.
    
    InputPort<bool>* pSelector;  ///< The selector input (HETEROGENEOUS: bool in a T-typed element!)
    std::vector<OutputPort<T>*> pOutputs;  ///< The output ports.

    // Allow le_Engine to access private members
    friend class Engine;
};

/**
 * @brief Digital multiplexer class inheriting from Mux with bool type.
 */
class MuxDigital : public Mux<bool>
{
public:
    /**
     * @brief Constructor for ElementType::MuxDigital.
     * @param signalWidth The width of the signal.
     * @param nInputs The number of inputs to the multiplexer.
     */
    MuxDigital(uint8_t signalWidth, uint8_t nInputs) : Mux<bool>(ElementType::MuxDigital, signalWidth, nInputs) {}
};

#ifdef LE_ELEMENTS_ANALOG
/**
 * @brief Analog multiplexer class inheriting from Mux with float type.
 */
class MuxAnalog : public Mux<float>
{
public:
    /**
     * @brief Constructor for ElementType::MuxAnalog.
     * @param signalWidth The width of the signal.
     * @param nInputs The number of inputs to the multiplexer.
     */
    MuxAnalog(uint8_t signalWidth, uint8_t nInputs) : Mux<float>(ElementType::MuxAnalog, signalWidth, nInputs) {}
};

#ifdef LE_ELEMENTS_ANALOG_COMPLEX
/**
 * @brief Analog multiplexer class inheriting from Mux with float type.
 */
class MuxAnalogComplex : public Mux<std::complex<float>>
{
public:
    /**
     * @brief Constructor for ElementType::MuxAnalog.
     * @param signalWidth The width of the signal.
     * @param nInputs The number of inputs to the multiplexer.
     */
    MuxAnalogComplex(uint8_t signalWidth, uint8_t nInputs) : Mux<std::complex<float>>(ElementType::MuxAnalogComplex, signalWidth, nInputs) {}
};
#endif // LE_ELEMENTS_ANALOG_COMPLEX
#endif // LE_ELEMENTS_ANALOG

// ============================================================================
// Template Implementation
// ============================================================================

template<typename T>
Mux<T>::Mux(ElementType type, uint8_t signalWidth, uint8_t nInputs) : Element(type)
{
    // Set extrinsic variables
    this->uSignalWidth = signalWidth;
    this->uNumInputSets = nInputs;

    // Create signal input ports for each input set
    // Example: signalWidth=3, nInputs=2 creates:
    //   input_0_0, input_0_1, input_0_2  (first set)
    //   input_1_0, input_1_1, input_1_2  (second set)
    for (uint8_t inputSet = 0; inputSet < nInputs; inputSet++)
    {
        for (uint8_t signal = 0; signal < signalWidth; signal++)
        {
            std::string portName = LE_PORT_INPUT_2D_NAME(inputSet, signal);
            this->template AddInputPort<T>(portName);
        }
    }

    // Create the selector input (HETEROGENEOUS PORT: bool in a T-typed element!)
    pSelector = this->template AddInputPort<bool>(LE_PORT_SELECTOR_NAME);

    // Create output ports
    for (uint8_t i = 0; i < signalWidth; i++)
    {
        OutputPort<T>* port = this->template AddOutputPort<T>(LE_PORT_OUTPUT_NAME(i));
        pOutputs.push_back(port);
    }
}

/**
* @brief Destructor to clean up the multiplexer.
*/
template<typename T>
Mux<T>::~Mux() {}

/**
* @brief Updates the multiplexer with the current input value.
* @param timeStamp The current timestamp.
*/
template<typename T>
void Mux<T>::Update(const Time& timeStamp)
{
    UNUSED(timeStamp);

    // Check if selector is connected
    if (!pSelector->IsConnected())
        return;

    // Get selector value (false = first input set, true = second input set)
    bool selector = pSelector->GetValue();

    // Select which input set to use (0 or 1)
    uint8_t selectedSet = selector ? 1 : 0;

    // Copy selected input set to outputs
    for (uint8_t i = 0; i < uSignalWidth; i++)
    {
        // Calculate the port name for the selected input
        std::string portName = LE_PORT_INPUT_2D_NAME(selectedSet, i);
        
        // Get the input port
        InputPort<T>* inputPort = this->template GetInputPortTyped<T>(portName);
        
        if (inputPort && inputPort->IsConnected())
        {
            T inputValue = inputPort->GetValue();
            pOutputs[i]->SetValue(inputValue);
        }
    }
}

/**
* @brief Connects an output port of another element to a signal input port.
* @param sourceElement The element to connect from.
* @param sourcePortName The name of the output port on the source element.
* @param inputPortName The name of the input port on this multiplexer.
*/
template<typename T>
void Mux<T>::SetInput(Element* sourceElement, const std::string& sourcePortName, const std::string& inputPortName)
{
    Element::Connect(sourceElement, sourcePortName, this, inputPortName);
}

/**
* @brief Connects an output port of another element to the select input.
* @param sourceElement The element to connect from.
* @param sourcePortName The name of the output port on the source element.
*/
template<typename T>
void Mux<T>::SetInput_Select(Element* sourceElement, const std::string& sourcePortName)
{
    Element::Connect(sourceElement, sourcePortName, this, LE_PORT_SELECTOR_NAME);
}

/**
* @brief Gets the value of a specific output.
* @param outputIndex The index of the output (0 to signalWidth-1).
* @return The value of the output.
*/
template<typename T>
T Mux<T>::GetOutput(uint8_t outputIndex) const
{
    if (outputIndex < pOutputs.size())
        return pOutputs[outputIndex]->GetValue();
    return static_cast<T>(0);
}

} // namespace LogicElements
