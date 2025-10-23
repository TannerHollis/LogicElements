#pragma once

#include "Core/le_Element.hpp"
#include "Core/le_Time.hpp"

namespace LogicElements {

/**
 * @brief Class representing a logical AND element.
 *        Performs logical AND operation on multiple boolean inputs.
 */
class AND : public Element
{
LE_ELEMENT_ACCESS_MOD:
    /**
     * @brief Constructor that initializes the AND element with a specified number of inputs.
     * @param nInputs Number of inputs for the AND element.
     */
    AND(uint8_t nInputs);

    /**
     * @brief Updates the AND element. Calculates the logical AND of all inputs.
     * @param timeStamp The current timestamp.
     */
    void Update(const Time& timeStamp) override;

public:
    /**
     * @brief Connects an output port of another element to a named input port of this AND element.
     * @param sourceElement The element to connect from.
     * @param sourcePortName The name of the output port on the source element.
     * @param inputPortName The name of the input port on this AND element.
     */
    void SetInput(Element* sourceElement, const std::string& sourcePortName, const std::string& inputPortName);

    /**
     * @brief Gets the output value.
     * @return The output value.
     */
    bool GetOutput() const;

private:
    OutputPort<bool>* pOutput; ///< The output port.

    // Allow Engine to access private members
    friend class Engine;
};
} // namespace LogicElements
