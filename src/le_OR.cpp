#include "le_OR.hpp"

/**
 * @brief Constructor implementation that initializes the OR element with a specified number of inputs.
 * @param nInputs Number of inputs for the OR element.
 */
le_OR::le_OR(uint16_t nInputs) : le_Base(nInputs, 1)
{
    // Do nothing...
}

/**
 * @brief Updates the OR element. Calculates the logical OR of all inputs.
 * @param timeStep The current timestamp.
 */
void le_OR::Update(float timeStep)
{
    // Set default to false
    bool nextValue = false;

    // Iterate through all input values and apply logical OR
    for (uint16_t i = 0; i < this->nInputs; i++)
    {
        le_Base<bool>* e = (le_Base<bool>*)this->_inputs[i];
        if (e != nullptr)
        {
            bool inputValue = e->GetValue(this->_outputSlots[i]);
            nextValue |= inputValue;
        }
    }

    // Set the next value
    this->SetValue(0, nextValue);
}

/**
 * @brief Connects an output slot of another element to an input slot of this OR element.
 * @param e The element to connect from.
 * @param outputSlot The output slot of the element to connect from.
 * @param inputSlot The input slot of this OR element to connect to.
 */
void le_OR::Connect(le_Base* e, uint16_t outputSlot, uint16_t inputSlot)
{
    // Use the base class connection function
    le_Base::Connect(e, outputSlot, this, inputSlot);
}