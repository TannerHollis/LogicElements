#include "le_Complex2Rect.hpp"

#ifdef LE_ELEMENTS_ANALOG_COMPLEX

namespace LogicElements {

/**
 * @brief Constructs a Complex2Rect object.
 */
Complex2Rect::Complex2Rect() : Element(ElementType::Complex2Rect)
{
    // HETEROGENEOUS PORTS: complex input, float outputs
    pComplex = AddInputPort<std::complex<float>>("complex");
    pReal = AddOutputPort<float>("real");
    pImag = AddOutputPort<float>("imaginary");
}

/**
 * @brief Updates the Complex2Rect element.
 * @param timeStamp The current timestamp.
 */
void Complex2Rect::Update(const Time& timeStamp)
{
    UNUSED(timeStamp);

    if (pComplex->IsConnected())
    {
        std::complex<float> c = pComplex->GetValue();
        pReal->SetValue(c.real());
        pImag->SetValue(c.imag());
    }
}

/**
 * @brief Sets the complex input for the Complex2Rect element.
 * @param sourceElement Pointer to the element.
 * @param sourcePortName The name of the output port on the source element.
 */
void Complex2Rect::SetInput(Element* sourceElement, const std::string& sourcePortName)
{
    Element::Connect(sourceElement, sourcePortName, this, "complex");
}

/**
 * @brief Gets the real part output value.
 * @return The real part.
 */
float Complex2Rect::GetReal() const
{
    return pReal->GetValue();
}

/**
 * @brief Gets the imaginary part output value.
 * @return The imaginary part.
 */
float Complex2Rect::GetImaginary() const
{
    return pImag->GetValue();
}

#endif

} // namespace LogicElements
