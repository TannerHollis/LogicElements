#pragma once

#include "le_Base.hpp"
#include "le_Time.hpp"

/**
 * @brief Class representing a logical AND element.
 *        Inherits from le_Base with boolean type.
 */
class le_AND : protected le_Base<bool>
{
LE_ELEMENT_ACCESS_MOD:
    /**
     * @brief Constructor that initializes the AND element with a specified number of inputs.
     * @param nInputs Number of inputs for the AND element.
     */
    le_AND(uint8_t nInputs);

    /**
     * @brief Updates the AND element. Calculates the logical AND of all inputs.
     * @param timeStamp The current timestamp.
     */
    void Update(const le_Time& timeStamp);

public:
    /**
     * @brief Connects an output slot of another element to an input slot of this AND element.
     * @param e The element to connect from.
     * @param outputSlot The output slot of the element to connect from.
     * @param inputSlot The input slot of this AND element to connect to.
     */
    void SetInput(le_Base<bool>* e, uint8_t outputSlot, uint8_t inputSlot);

private:
    // Allow le_Engine to access private members
    friend class le_Engine;
};