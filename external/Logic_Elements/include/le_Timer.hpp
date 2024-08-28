#pragma once

#include "le_Base.hpp"
#include "le_Time.hpp"

/**
 * @brief Class representing a timer element.
 *        Inherits from le_Base with boolean type.
 */
class le_Timer : protected le_Base<bool>
{
LE_ELEMENT_ACCESS_MOD:
    /**
     * @brief Enum representing different timer states.
     */
    typedef enum le_Timer_State
    {
        TIM_IDLE = 0,
        TIM_PICKUP = 1,
        TIM_DROPOUT = 2
    } le_Timer_State;

    /**
     * @brief Constructor that initializes the timer element with specified pickup and dropout times.
     * @param pickup The pickup time in seconds.
     * @param dropout The dropout time in seconds.
     */
    le_Timer(float pickup, float dropout);

    /**
     * @brief Updates the timer element.
     * @param timeStamp The current timestamp.
     */
    void Update(const le_Time& timeStamp);

public:
    /**
     * @brief Connects an output slot of another element to the input of this timer element.
     * @param e The element to connect from.
     * @param outputSlot The output slot of the element to connect from.
     */
    void SetInput(le_Base<bool>* e, uint8_t outputSlot);

private:
    le_Time fPickup;   ///< The pickup time.
    le_Time fDropout;  ///< The dropout time.

    le_Time pickupTime;  ///< The future time when pickup condition is met.
    le_Time dropoutTime; ///< The future time when dropout condition is met.
    le_Timer_State state; ///< The current state of the timer.

    // Last timestamp
    le_Time lastTimeStamp;

    // Allow le_Engine to access private members
    friend class le_Engine;
};
