#include "le_AND.hpp"

/**
 * @brief Constructor implementation that initializes the AND element with a specified number of inputs.
 * @param nInputs Number of inputs for the AND element.
 */
le_AND::le_AND(uint8_t nInputs) : le_Base<bool>(nInputs, 1)
{
    // Do nothing...
}

/**
 * @brief Updates the AND element. Calculates the logical AND of all inputs.
 * @param timeStep The current timestamp.
 */
void le_AND::Update(float timeStep)
{
    // Set default to true
    bool nextValue = true;

    // Iterate through all input values and apply logical AND
    for (uint8_t i = 0; i < this->nInputs; i++)
    {
        le_Base<bool>* e = (le_Base<bool>*)(this->_inputs[i]);
        if (e != nullptr)
        {
            bool inputValue = e->GetValue(this->_outputSlots[i]);
            nextValue &= inputValue;
        }
    }

    // Set the next value
    this->SetValue(0, nextValue);
}

/**
 * @brief Connects an output slot of another element to an input slot of this AND element.
 * @param e The element to connect from.
 * @param outputSlot The output slot of the element to connect from.
 * @param inputSlot The input slot of this AND element to connect to.
 */
void le_AND::SetInput(le_Base<bool>* e, uint8_t outputSlot, uint8_t inputSlot)
{
    // Use the base class connection function
    le_Base::Connect(e, outputSlot, this, inputSlot);
}
