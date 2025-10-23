#pragma once

#include "Core/le_Element.hpp"

namespace LogicElements {

#ifdef LE_ELEMENTS_ANALOG

/**
 * @brief Class representing a division element.
 *        Divides input_0 by input_1 and outputs the result.
 *        Guards against divide-by-zero.
 */
class Divide : public Element
{
LE_ELEMENT_ACCESS_MOD:
    /**
     * @brief Constructor that initializes the Divide element with two inputs.
     */
    Divide();

    /**
     * @brief Updates the Divide element. Calculates input_0 / input_1.
     * @param timeStamp The current timestamp.
     */
    void Update(const Time& timeStamp) override;

public:
    /**
     * @brief Connects an output port of another element to a named input port of this Divide element.
     * @param sourceElement The element to connect from.
     * @param sourcePortName The name of the output port on the source element.
     * @param inputPortName The name of the input port on this Divide element.
     */
    void SetInput(Element* sourceElement, const std::string& sourcePortName, const std::string& inputPortName);

    /**
     * @brief Gets the output value.
     * @return The output value (input_0 / input_1, or 0 if input_1 is zero).
     */
    float GetOutput() const;

private:
    OutputPort<float>* pOutput; ///< The output port.

    // Allow Engine to access private members
    friend class Engine;
};

#endif // LE_ELEMENTS_ANALOG

} // namespace LogicElements

