#include "le_FTrig.hpp"

/**
 * @brief Constructor implementation that initializes the FTrig element.
 */
le_FTrig::le_FTrig() : le_Base<bool>(1, 1)
{
    // Initialize input states to false
    this->_inputStates[0] = false;
    this->_inputStates[1] = false;
}

/**
 * @brief Updates the FTrig element. Detects falling edge transitions.
 * @param timeStep The current timestamp.
 */
void le_FTrig::Update(float timeStep)
{
    UNUSED(timeStep);

    // Get the input element
    le_Base<bool>* e = this->GetInput<le_Base<bool>>(0);
    if (e != nullptr)
    {
        // Get the input value
        bool inputValue = e->GetValue(this->GetOutputSlot(0));

        // Shift input states
        this->_inputStates[1] = this->_inputStates[0];
        this->_inputStates[0] = inputValue;

        // Set the next value based on falling edge detection
        this->SetValue(0, !this->_inputStates[0] && this->_inputStates[1]);
    }
}

/**
 * @brief Connects an output slot of another element to the input of this FTrig element.
 * @param e The element to connect from.
 * @param outputSlot The output slot of the element to connect from.
 */
void le_FTrig::SetInput(le_Base* e, uint8_t outputSlot)
{
    // Use the base class connection function
    le_Base::Connect(e, outputSlot, this, 0);
}
