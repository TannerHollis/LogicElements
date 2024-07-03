#include "le_RTrig.hpp"

/**
 * @brief Constructor implementation that initializes the RTrig element.
 */
le_RTrig::le_RTrig() : le_Base<bool>(1, 1)
{
    // Initialize input states to false
    this->_inputStates[0] = false;
    this->_inputStates[1] = false;
}

/**
 * @brief Updates the RTrig element. Detects rising edge transitions.
 * @param timeStep The current timestamp.
 */
void le_RTrig::Update(float timeStep)
{
    // Get the input element
    le_Base<bool>* e = (le_Base<bool>*)this->_inputs[0];
    if (e != nullptr)
    {
        // Get the input value
        bool inputValue = e->GetValue(this->_outputSlots[0]);

        // Shift input states
        this->_inputStates[1] = this->_inputStates[0];
        this->_inputStates[0] = inputValue;

        // Set the next value based on rising edge detection
        this->SetValue(0, this->_inputStates[0] && !this->_inputStates[1]);
    }
}

/**
 * @brief Connects an output slot of another element to the input of this RTrig element.
 * @param e The element to connect from.
 * @param outputSlot The output slot of the element to connect from.
 */
void le_RTrig::SetInput(le_Base<bool>* e, uint16_t outputSlot)
{
    // Use the base class connection function
    le_Base::Connect(e, outputSlot, this, 0);
}
