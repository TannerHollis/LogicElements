#include "le_Polar2Rect.hpp"

#ifdef LE_ELEMENTS_ANALOG

#include <cmath>

namespace LogicElements {

/**
 * @brief Constructs a Polar2Rect object.
 */
Polar2Rect::Polar2Rect() : Element(ElementType::Polar2Rect)
{
    // Create input ports
    pMagnitude = AddInputPort<float>("magnitude");
    pAngle = AddInputPort<float>("angle");

    // Create output ports
    pReal = AddOutputPort<float>("real");
    pImag = AddOutputPort<float>("imaginary");
}

/**
 * @brief Updates the Polar2Rect element.
 * @param timeStamp The current timestamp.
 */
void Polar2Rect::Update(const Time& timeStamp)
{
    UNUSED(timeStamp);

    // Check if inputs are connected
    if (pMagnitude->IsConnected() && pAngle->IsConnected())
    {
        float mag = pMagnitude->GetValue();
        float angle = pAngle->GetValue() / 180.0f * (float)M_PI;  // Convert from degrees to radians

        // Convert to rectangular coordinates
        float real = mag * cosf(angle);
        float imag = mag * sinf(angle);

        pReal->SetValue(real);
        pImag->SetValue(imag);
    }
}

/**
 * @brief Sets the magnitude input for the Polar2Rect element.
 * @param sourceElement Pointer to the element providing the magnitude.
 * @param sourcePortName The name of the output port on the source element.
 */
void Polar2Rect::SetInput_Mag(Element* sourceElement, const std::string& sourcePortName)
{
    Element::Connect(sourceElement, sourcePortName, this, "magnitude");
}

/**
 * @brief Sets the angle input for the Polar2Rect element.
 * @param sourceElement Pointer to the element providing the angle.
 * @param sourcePortName The name of the output port on the source element.
 */
void Polar2Rect::SetInput_Angle(Element* sourceElement, const std::string& sourcePortName)
{
    Element::Connect(sourceElement, sourcePortName, this, "angle");
}

/**
 * @brief Gets the real part output value.
 * @return The real part.
 */
float Polar2Rect::GetReal() const
{
    return pReal->GetValue();
}

/**
 * @brief Gets the imaginary part output value.
 * @return The imaginary part.
 */
float Polar2Rect::GetImaginary() const
{
    return pImag->GetValue();
}

#endif

} // namespace LogicElements
