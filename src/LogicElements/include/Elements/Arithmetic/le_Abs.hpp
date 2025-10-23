#pragma once

#include "Core/le_Element.hpp"

namespace LogicElements {

#ifdef LE_ELEMENTS_ANALOG

/**
 * @brief Class representing an absolute value element.
 *        Computes the absolute value of a float input.
 */
class Abs : public Element
{
LE_ELEMENT_ACCESS_MOD:
    /**
     * @brief Constructor that initializes the Abs element with one input.
     */
    Abs();

    /**
     * @brief Updates the Abs element. Calculates abs(input).
     * @param timeStamp The current timestamp.
     */
    void Update(const Time& timeStamp) override;

public:
    /**
     * @brief Connects an output port of another element to the input port of this Abs element.
     * @param sourceElement The element to connect from.
     * @param sourcePortName The name of the output port on the source element.
     */
    void SetInput(Element* sourceElement, const std::string& sourcePortName);

    /**
     * @brief Gets the output value.
     * @return The output value (|input|).
     */
    float GetOutput() const;

private:
    InputPort<float>* pInput;   ///< The input port.
    OutputPort<float>* pOutput; ///< The output port.

    // Allow Engine to access private members
    friend class Engine;
};

#endif // LE_ELEMENTS_ANALOG

} // namespace LogicElements

