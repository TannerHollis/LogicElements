#include "le_Counter.hpp"

/**
 * @brief Constructor implementation that initializes the counter element with a specified final count.
 * @param countFinal The final count value for the counter.
 */
le_Counter::le_Counter(uint16_t countFinal) : le_Base<bool>(2, 1)
{
    // Set extrinsic variables
    this->uCountFinal = countFinal;

    // Set intrinsic variables
    this->uCount = 0;
    this->_inputStates[0] = false;
    this->_inputStates[1] = false;
}

/**
 * @brief Updates the counter element.
 * @param timeStep The current timestamp.
 */
void le_Counter::Update(float timeStep)
{
    UNUSED(timeStep);

    // Get input components for count-up and reset
    le_Base<bool>* cu = this->GetInput<le_Base<bool>>(0);
    le_Base<bool>* reset = this->GetInput<le_Base<bool>>(1);

    // If inputs are invalid, skip update
    if (cu == nullptr || reset == nullptr)
        return;

    // Update input states for rising trigger detection
    this->_inputStates[1] = this->_inputStates[0];
    this->_inputStates[0] = cu->GetValue(this->GetOutputSlot(0));

    // Detect rising edge trigger
    bool rtrig = this->_inputStates[0] && !this->_inputStates[1];

    // Reset dominant logic
    if (reset->GetValue(this->GetOutputSlot(1)))
        this->uCount = 0;
    else if (rtrig)
        this->uCount += 1;

    // Set the next state based on the count value
    this->SetValue(0, this->uCount >= this->uCountFinal);
}

/**
 * @brief Sets the input for counting up.
 * @param e The element to connect from.
 * @param outputSlot The output slot of the element to connect from.
 */
void le_Counter::SetInput_CountUp(le_Base<bool>* e, uint8_t outputSlot)
{
    // Use the base class connection function
    le_Base::Connect(e, outputSlot, this, 0);
}

/**
 * @brief Sets the input for resetting the counter.
 * @param e The element to connect from.
 * @param outputSlot The output slot of the element to connect from.
 */
void le_Counter::SetInput_Reset(le_Base<bool>* e, uint8_t outputSlot)
{
    // Use the base class connection function
    le_Base::Connect(e, outputSlot, this, 1);
}
