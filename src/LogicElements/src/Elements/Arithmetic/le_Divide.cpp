#include "Elements/Arithmetic/le_Divide.hpp"
#include <cmath>

namespace LogicElements {

#ifdef LE_ELEMENTS_ANALOG

/**
 * @brief Constructor implementation that initializes the Divide element with two inputs.
 */
Divide::Divide() : Element(ElementType::Divide)
{
    // Create named input ports
    AddInputPort<float>("input_0");
    AddInputPort<float>("input_1");

    // Create output port
    pOutput = AddOutputPort<float>("output");
}

/**
 * @brief Updates the Divide element. Calculates input_0 / input_1.
 * @param timeStamp The current timestamp.
 */
void Divide::Update(const Time& timeStamp)
{
    UNUSED(timeStamp);

    float val0 = 0.0f;
    float val1 = 1.0f; // Default to 1.0 to avoid accidental division by zero

    // Get input_0
    auto in0 = GetInputPortTyped<float>("input_0");
    if (in0 && in0->IsConnected())
        val0 = in0->GetValue();

    // Get input_1
    auto in1 = GetInputPortTyped<float>("input_1");
    if (in1 && in1->IsConnected())
        val1 = in1->GetValue();

    // Guard against divide-by-zero
    if (std::abs(val1) < 1e-10f)
    {
        pOutput->SetValue(0.0f);
        return;
    }

    // Set the output value
    pOutput->SetValue(val0 / val1);
}

/**
 * @brief Connects an output port of another element to a named input port of this Divide element.
 * @param sourceElement The element to connect from.
 * @param sourcePortName The name of the output port on the source element.
 * @param inputPortName The name of the input port on this Divide element.
 */
void Divide::SetInput(Element* sourceElement, const std::string& sourcePortName, const std::string& inputPortName)
{
    Element::Connect(sourceElement, sourcePortName, this, inputPortName);
}

/**
 * @brief Gets the output value.
 * @return The output value.
 */
float Divide::GetOutput() const
{
    return pOutput->GetValue();
}

#endif // LE_ELEMENTS_ANALOG

} // namespace LogicElements

