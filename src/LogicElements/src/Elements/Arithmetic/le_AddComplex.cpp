#include "Elements/Arithmetic/le_AddComplex.hpp"

namespace LogicElements {

#ifdef LE_ELEMENTS_ANALOG
#ifdef LE_ELEMENTS_ANALOG_COMPLEX

/**
 * @brief Constructor implementation that initializes the AddComplex element with two inputs.
 */
AddComplex::AddComplex() : Element(ElementType::AddComplex)
{
    // Create named input ports
    AddInputPort<std::complex<float>>("input_0");
    AddInputPort<std::complex<float>>("input_1");

    // Create output port
    pOutput = AddOutputPort<std::complex<float>>("output");
}

/**
 * @brief Updates the AddComplex element. Calculates input_0 + input_1.
 * @param timeStamp The current timestamp.
 */
void AddComplex::Update(const Time& timeStamp)
{
    UNUSED(timeStamp);

    std::complex<float> val0(0.0f, 0.0f);
    std::complex<float> val1(0.0f, 0.0f);

    // Get input_0
    auto in0 = GetInputPortTyped<std::complex<float>>("input_0");
    if (in0 && in0->IsConnected())
        val0 = in0->GetValue();

    // Get input_1
    auto in1 = GetInputPortTyped<std::complex<float>>("input_1");
    if (in1 && in1->IsConnected())
        val1 = in1->GetValue();

    // Set the output value
    pOutput->SetValue(val0 + val1);
}

/**
 * @brief Connects an output port of another element to a named input port of this AddComplex element.
 * @param sourceElement The element to connect from.
 * @param sourcePortName The name of the output port on the source element.
 * @param inputPortName The name of the input port on this AddComplex element.
 */
void AddComplex::SetInput(Element* sourceElement, const std::string& sourcePortName, const std::string& inputPortName)
{
    Element::Connect(sourceElement, sourcePortName, this, inputPortName);
}

/**
 * @brief Gets the output value.
 * @return The output value.
 */
std::complex<float> AddComplex::GetOutput() const
{
    return pOutput->GetValue();
}

#endif // LE_ELEMENTS_ANALOG_COMPLEX
#endif // LE_ELEMENTS_ANALOG

} // namespace LogicElements

