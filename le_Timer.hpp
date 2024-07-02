#pragma once

#include "le_Base.hpp"

/**
 * @brief Class representing a timer element.
 *        Inherits from le_Base with boolean type.
 */
class le_Timer : protected le_Base<bool>
{
private:
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
     * @param pickup The pickup time.
     * @param dropout The dropout time.
     */
    le_Timer(float pickup, float dropout);

    /**
     * @brief Updates the timer element.
     * @param timeStep The current timestamp.
     */
    void Update(float timeStep);

public:
    /**
     * @brief Connects an output slot of another element to the input of this timer element.
     * @param e The element to connect from.
     * @param outputSlot The output slot of the element to connect from.
     */
    void Connect(le_Base<bool>* e, uint16_t outputSlot);

private:
    float fPickup;   ///< The pickup time.
    float fDropout;  ///< The dropout time.

    float fTimer;    ///< The timer value.
    le_Timer_State state; ///< The current state of the timer.

    // Allow le_Engine to access private members
    friend class le_Engine;
};
