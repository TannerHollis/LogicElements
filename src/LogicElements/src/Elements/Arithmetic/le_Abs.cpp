#include "Elements/Arithmetic/le_Abs.hpp"
#include <cmath>

namespace LogicElements {

#ifdef LE_ELEMENTS_ANALOG

/**
 * @brief Constructor implementation that initializes the Abs element with one input.
 */
Abs::Abs() : Element(ElementType::Abs)
{
    // Create named input port
    AddInputPort<float>("input");

    // Create output port
    pOutput = AddOutputPort<float>("output");
}

/**
 * @brief Updates the Abs element. Calculates abs(input).
 * @param timeStamp The current timestamp.
 */
void Abs::Update(const Time& timeStamp)
{
    UNUSED(timeStamp);

    float val = 0.0f;

    // Get input
    auto in = GetInputPortTyped<float>("input");
    if (in && in->IsConnected())
        val = in->GetValue();

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

