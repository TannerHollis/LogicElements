#pragma once

#include "Core/le_Element.hpp"

#ifdef LE_ELEMENTS_ANALOG


namespace LogicElements {
/**
 * @brief Class representing a conversion from rectangular to polar coordinates.
 *        Converts (real, imaginary) to (magnitude, angle).
 */
class Rect2Polar : public Element
{
LE_ELEMENT_ACCESS_MOD:
    /**
     * @brief Constructs a Rect2Polar object.
     */
    Rect2Polar();

    /**
     * @brief Updates the Rect2Polar element.
     * @param timeStamp The current timestamp.
     */
    void Update(const Time& timeStamp) override;

public:
    /**
     * @brief Sets the real input for the Rect2Polar element.
     * @param sourceElement Pointer to the element providing the real part.
     * @param sourcePortName The name of the output port on the source element.
     */
    void SetInput_Real(Element* sourceElement, const std::string& sourcePortName);

    /**
     * @brief Sets the imaginary input for the Rect2Polar element.
     * @param sourceElement Pointer to the element providing the imaginary part.
     * @param sourcePortName The name of the output port on the source element.
     */
    void SetInput_Imag(Element* sourceElement, const std::string& sourcePortName);

    /**
     * @brief Gets the magnitude output value.
     * @return The magnitude.
     */
    float GetMagnitude() const;

    /**
     * @brief Gets the angle output value.
     * @return The angle in degrees.
     */
    float GetAngle() const;

private:
    InputPort<float>* pReal;      ///< Real input port.
    InputPort<float>* pImag;      ///< Imaginary input port.
    OutputPort<float>* pMagnitude; ///< Magnitude output port.
    OutputPort<float>* pAngle;     ///< Angle output port.

    friend class Engine;
};
#endif

} // namespace LogicElements
