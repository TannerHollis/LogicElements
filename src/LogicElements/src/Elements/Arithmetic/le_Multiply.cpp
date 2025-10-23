#include "Elements/Arithmetic/le_Multiply.hpp"

namespace LogicElements {

#ifdef LE_ELEMENTS_ANALOG

/**
 * @brief Constructor implementation that initializes the Multiply element with two inputs.
 */
Multiply::Multiply() : Element(ElementType::Multiply)
{
    // Create named input ports and cache pointers
    pInput0 = AddInputPort<float>(LE_PORT_INPUT_NAME(0));
    pInput1 = AddInputPort<float>(LE_PORT_INPUT_NAME(1));

    // Create output port and cache pointer
    pOutput = AddOutputPort<float>(LE_PORT_OUTPUT_PREFIX);
}

/**
 * @brief Updates the Multiply element. Calculates input_0 * input_1.
 * @param timeStamp The current timestamp.
 */
void Multiply::Update(const Time& timeStamp)
{
    UNUSED(timeStamp);

    float val0 = 0.0f;
    float val1 = 0.0f;

    // Get input_0 (using cached pointer - no string lookup!)
    if (pInput0 && pInput0->IsConnected())
        val0 = pInput0->GetValue();

    // Get input_1 (using cached pointer - no string lookup!)
    if (pInput1 && pInput1->IsConnected())
        val1 = pInput1->GetValue();

    // Set the output value
    pOutput->SetValue(val0 * val1);
}

/**
 * @brief Connects an output port of another element to a named input port of this Multiply element.
 * @param sourceElement The element to connect from.
 * @param sourcePortName The name of the output port on the source element.
 * @param inputPortName The name of the input port on this Multiply element.
 */
void Multiply::SetInput(Element* sourceElement, const std::string& sourcePortName, const std::string& inputPortName)
{
    Element::Connect(sourceElement, sourcePortName, this, inputPortName);
}

/**
 * @brief Gets the output value.
 * @return The output value.
 */
float Multiply::GetOutput() const
{
    return pOutput->GetValue();
}

#endif // LE_ELEMENTS_ANALOG

} // namespace LogicElements

