#include "Elements/Arithmetic/le_Abs.hpp"
#include <cmath>

namespace LogicElements {

#ifdef LE_ELEMENTS_ANALOG

/**
 * @brief Constructor implementation that initializes the Abs element with one input.
 */
Abs::Abs() : Element(ElementType::Abs)
{
    // Create named input port and cache pointer
    pInput = AddInputPort<float>("input");

    // Create output port and cache pointer
    pOutput = AddOutputPort<float>(LE_PORT_OUTPUT_PREFIX);
}

/**
 * @brief Updates the Abs element. Calculates abs(input).
 * @param timeStamp The current timestamp.
 */
void Abs::Update(const Time& timeStamp)
{
    UNUSED(timeStamp);

    float val = 0.0f;

    // Get input (using cached pointer - no string lookup!)
    if (pInput && pInput->IsConnected())
        val = pInput->GetValue();

    // Set the output value (absolute value)
    pOutput->SetValue(std::abs(val));
}

/**
 * @brief Connects an output port of another element to the input port of this Abs element.
 * @param sourceElement The element to connect from.
 * @param sourcePortName The name of the output port on the source element.
 */
void Abs::SetInput(Element* sourceElement, const std::string& sourcePortName)
{
    Element::Connect(sourceElement, sourcePortName, this, "input");
}

/**
 * @brief Gets the output value.
 * @return The output value.
 */
float Abs::GetOutput() const
{
    return pOutput->GetValue();
}

#endif // LE_ELEMENTS_ANALOG

} // namespace LogicElements

