#pragma once

#include "le_Base.hpp"

/**
 * @brief Class representing a logical OR element.
 *        Inherits from le_Base with boolean type.
 */
class le_OR : protected le_Base<bool>
{
LE_ELEMENT_ACCESS_MOD:
    /**
     * @brief Constructor that initializes the OR element with a specified number of inputs.
     * @param nInputs Number of inputs for the OR element.
     */
    le_OR(uint8_t nInputs);

    /**
     * @brief Updates the OR element. Calculates the logical OR of all inputs.
     * @param timeStamp The current timestamp.
     */
    void Update(const le_Time& timeStamp);

public:
    /**
     * @brief Connects an output slot of another element to an input slot of this OR element.
     * @param e The element to connect from.
     * @param outputSlot The output slot of the element to connect from.
     * @param inputSlot The input slot of this OR element to connect to.
     */
    void SetInput(le_Base<bool>* e, uint8_t outputSlot, uint8_t inputSlot);

private:
    // Allow le_Engine to access private members
    friend class le_Engine;
};
