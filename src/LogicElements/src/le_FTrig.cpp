#include "le_FTrig.hpp"

namespace LogicElements {

/**
 * @brief Constructor implementation that initializes the FTrig element.
 */
FTrig::FTrig() : Element(ElementType::FTrig)
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
 * @brief Updates the FTrig element. Detects falling edge transitions.
 * @param timeStamp The current timestamp.
 */
void FTrig::Update(const Time& timeStamp)
{
    UNUSED(timeStamp);

    // Get the input value
    if (pInput->IsConnected())
    {
        bool inputValue = pInput->GetValue();

        // Shift input states
        this->_inputStates[1] = this->_inputStates[0];
        this->_inputStates[0] = inputValue;

        // Set the output value based on falling edge detection (high-to-low transition)
        pOutput->SetValue(!this->_inputStates[0] && this->_inputStates[1]);
    }
}

/**
 * @brief Connects an output port of another element to the input of this FTrig element.
 * @param sourceElement The element to connect from.
 * @param sourcePortName The name of the output port on the source element.
 */
void FTrig::SetInput(Element* sourceElement, const std::string& sourcePortName)
{
    Element::Connect(sourceElement, sourcePortName, this, "input");
}

/**
 * @brief Gets the output value.
 * @return The output value.
 */
bool FTrig::GetOutput() const
{
    return pOutput->GetValue();
}


} // namespace LogicElements
