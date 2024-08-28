#pragma once

#include "le_Base.hpp"

/**
 * @brief Class representing a falling edge trigger (FTrig) element.
 *        Inherits from le_Base with boolean type.
 */
class le_FTrig : protected le_Base<bool>
{
LE_ELEMENT_ACCESS_MOD:
    /**
     * @brief Constructor that initializes the FTrig element.
     */
    le_FTrig();

    /**
     * @brief Updates the FTrig element. Detects falling edge transitions.
     * @param timeStamp The current timestamp.
     */
    void Update(const le_Time& timeStamp);

public:

    /**
     * @brief Connects an output slot of another element to the input of this FTrig element.
     * @param e The element to connect from.
     * @param outputSlot The output slot of the element to connect from.
     */
    void SetInput(le_Base<bool>* e, uint8_t outputSlot);

private:
    bool _inputStates[2]; ///< Stores the previous and current input states

    // Allow le_Engine to access private members
    friend class le_Engine;
};
