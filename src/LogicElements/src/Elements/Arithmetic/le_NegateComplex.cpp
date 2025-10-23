#include "Elements/Arithmetic/le_NegateComplex.hpp"

namespace LogicElements {

#ifdef LE_ELEMENTS_ANALOG
#ifdef LE_ELEMENTS_ANALOG_COMPLEX

/**
 * @brief Constructor implementation that initializes the NegateComplex element with one input.
 */
NegateComplex::NegateComplex() : Element(ElementType::NegateComplex)
{
    // Create named input port
    AddInputPort<std::complex<float>>("input");

    // Create output port
    pOutput = AddOutputPort<std::complex<float>>("output");
}

/**
 * @brief Updates the NegateComplex element. Calculates -input.
 * @param timeStamp The current timestamp.
 */
void NegateComplex::Update(const Time& timeStamp)
{
    UNUSED(timeStamp);

    std::complex<float> val(0.0f, 0.0f);

    // Get input
    auto in = GetInputPortTyped<std::complex<float>>("input");
    if (in && in->IsConnected())
        val = in->GetValue();

    // Set the output value (negated)
    pOutput->SetValue(-val);
}

/**
 * @brief Connects an output port of another element to the input port of this NegateComplex element.
 * @param sourceElement The element to connect from.
 * @param sourcePortName The name of the output port on the source element.
 */
void NegateComplex::SetInput(Element* sourceElement, const std::string& sourcePortName)
{
    Element::Connect(sourceElement, sourcePortName, this, "input");
}

/**
 * @brief Gets the output value.
 * @return The output value.
 */
std::complex<float> NegateComplex::GetOutput() const
{
    return pOutput->GetValue();
}

#endif // LE_ELEMENTS_ANALOG_COMPLEX
#endif // LE_ELEMENTS_ANALOG

} // namespace LogicElements

