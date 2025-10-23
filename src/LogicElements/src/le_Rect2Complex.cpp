#include "le_Rect2Complex.hpp"

#ifdef LE_ELEMENTS_ANALOG_COMPLEX

namespace LogicElements {

/**
 * @brief Constructs a Rect2Complex object.
 */
Rect2Complex::Rect2Complex() : Element(ElementType::Rect2Complex)
{
    // HETEROGENEOUS PORTS: float inputs, complex output
    pReal = AddInputPort<float>("real");
    pImag = AddInputPort<float>("imaginary");
    pComplex = AddOutputPort<std::complex<float>>("complex");
}

/**
 * @brief Updates the Rect2Complex element.
 * @param timeStamp The current timestamp.
 */
void Rect2Complex::Update(const Time& timeStamp)
{
    UNUSED(timeStamp);

    if (pReal->IsConnected() && pImag->IsConnected())
    {
        float real = pReal->GetValue();
        float imag = pImag->GetValue();
        pComplex->SetValue(std::complex<float>(real, imag));
    }
}

/**
 * @brief Sets the real input for the Rect2Complex element.
 * @param sourceElement Pointer to the element.
 * @param sourcePortName The name of the output port on the source element.
 */
void Rect2Complex::SetInput_Real(Element* sourceElement, const std::string& sourcePortName)
{
    Element::Connect(sourceElement, sourcePortName, this, "real");
}

/**
 * @brief Sets the imaginary input for the Rect2Complex element.
 * @param sourceElement Pointer to the element.
 * @param sourcePortName The name of the output port on the source element.
 */
void Rect2Complex::SetInput_Imag(Element* sourceElement, const std::string& sourcePortName)
{
    Element::Connect(sourceElement, sourcePortName, this, "imaginary");
}

/**
 * @brief Gets the complex output value.
 * @return The complex value.
 */
std::complex<float> Rect2Complex::GetComplex() const
{
    return pComplex->GetValue();
}

#endif

} // namespace LogicElements
