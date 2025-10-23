#pragma once

#include "Core/le_Element.hpp"


namespace LogicElements {
/**
 * @brief Class representing a counter element.
 *        Counts rising edges and outputs true when count reaches target.
 */
class Counter : public Element
{
LE_ELEMENT_ACCESS_MOD:
    /**
     * @brief Constructor that initializes the counter element with a specified final count.
     * @param countFinal The final count value for the counter.
     */
    Counter(uint16_t countFinal);

    /**
     * @brief Updates the counter element.
     * @param timeStamp The current timestamp.
     */
    void Update(const Time& timeStamp) override;

public:
    /**
     * @brief Sets the input for counting up.
     * @param sourceElement The element to connect from.
     * @param sourcePortName The name of the output port on the source element.
     */
    void SetInput_CountUp(Element* sourceElement, const std::string& sourcePortName);

    /**
     * @brief Sets the input for resetting the counter.
     * @param sourceElement The element to connect from.
     * @param sourcePortName The name of the output port on the source element.
     */
    void SetInput_Reset(Element* sourceElement, const std::string& sourcePortName);

    /**
     * @brief Gets the output value.
     * @return The output value.
     */
    bool GetOutput() const;

private:
    InputPort<bool>* pCountUp;  ///< The count-up input port.
    InputPort<bool>* pReset;    ///< The reset input port.
    OutputPort<bool>* pOutput;  ///< The output port.

    uint16_t uCountFinal;  ///< The final count value.
    uint16_t uCount;       ///< The current count value.

    bool _inputStates[2];  ///< Stores the previous and current input states

    // Allow le_Engine to access private members
    friend class Engine;
};

} // namespace LogicElements
