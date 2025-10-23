#pragma once

#include "Core/le_Element.hpp"
#include <complex>

namespace LogicElements {

#ifdef LE_ELEMENTS_ANALOG
#ifdef LE_ELEMENTS_ANALOG_COMPLEX

/**
 * @brief Class representing a complex addition element.
 *        Adds two complex float inputs and outputs the result.
 */
class AddComplex : public Element
{
LE_ELEMENT_ACCESS_MOD:
    /**
     * @brief Constructor that initializes the AddComplex element with two inputs.
     */
    AddComplex();

    /**
     * @brief Updates the AddComplex element. Calculates input_0 + input_1.
     * @param timeStamp The current timestamp.
     */
    void Update(const Time& timeStamp) override;

public:
    /**
     * @brief Connects an output port of another element to a named input port of this AddComplex element.
     * @param sourceElement The element to connect from.
     * @param sourcePortName The name of the output port on the source element.
     * @param inputPortName The name of the input port on this AddComplex element.
     */
    void SetInput(Element* sourceElement, const std::string& sourcePortName, const std::string& inputPortName);

    /**
     * @brief Gets the output value.
     * @return The output value (input_0 + input_1).
     */
    std::complex<float> GetOutput() const;

private:
    InputPort<std::complex<float>>* pInput0;  ///< The first input port.
    InputPort<std::complex<float>>* pInput1;  ///< The second input port.
    OutputPort<std::complex<float>>* pOutput; ///< The output port.

    // Allow Engine to access private members
    friend class Engine;
};

#endif // LE_ELEMENTS_ANALOG_COMPLEX
#endif // LE_ELEMENTS_ANALOG

} // namespace LogicElements

