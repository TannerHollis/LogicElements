#include "Elements/Conversions/le_Rect2Polar.hpp"

#ifdef LE_ELEMENTS_ANALOG

namespace LogicElements {

/**
 * @brief Constructs a Rect2Polar object.
 */
Rect2Polar::Rect2Polar() : Element(ElementType::Rect2Polar)
{
    // Create input ports
    pReal = AddInputPort<float>("real");
    pImag = AddInputPort<float>("imaginary");

    // Create output ports
    pMagnitude = AddOutputPort<float>("magnitude");
    pAngle = AddOutputPort<float>("angle");
}

/**
 * @brief Updates the Rect2Polar element.
 * @param timeStamp The current timestamp.
 */
void Rect2Polar::Update(const Time& timeStamp)
{
    UNUSED(timeStamp);

    // Check if inputs are connected
    if (pReal->IsConnected() && pImag->IsConnected())
    {
        float real = pReal->GetValue();
        float imag = pImag->GetValue();

        // Convert to polar coordinates
        float mag = sqrtf(real * real + imag * imag);
        float angle = atan2f(imag, real) * 180.0f / (float)M_PI;  // Convert to degrees

        pMagnitude->SetValue(mag);
        pAngle->SetValue(angle);
    }
}

/**
 * @brief Sets the real input for the Rect2Polar element.
 * @param sourceElement Pointer to the element providing the real part.
 * @param sourcePortName The name of the output port on the source element.
 */
void Rect2Polar::SetInput_Real(Element* sourceElement, const std::string& sourcePortName)
{
    Element::Connect(sourceElement, sourcePortName, this, "real");
}

/**
 * @brief Sets the imaginary input for the Rect2Polar element.
 * @param sourceElement Pointer to the element providing the imaginary part.
 * @param sourcePortName The name of the output port on the source element.
 */
void Rect2Polar::SetInput_Imag(Element* sourceElement, const std::string& sourcePortName)
{
    Element::Connect(sourceElement, sourcePortName, this, "imaginary");
}

/**
 * @brief Gets the magnitude output value.
 * @return The magnitude.
 */
float Rect2Polar::GetMagnitude() const
{
    return pMagnitude->GetValue();
}

/**
 * @brief Gets the angle output value.
 * @return The angle in degrees.
 */
float Rect2Polar::GetAngle() const
{
    return pAngle->GetValue();
}

#endif

} // namespace LogicElements
