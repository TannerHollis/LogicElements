#include "le_Timer.hpp"

/**
 * @brief Constructor that initializes the timer element with specified pickup and dropout times.
 * @param pickup The pickup time.
 * @param dropout The dropout time.
 */
le_Timer::le_Timer(float pickup, float dropout) : le_Base<bool>(1, 1)
{
    // Set extrinsic variables
    this->fPickup = pickup;
    this->fDropout = dropout;

    // Set intrinsic variables
    this->fTimer = 0.0f;
    this->state = le_Timer_State::TIM_IDLE;
}

/**
 * @brief Updates the timer element.
 * @param timeStep The current timestamp.
 */
void le_Timer::Update(float timeStep)
{
    // Get input component
    le_Base<bool>* e = (le_Base<bool>*)this->_inputs[0];
    if (e != nullptr)
    {
        // Get input value
        bool inputValue = e->GetValue(this->_outputSlots[0]);

        // Process input based on current state
        switch (this->state)
        {
        case le_Timer_State::TIM_IDLE: // No input is valid
            if (inputValue)
            {
                if (this->fPickup == 0.0f) // If no pickup time, jump to dropout timer
                    this->state = le_Timer_State::TIM_DROPOUT;
                else
                    this->state = le_Timer_State::TIM_PICKUP;
            }
            this->fTimer = 0.0f;
            break;

        case le_Timer_State::TIM_PICKUP: // Timer is in pickup state
            if (inputValue)
            {
                this->fTimer += timeStep;
                if (this->fTimer >= this->fPickup)
                {
                    this->state = le_Timer_State::TIM_DROPOUT;
                    this->fTimer = 0.0f;
                }
            }
            else
            {
                this->state = le_Timer_State::TIM_IDLE;
            }
            break;

        case le_Timer_State::TIM_DROPOUT: // Timer is in dropout state
            if (!inputValue)
            {
                this->fTimer += timeStep;
                if (this->fTimer >= this->fDropout)
                {
                    this->state = le_Timer_State::TIM_IDLE;
                }
            }
            else
                this->fTimer = 0.0f;
            break;

        default:
            this->state = le_Timer_State::TIM_IDLE;
            break;
        }

        // Set the output of the timer
        this->SetValue(0, this->state == le_Timer_State::TIM_DROPOUT);
    }
}

/**
 * @brief Connects an output slot of another element to the input of this timer element.
 * @param e The element to connect from.
 * @param outputSlot The output slot of the element to connect from.
 */
void le_Timer::Connect(le_Base<bool>* e, uint16_t outputSlot)
{
    // Use default connection function
    le_Base::Connect(e, outputSlot, this, 0);
}
