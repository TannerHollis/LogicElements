#include "le_NOT.hpp"

/**
 * @brief Constructor that initializes the NOT element.
 */
le_NOT::le_NOT() : le_Base<bool>(1, 1)
{
    // Do nothing...
}

/**
 * @brief Updates the NOT element. Calculates the logical NOT of the input.
 * @param timeStep The current timestamp.
 */
void le_NOT::Update(float timeStep)
{
    // Get input value
    le_Base<bool>* e = (le_Base<bool>*)this->_inputs[0];
    if (e != nullptr)
    {
        bool inputValue = e->GetValue(this->_outputSlots[0]);

        // Set next value
        this->SetValue(0, !inputValue);
    }
}

/**
 * @brief Connects an output slot of another element to the input of this NOT element.
 * @param e The element to connect from.
 * @param outputSlot The output slot of the element to connect from.
 */
void le_NOT::SetInput(le_Base<bool>* e, uint16_t outputSlot)
{
    // Use default connection function
    le_Base::Connect(e, outputSlot, this, 0);
}
