#pragma once

#include "Core/le_Element.hpp"

namespace LogicElements {

#ifdef LE_ELEMENTS_ANALOG

/**
 * @brief Class representing a subtraction element.
 *        Subtracts input_1 from input_0 and outputs the result.
 */
class Subtract : public Element
{
LE_ELEMENT_ACCESS_MOD:
    /**
     * @brief Constructor that initializes the Subtract element with two inputs.
     */
    Subtract();

    /**
     * @brief Updates the Subtract element. Calculates input_0 - input_1.
     * @param timeStamp The current timestamp.
     */
    void Update(const Time& timeStamp) override;

public:
    /**
     * @brief Connects an output port of another element to a named input port of this Subtract element.
     * @param sourceElement The element to connect from.
     * @param sourcePortName The name of the output port on the source element.
     * @param inputPortName The name of the input port on this Subtract element.
     */
    void SetInput(Element* sourceElement, const std::string& sourcePortName, const std::string& inputPortName);

    /**
     * @brief Gets the output value.
     * @return The output value (input_0 - input_1).
     */
    float GetOutput() const;

private:
    InputPort<float>* pInput0;  ///< The first input port (minuend).
    InputPort<float>* pInput1;  ///< The second input port (subtrahend).
    OutputPort<float>* pOutput; ///< The output port.

    // Allow Engine to access private members
    friend class Engine;
};

#endif // LE_ELEMENTS_ANALOG

} // namespace LogicElements

