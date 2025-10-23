#pragma once

#include "le_Element.hpp"


namespace LogicElements {
/**
 * @brief Class representing a timer element.
 *        Provides pickup and dropout timing functionality.
 */
class Timer : public Element
{
LE_ELEMENT_ACCESS_MOD:
    /**
     * @brief Enum representing different timer states.
     */
    typedef enum TimerState
    {
        TIM_IDLE = 0,
        TIM_PICKUP = 1,
        TIM_DROPOUT = 2
    } TimerState;

    /**
     * @brief Constructor that initializes the timer element with specified pickup and dropout times.
     * @param pickup The pickup time in seconds.
     * @param dropout The dropout time in seconds.
     */
    Timer(float pickup, float dropout);

    /**
     * @brief Updates the timer element.
     * @param timeStamp The current timestamp.
     */
    void Update(const Time& timeStamp) override;

public:
    /**
     * @brief Connects an output port of another element to the input of this timer element.
     * @param sourceElement The element to connect from.
     * @param sourcePortName The name of the output port on the source element.
     */
    void SetInput(Element* sourceElement, const std::string& sourcePortName);

    /**
     * @brief Gets the output value.
     * @return The output value.
     */
    bool GetOutput() const;

private:
    InputPort<bool>* pInput;   ///< The input port.
    OutputPort<bool>* pOutput; ///< The output port.

    Time fPickup;   ///< The pickup time.
    Time fDropout;  ///< The dropout time.

    Time pickupTime;  ///< The future time when pickup condition is met.
    Time dropoutTime; ///< The future time when dropout condition is met.
    TimerState state; ///< The current state of the timer.

    // Last timestamp
    Time lastTimeStamp;

    // Allow le_Engine to access private members
    friend class Engine;
};

} // namespace LogicElements
