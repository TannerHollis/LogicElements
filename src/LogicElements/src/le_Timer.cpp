#include "le_Timer.hpp"

namespace LogicElements {

/**
 * @brief Constructor that initializes the timer element with specified pickup and dropout times.
 * @param pickup The pickup time in seconds.
 * @param dropout The dropout time in seconds.
 */
Timer::Timer(float pickup, float dropout) : Element(ElementType::Timer)
{
    // Create ports
    pInput = AddInputPort<bool>("input");
    pOutput = AddOutputPort<bool>("output");

    // Convert pickup and dropout times from seconds to nanoseconds and initialize Time objects
    uint32_t pickupNanoseconds = static_cast<uint32_t>(pickup * 1000000000);
    uint32_t dropoutNanoseconds = static_cast<uint32_t>(dropout * 1000000000);

    // Set extrinsic variables
    this->fPickup = Time(Time::GetSubSecondFraction(), pickupNanoseconds, 0, 0, 0, 0, 0);
    this->fDropout = Time(Time::GetSubSecondFraction(), dropoutNanoseconds, 0, 0, 0, 0, 0);

    // Set intrinsic variables
    this->state = TimerState::TIM_IDLE;

    // Initialize last timestamp
    this->lastTimeStamp = Time();
}

/**
 * @brief Updates the timer element.
 * @param timeStamp The current timestamp.
 */
void Timer::Update(const Time& timeStamp)
{
    // Get input value
    if (pInput->IsConnected())
    {
        bool inputValue = pInput->GetValue();

        // Process input based on current state
        switch (this->state)
        {
        case TimerState::TIM_IDLE: // No input is valid
            if (inputValue)
            {
                if (this->fPickup.GetSubSecond() == 0) // If no pickup time, jump to dropout timer
                {
                    this->state = TimerState::TIM_DROPOUT;
                    this->dropoutTime = timeStamp + this->fDropout;
                }
                else
                {
                    this->state = TimerState::TIM_PICKUP;
                    this->pickupTime = timeStamp + this->fPickup;
                }
            }
            break;

        case TimerState::TIM_PICKUP: // Timer is in pickup state
            if (inputValue)
            {
                if (timeStamp.HasElapsed(this->pickupTime))
                {
                    this->state = TimerState::TIM_DROPOUT;
                    this->dropoutTime = (Time&)timeStamp + this->fDropout;
                }
            }
            else
            {
                this->state = TimerState::TIM_IDLE;
            }
            break;

        case TimerState::TIM_DROPOUT: // Timer is in dropout state
            if (!inputValue)
            {
                if (timeStamp.HasElapsed(this->dropoutTime))
                {
                    this->state = TimerState::TIM_IDLE;
                }
            }
            break;

        default:
            this->state = TimerState::TIM_IDLE;
            break;
        }

        // Set the output of the timer
        pOutput->SetValue(this->state == TimerState::TIM_DROPOUT);
    }
}

/**
 * @brief Connects an output port of another element to the input of this timer element.
 * @param sourceElement The element to connect from.
 * @param sourcePortName The name of the output port on the source element.
 */
void Timer::SetInput(Element* sourceElement, const std::string& sourcePortName)
{
    Element::Connect(sourceElement, sourcePortName, this, "input");
}

/**
 * @brief Gets the output value.
 * @return The output value.
 */
bool Timer::GetOutput() const
{
    return pOutput->GetValue();
}

} // namespace LogicElements
