#pragma once

#include "Core/le_Element.hpp"

#ifdef LE_ELEMENTS_ANALOG_COMPLEX


namespace LogicElements {
/**
 * @brief Class representing a conversion from complex to rectangular (real, imaginary) components.
 *        HETEROGENEOUS ELEMENT: complex<float> input → float outputs!
 */
class Complex2Rect : public Element
{
LE_ELEMENT_ACCESS_MOD:
    /**
     * @brief Constructs a Complex2Rect object.
     */
    Complex2Rect();

    /**
     * @brief Updates the Complex2Rect element.
     * @param timeStamp The current timestamp.
     */
    void Update(const Time& timeStamp) override;

public:
    /**
     * @brief Sets the complex input for the Complex2Rect element.
     * @param sourceElement Pointer to the element.
     * @param sourcePortName The name of the output port on the source element.
     */
    void SetInput(Element* sourceElement, const std::string& sourcePortName);

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
    InputPort<std::complex<float>>* pComplex;  ///< Complex input port.
    OutputPort<float>* pReal;                   ///< Real output port (HETEROGENEOUS).
    OutputPort<float>* pImag;                   ///< Imaginary output port (HETEROGENEOUS).

    friend class Engine;
};
#endif

} // namespace LogicElements
