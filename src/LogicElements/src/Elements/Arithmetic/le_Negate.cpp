#include "Elements/Arithmetic/le_Negate.hpp"

namespace LogicElements {

#ifdef LE_ELEMENTS_ANALOG

/**
 * @brief Constructor implementation that initializes the Negate element with one input.
 */
Negate::Negate() : Element(ElementType::Negate)
{
    // Create named input port
    AddInputPort<float>("input");

    // Create output port
    pOutput = AddOutputPort<float>("output");
}

/**
 * @brief Updates the Negate element. Calculates -input.
 * @param timeStamp The current timestamp.
 */
void Negate::Update(const Time& timeStamp)
{
    UNUSED(timeStamp);

    float val = 0.0f;

    // Get input
    auto in = GetInputPortTyped<float>("input");
    if (in && in->IsConnected())
        val = in->GetValue();

    // Set the output value (negated)
    pOutput->SetValue(-val);
}

/**
 * @brief Connects an output port of another element to the input port of this Negate element.
 * @param sourceElement The element to connect from.
 * @param sourcePortName The name of the output port on the source element.
 */
void Negate::SetInput(Element* sourceElement, const std::string& sourcePortName)
{
    Element::Connect(sourceElement, sourcePortName, this, "input");
}

/**
 * @brief Gets the output value.
 * @return The output value.
 */
float Negate::GetOutput() const
{
    return pOutput->GetValue();
}

#endif // LE_ELEMENTS_ANALOG

} // namespace LogicElements

