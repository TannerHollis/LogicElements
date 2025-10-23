#include "Elements/Digital/le_AND.hpp"

namespace LogicElements {

/**
 * @brief Constructor implementation that initializes the AND element with a specified number of inputs.
 * @param nInputs Number of inputs for the AND element.
 */
AND::AND(uint8_t nInputs) : Element(ElementType::AND)
{
    // Create named input ports
    for (uint8_t i = 0; i < nInputs; i++)
    {
        AddInputPort<bool>(LE_PORT_INPUT_NAME(i));
    }

    // Create output port
    pOutput = AddOutputPort<bool>(LE_PORT_OUTPUT_PREFIX);
}

/**
 * @brief Updates the AND element. Calculates the logical AND of all inputs.
 * @param timeStamp The current timestamp.
 */
void AND::Update(const Time& timeStamp)
{
    UNUSED(timeStamp);

    // Set default to true
    bool nextValue = true;

    // Iterate through all input ports and apply logical AND
    for (Port* port : _inputPorts)
    {
        InputPort<bool>* inputPort = dynamic_cast<InputPort<bool>*>(port);
        if (inputPort && inputPort->IsConnected())
        {
            bool inputValue = inputPort->GetValue();
            nextValue &= inputValue;
        }
    }

    // Set the output value
    pOutput->SetValue(nextValue);
}

/**
 * @brief Connects an output port of another element to a named input port of this AND element.
 * @param sourceElement The element to connect from.
 * @param sourcePortName The name of the output port on the source element.
 * @param inputPortName The name of the input port on this AND element.
 */
void AND::SetInput(Element* sourceElement, const std::string& sourcePortName, const std::string& inputPortName)
{
    Element::Connect(sourceElement, sourcePortName, this, inputPortName);
}

/**
 * @brief Gets the output value.
 * @return The output value.
 */
bool AND::GetOutput() const
{
    return pOutput->GetValue();
}

} // namespace LogicElements
