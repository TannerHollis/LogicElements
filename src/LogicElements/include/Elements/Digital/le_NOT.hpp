#pragma once

#include "Core/le_Element.hpp"


namespace LogicElements {
/**
 * @brief Class representing a logical NOT element.
 *        Performs logical NOT (inversion) operation on a boolean input.
 */
class NOT : public Element
{
LE_ELEMENT_ACCESS_MOD:
    /**
     * @brief Constructor that initializes the NOT element.
     */
    NOT();

    /**
     * @brief Updates the NOT element. Calculates the logical NOT of the input.
     * @param timeStamp The current timestamp.
     */
    void Update(const Time& timeStamp) override;

public:
    /**
     * @brief Connects an output port of another element to the input of this NOT element.
     * @param sourceElement The element to connect from.
     * @param sourcePortName The name of the output port on the source element.
     */
    void SetInput(Element* sourceElement, const std::string& sourcePortName);

    /**
     * @brief Gets the output value.
     * @return The output value.
     */
    bool GetOutput() const;

private:
    InputPort<bool>* pInput;   ///< The input port.
    OutputPort<bool>* pOutput; ///< The output port.

    // Allow le_Engine to access private members
    friend class Engine;
};

} // namespace LogicElements
