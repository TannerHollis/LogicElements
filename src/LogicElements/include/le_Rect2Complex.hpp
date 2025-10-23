#pragma once

#include "le_Element.hpp"

#ifdef LE_ELEMENTS_ANALOG_COMPLEX


namespace LogicElements {
/**
 * @brief Class representing a conversion from rectangular (real, imaginary) to complex.
 *        HETEROGENEOUS ELEMENT: float inputs → complex<float> output!
 */
class Rect2Complex : public Element
{
LE_ELEMENT_ACCESS_MOD:
    /**
     * @brief Constructs a Rect2Complex object.
     */
    Rect2Complex();

    /**
     * @brief Updates the Rect2Complex element.
     * @param timeStamp The current timestamp.
     */
    void Update(const Time& timeStamp) override;

public:
    /**
     * @brief Sets the real input for the Rect2Complex element.
     * @param sourceElement Pointer to the element.
     * @param sourcePortName The name of the output port on the source element.
     */
    void SetInput_Real(Element* sourceElement, const std::string& sourcePortName);

    /**
     * @brief Sets the imaginary input for the Rect2Complex element.
     * @param sourceElement Pointer to the element.
     * @param sourcePortName The name of the output port on the source element.
     */
    void SetInput_Imag(Element* sourceElement, const std::string& sourcePortName);

    /**
     * @brief Gets the complex output value.
     * @return The complex value.
     */
    std::complex<float> GetComplex() const;

private:
    InputPort<float>* pReal;                          ///< Real input port (HETEROGENEOUS).
    InputPort<float>* pImag;                          ///< Imaginary input port (HETEROGENEOUS).
    OutputPort<std::complex<float>>* pComplex;        ///< Complex output port.

    friend class Engine;
};
#endif

} // namespace LogicElements
