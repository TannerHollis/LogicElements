#pragma once

#include "le_Base.hpp"

/**
 * @brief Class representing a counter element.
 *        Inherits from le_Base with boolean type.
 */
class le_Counter : protected le_Base<bool>
{
LE_ELEMENT_ACCESS_MOD:
    /**
     * @brief Constructor that initializes the counter element with a specified final count.
     * @param countFinal The final count value for the counter.
     */
    le_Counter(uint16_t countFinal);

    /**
     * @brief Updates the counter element.
     * @param timeStamp The current timestamp.
     */
    void Update(const le_Time& timeStamp);

public:
    /**
     * @brief Sets the input for counting up.
     * @param e The element to connect from.
     * @param outputSlot The output slot of the element to connect from.
     */
    void SetInput_CountUp(le_Base<bool>* e, uint8_t outputSlot);

    /**
     * @brief Sets the input for resetting the counter.
     * @param e The element to connect from.
     * @param outputSlot The output slot of the element to connect from.
     */
    void SetInput_Reset(le_Base<bool>* e, uint8_t outputSlot);

private:
    uint16_t uCountFinal;  ///< The final count value.
    uint16_t uCount;       ///< The current count value.

    bool _inputStates[2];  ///< Stores the previous and current input states

    // Allow le_Engine to access private members
    friend class le_Engine;
};
