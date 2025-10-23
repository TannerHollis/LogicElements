#pragma once

#include "Core/le_Element.hpp"
#include <complex>

namespace LogicElements {

#ifdef LE_ELEMENTS_ANALOG
#ifdef LE_ELEMENTS_ANALOG_COMPLEX

/**
 * @brief Class representing a magnitude element.
 *        Computes the magnitude (absolute value) of a complex input.
 *        HETEROGENEOUS: Takes complex<float> input, outputs float.
 */
class Magnitude : public Element
{
LE_ELEMENT_ACCESS_MOD:
    /**
     * @brief Constructor that initializes the Magnitude element with one input.
     */
    Magnitude();

    /**
     * @brief Updates the Magnitude element. Calculates |input|.
     * @param timeStamp The current timestamp.
     */
    void Update(const Time& timeStamp) override;

public:
    /**
     * @brief Connects an output port of another element to the input port of this Magnitude element.
     * @param sourceElement The element to connect from.
     * @param sourcePortName The name of the output port on the source element.
     */
    void SetInput(Element* sourceElement, const std::string& sourcePortName);

    /**
     * @brief Gets the output value.
     * @return The output value (|input|).
     */
    float GetOutput() const;

private:
    InputPort<std::complex<float>>* pInput; ///< The input port (complex).
    OutputPort<float>* pOutput; ///< The output port (float) - HETEROGENEOUS!

    // Allow Engine to access private members
    friend class Engine;
};

#endif // LE_ELEMENTS_ANALOG_COMPLEX
#endif // LE_ELEMENTS_ANALOG

} // namespace LogicElements

