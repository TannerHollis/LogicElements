#pragma once

#include "Core/le_Element.hpp"
#include <complex>

namespace LogicElements {

#ifdef LE_ELEMENTS_ANALOG
#ifdef LE_ELEMENTS_ANALOG_COMPLEX

/**
 * @brief Class representing a complex negation element.
 *        Negates a single complex float input (unary minus).
 */
class NegateComplex : public Element
{
LE_ELEMENT_ACCESS_MOD:
    /**
     * @brief Constructor that initializes the NegateComplex element with one input.
     */
    NegateComplex();

    /**
     * @brief Updates the NegateComplex element. Calculates -input.
     * @param timeStamp The current timestamp.
     */
    void Update(const Time& timeStamp) override;

public:
    /**
     * @brief Connects an output port of another element to the input port of this NegateComplex element.
     * @param sourceElement The element to connect from.
     * @param sourcePortName The name of the output port on the source element.
     */
    void SetInput(Element* sourceElement, const std::string& sourcePortName);

    /**
     * @brief Gets the output value.
     * @return The output value (-input).
     */
    std::complex<float> GetOutput() const;

private:
    OutputPort<std::complex<float>>* pOutput; ///< The output port.

    // Allow Engine to access private members
    friend class Engine;
};

#endif // LE_ELEMENTS_ANALOG_COMPLEX
#endif // LE_ELEMENTS_ANALOG

} // namespace LogicElements

