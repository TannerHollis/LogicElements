#pragma once

#include "le_Base.hpp"

/**
 * @brief Class representing a rising edge trigger (RTrig) element.
 *        Inherits from le_Base with boolean type.
 */
class le_RTrig : protected le_Base<bool>
{
LE_ELEMENT_ACCESS_MOD:
    /**
     * @brief Constructor that initializes the RTrig element.
     */
    le_RTrig();

    /**
     * @brief Updates the RTrig element. Detects rising edge transitions.
     * @param timeStamp The current timestamp.
     */
    void Update(const le_Time& timeStamp);

public:
    /**
     * @brief Connects an output slot of another element to the input of this RTrig element.
     * @param e The element to connect from.
     * @param outputSlot The output slot of the element to connect from.
     */
    void SetInput(le_Base<bool>* e, uint8_t outputSlot);

private:
    bool _inputStates[2]; ///< Stores the previous and current input states

    // Allow le_Engine to access private members
    friend class le_Engine;
};
