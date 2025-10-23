#include "Elements/Arithmetic/le_Magnitude.hpp"
#include <cmath>

namespace LogicElements {

#ifdef LE_ELEMENTS_ANALOG
#ifdef LE_ELEMENTS_ANALOG_COMPLEX

/**
 * @brief Constructor implementation that initializes the Magnitude element with one input.
 */
Magnitude::Magnitude() : Element(ElementType::Magnitude)
{
    // Create named input port (COMPLEX type)
    pInput = AddInputPort<std::complex<float>>("input");

    // Create output port (FLOAT type) - HETEROGENEOUS!
    pOutput = AddOutputPort<float>("output");
}

/**
 * @brief Updates the Magnitude element. Calculates |input|.
 * @param timeStamp The current timestamp.
 */
void Magnitude::Update(const Time& timeStamp)
{
    UNUSED(timeStamp);

    std::complex<float> val(0.0f, 0.0f);

    // Get input
    if (pInput && pInput->IsConnected())
        val = pInput->GetValue();

    // Set the output value (magnitude/absolute value)
    pOutput->SetValue(std::abs(val));
}

/**
 * @brief Connects an output port of another element to the input port of this Magnitude element.
 * @param sourceElement The element to connect from.
 * @param sourcePortName The name of the output port on the source element.
 */
void Magnitude::SetInput(Element* sourceElement, const std::string& sourcePortName)
{
    Element::Connect(sourceElement, sourcePortName, this, "input");
}

/**
 * @brief Gets the output value.
 * @return The output value.
 */
float Magnitude::GetOutput() const
{
    return pOutput->GetValue();
}

#endif // LE_ELEMENTS_ANALOG_COMPLEX
#endif // LE_ELEMENTS_ANALOG

} // namespace LogicElements

