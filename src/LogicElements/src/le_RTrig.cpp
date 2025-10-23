#include "le_RTrig.hpp"

namespace LogicElements {

/**
 * @brief Constructor implementation that initializes the RTrig element.
 */
RTrig::RTrig() : Element(ElementType::RTrig)
{
    // Create input port
    pInput = AddInputPort<bool>("input");

    // Create output port
    pOutput = AddOutputPort<bool>("output");

    // Initialize input states to false
    this->_inputStates[0] = false;
    this->_inputStates[1] = false;
}

/**
 * @brief Updates the RTrig element. Detects rising edge transitions.
 * @param timeStamp The current timestamp.
 */
void RTrig::Update(const Time& timeStamp)
{
    UNUSED(timeStamp);

    // Get the input value
    if (pInput->IsConnected())
    {
        bool inputValue = pInput->GetValue();

        // Shift input states
        this->_inputStates[1] = this->_inputStates[0];
        this->_inputStates[0] = inputValue;

        // Set the output value based on rising edge detection (low-to-high transition)
        pOutput->SetValue(this->_inputStates[0] && !this->_inputStates[1]);
    }
}

/**
 * @brief Connects an output port of another element to the input of this RTrig element.
 * @param sourceElement The element to connect from.
 * @param sourcePortName The name of the output port on the source element.
 */
void RTrig::SetInput(Element* sourceElement, const std::string& sourcePortName)
{
    Element::Connect(sourceElement, sourcePortName, this, "input");
}

/**
 * @brief Gets the output value.
 * @return The output value.
 */
bool RTrig::GetOutput() const
{
    return pOutput->GetValue();
}


} // namespace LogicElements
