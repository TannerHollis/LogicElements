#include "le_NOT.hpp"

namespace LogicElements {

/**
 * @brief Constructor that initializes the NOT element.
 */
NOT::NOT() : Element(ElementType::NOT)
{
    // Create input port
    pInput = AddInputPort<bool>("input");

    // Create output port
    pOutput = AddOutputPort<bool>("output");
}

/**
 * @brief Updates the NOT element. Calculates the logical NOT of the input.
 * @param timeStamp The current timestamp.
 */
void NOT::Update(const Time& timeStamp)
{
    UNUSED(timeStamp);

    // Get input value and invert it
    if (pInput->IsConnected())
    {
        bool inputValue = pInput->GetValue();
        pOutput->SetValue(!inputValue);
    }
}

/**
 * @brief Connects an output port of another element to the input of this NOT element.
 * @param sourceElement The element to connect from.
 * @param sourcePortName The name of the output port on the source element.
 */
void NOT::SetInput(Element* sourceElement, const std::string& sourcePortName)
{
    Element::Connect(sourceElement, sourcePortName, this, "input");
}

/**
 * @brief Gets the output value.
 * @return The output value.
 */
bool NOT::GetOutput() const
{
    return pOutput->GetValue();
}


} // namespace LogicElements
