#pragma once

#include "le_Base.hpp"
#include <vector>
#include <string>

#define MAX_SER_HISTORY 1000
#define DEFAULT_SER_NAME "__SER__"

/**
 * @brief Class representing a Sequential Event Recorder (SER) element.
 *        Inherits from le_Base with boolean type.
 */
class le_SER : protected le_Base<bool>
{
public:
    enum class le_SER_Event_Type : uint8_t
    {
        RISING_EDGE,
        FALLING_EDGE,
        NONE
    };

    typedef struct le_SER_Event
    {
        le_Element* e;
        le_SER_Event_Type type;
        le_Time time;
    } le_SER_Event;

LE_ELEMENT_ACCESS_MOD:
    /**
     * @brief Constructor that initializes the SER element with a specified number of inputs.
     * @param nInputs Number of inputs for the SER element.
     */
    le_SER(uint8_t nInputs);

    /**
     * @brief Updates the SER element. Detects rising and falling edges and logs the events.
     * @param timeStep The current timestamp.
     */
    void Update(const le_Time& timeStamp);

public:
    /**
     * @brief Connects an output slot of another element to an input slot of this SER element.
     * @param e The element to connect from.
     * @param outputSlot The output slot of the element to connect from.
     * @param inputSlot The input slot of this SER element to connect to.
     */
    void SetInput(le_Base<bool>* e, uint8_t outputSlot, uint8_t inputSlot);

    /**
     * @brief Retrieves the event log.
     * @param eventBuffer A buffer to store the retrieved events.
     * @param nEvents The maximum number of events to retrieve.
     * @return The number of events retrieved.
     */
    uint16_t GetEventLog(le_SER_Event* eventBuffer, uint16_t nEvents);

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
    friend class le_Engine;

    // Vector to store the event log
    le_SER_Event eventLog[MAX_SER_HISTORY];
    uint16_t eventLogIndex;
    uint16_t eventLogCount;

    // Vector to store the previous state of inputs
    bool* prevState;
};