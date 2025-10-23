#pragma once

#include "Core/le_Element.hpp"

namespace LogicElements {

#ifdef LE_ELEMENTS_ANALOG

/**
 * @brief Class representing an addition element.
 *        Adds two float inputs and outputs the result.
 */
class Add : public Element
{
LE_ELEMENT_ACCESS_MOD:
    /**
     * @brief Constructor that initializes the Add element with two inputs.
     */
    Add();

    /**
     * @brief Updates the Add element. Calculates input_0 + input_1.
     * @param timeStamp The current timestamp.
     */
    void Update(const Time& timeStamp) override;

public:
    /**
     * @brief Connects an output port of another element to a named input port of this Add element.
     * @param sourceElement The element to connect from.
     * @param sourcePortName The name of the output port on the source element.
     * @param inputPortName The name of the input port on this Add element.
     */
    void SetInput(Element* sourceElement, const std::string& sourcePortName, const std::string& inputPortName);

    /**
     * @brief Gets the output value.
     * @return The output value (input_0 + input_1).
     */
    float GetOutput() const;

private:
    OutputPort<float>* pOutput; ///< The output port.

    // Allow Engine to access private members
    friend class Engine;
};

#endif // LE_ELEMENTS_ANALOG

} // namespace LogicElements

