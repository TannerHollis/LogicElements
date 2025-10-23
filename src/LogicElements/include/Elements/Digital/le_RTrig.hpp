#pragma once

#include "Core/le_Element.hpp"


namespace LogicElements {
/**
 * @brief Class representing a rising edge trigger (RTrig) element.
 *        Detects rising edge (low-to-high) transitions on a boolean input.
 */
class RTrig : public Element
{
LE_ELEMENT_ACCESS_MOD:
    /**
     * @brief Constructor that initializes the RTrig element.
     */
    RTrig();

    /**
     * @brief Updates the RTrig element. Detects rising edge transitions.
     * @param timeStamp The current timestamp.
     */
    void Update(const Time& timeStamp) override;

public:
    /**
     * @brief Connects an output port of another element to the input of this RTrig element.
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
    bool _inputStates[2];          ///< Stores the previous and current input states

    // Allow le_Engine to access private members
    friend class Engine;
};

} // namespace LogicElements
