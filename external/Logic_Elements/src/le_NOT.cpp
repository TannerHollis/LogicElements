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
    UNUSED(timeStep);

    // Get input value
    le_Base<bool>* e = this->template GetInput<le_Base<bool>>(0);
    if (e != nullptr)
    {
        bool inputValue = e->GetValue(this->GetOutputSlot(0));

        // Set next value
        this->SetValue(0, !inputValue);
    }
}

/**
 * @brief Connects an output slot of another element to the input of this NOT element.
 * @param e The element to connect from.
 * @param outputSlot The output slot of the element to connect from.
 */
void le_NOT::SetInput(le_Base<bool>* e, uint8_t outputSlot)
{
    // Use default connection function
    le_Base::Connect(e, outputSlot, this, 0);
}
