#pragma once

#include "le_Element.hpp"


namespace LogicElements {
/**
 * @brief Class representing a logical OR element.
 *        Performs logical OR operation on multiple boolean inputs.
 */
class OR : public Element
{
LE_ELEMENT_ACCESS_MOD:
    /**
     * @brief Constructor that initializes the OR element with a specified number of inputs.
     * @param nInputs Number of inputs for the OR element.
     */
    OR(uint8_t nInputs);

    /**
     * @brief Updates the OR element. Calculates the logical OR of all inputs.
     * @param timeStamp The current timestamp.
     */
    void Update(const Time& timeStamp) override;

public:
    /**
     * @brief Connects an output port of another element to a named input port of this OR element.
     * @param sourceElement The element to connect from.
     * @param sourcePortName The name of the output port on the source element.
     * @param inputPortName The name of the input port on this OR element.
     */
    void SetInput(Element* sourceElement, const std::string& sourcePortName, const std::string& inputPortName);

    /**
     * @brief Gets the output value.
     * @return The output value.
     */
    bool GetOutput() const;

private:
    OutputPort<bool>* pOutput; ///< The output port.

    // Allow le_Engine to access private members
    friend class Engine;
};

} // namespace LogicElements
