#pragma once

#include "le_Element.hpp"

#ifdef LE_ELEMENTS_ANALOG


namespace LogicElements {
/**
 * @brief Class representing a conversion from polar to rectangular coordinates.
 *        Converts (magnitude, angle) to (real, imaginary).
 */
class Polar2Rect : public Element
{
LE_ELEMENT_ACCESS_MOD:
    /**
     * @brief Constructs a Polar2Rect object.
     */
    Polar2Rect();

    /**
     * @brief Updates the Polar2Rect element.
     * @param timeStamp The current timestamp.
     */
    void Update(const Time& timeStamp) override;

public:
    /**
     * @brief Sets the magnitude input for the Polar2Rect element.
     * @param sourceElement Pointer to the element providing the magnitude.
     * @param sourcePortName The name of the output port on the source element.
     */
    void SetInput_Mag(Element* sourceElement, const std::string& sourcePortName);

    /**
     * @brief Sets the angle input for the Polar2Rect element.
     * @param sourceElement Pointer to the element providing the angle.
     * @param sourcePortName The name of the output port on the source element.
     */
    void SetInput_Angle(Element* sourceElement, const std::string& sourcePortName);

    /**
     * @brief Gets the real part output value.
     * @return The real part.
     */
    float GetReal() const;

    /**
     * @brief Gets the imaginary part output value.
     * @return The imaginary part.
     */
    float GetImaginary() const;

private:
    InputPort<float>* pMagnitude; ///< Magnitude input port.
    InputPort<float>* pAngle;     ///< Angle input port.
    OutputPort<float>* pReal;     ///< Real output port.
    OutputPort<float>* pImag;     ///< Imaginary output port.

    friend class Engine;
};
#endif

} // namespace LogicElements
