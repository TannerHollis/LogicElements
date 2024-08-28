#include "le_Timer.hpp"

/**
 * @brief Constructor that initializes the timer element with specified pickup and dropout times.
 * @param pickup The pickup time in seconds.
 * @param dropout The dropout time in seconds.
 */
le_Timer::le_Timer(float pickup, float dropout) : le_Base<bool>(le_Element_Type::LE_TIMER, 1, 1)
{
    // Convert pickup and dropout times from seconds to microseconds and initialize le_Time objects
    uint32_t pickupMicroseconds = static_cast<uint32_t>(pickup * 1000000);
    uint32_t dropoutMicroseconds = static_cast<uint32_t>(dropout * 1000000);

    // Set extrinsic variables
    this->fPickup = le_Time(1000000, pickupMicroseconds, 0, 0, 0, 0, 0);
    this->fDropout = le_Time(1000000, dropoutMicroseconds, 0, 0, 0, 0, 0);

    // Set intrinsic variables
    this->state = le_Timer_State::TIM_IDLE;

    // Initialize last timestamp
    this->lastTimeStamp = le_Time();
}

/**
 * @brief Updates the timer element.
 * @param timeStamp The current timestamp.
 */
void le_Timer::Update(const le_Time& timeStamp)
{
    // Get input component
    le_Base<bool>* e = this->GetInput<le_Base<bool>>(0);
    if (e != nullptr)
    {
        // Get input value
        bool inputValue = e->GetValue(this->GetOutputSlot(0));

        // Process input based on current state
        switch (this->state)
        {
        case le_Timer_State::TIM_IDLE: // No input is valid
            if (inputValue)
            {
                if (this->fPickup.uSubSecond == 0) // If no pickup time, jump to dropout timer
                {
                    this->state = le_Timer_State::TIM_DROPOUT;
                    this->dropoutTime = (le_Time&)timeStamp + this->fDropout;
                }
                else
                {
                    this->state = le_Timer_State::TIM_PICKUP;
                    this->pickupTime = (le_Time&)timeStamp + this->fPickup;
                }
            }
            break;

        case le_Timer_State::TIM_PICKUP: // Timer is in pickup state
            if (inputValue)
            {
                if (timeStamp.HasElapsed(this->pickupTime))
                {
                    this->state = le_Timer_State::TIM_DROPOUT;
                    this->dropoutTime = (le_Time&)timeStamp + this->fDropout;
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
                if (timeStamp.HasElapsed(this->dropoutTime))
                {
                    this->state = le_Timer_State::TIM_IDLE;
                }
            }
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
void le_Timer::SetInput(le_Base<bool>* e, uint8_t outputSlot)
{
    // Use default connection function
    le_Base::Connect(e, outputSlot, this, 0);
}
