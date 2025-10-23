#pragma once

#include "le_Element.hpp"
#include <vector>
#include <string>

#define MAX_SER_HISTORY 1000
#define DEFAULT_SER_NAME "__SER__"


namespace LogicElements {
/**
 * @brief Class representing a Sequential Event Recorder (SER) element.
 *        Records rising and falling edge events from multiple inputs.
 */
class SER : public Element
{
public:
    enum class SEREventType : uint8_t
    {
        RISING_EDGE,
        FALLING_EDGE,
        NONE
    };

    typedef struct SEREvent
    {
        Element* e;
        SEREventType type;
        Time time;
    } SEREvent;

LE_ELEMENT_ACCESS_MOD:
    /**
     * @brief Constructor that initializes the SER element with a specified number of inputs.
     * @param nInputs Number of inputs for the SER element.
     */
    SER(uint8_t nInputs);

    /**
     * @brief Destructor to clean up allocated memory.
     */
    ~SER();

    /**
     * @brief Updates the SER element. Detects rising and falling edges and logs the events.
     * @param timeStamp The current timestamp.
     */
    void Update(const Time& timeStamp) override;

public:
    /**
     * @brief Connects an output port of another element to a named input port of this SER element.
     * @param sourceElement The element to connect from.
     * @param sourcePortName The name of the output port on the source element.
     * @param inputPortName The name of the input port on this SER element.
     */
    void SetInput(Element* sourceElement, const std::string& sourcePortName, const std::string& inputPortName);

    /**
     * @brief Retrieves the event log.
     * @param eventBuffer A buffer to store the retrieved events.
     * @param nEvents The maximum number of events to retrieve.
     * @return The number of events retrieved.
     */
    uint16_t GetEventLog(SEREvent* eventBuffer, uint16_t nEvents);

    /**
     * @brief Removes the specified number of oldest events from the event log.
     * @param nEvents The number of events to remove.
     */
    void RemoveOldestEvents(uint16_t nEvents);

    /**
     * @brief Checks if the event log is full.
     * @return True if the event log is full, false otherwise.
     */
    bool IsEventLogFull() const;

private:
    // Allow le_Engine to access private members
    friend class Engine;

    // Event log storage
    SEREvent eventLog[MAX_SER_HISTORY];
    uint16_t eventLogIndex;
    uint16_t eventLogCount;

    // Vector to store the previous state of inputs
    bool* prevState;
    uint8_t nInputCount;  ///< Number of inputs for tracking state
};

} // namespace LogicElements
