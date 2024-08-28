#include "le_SER.hpp"

le_SER::le_SER(uint8_t nInputs) : le_Base<bool>(le_Element_Type::LE_SER, nInputs, 0), eventLogIndex(0), eventLogCount(0)
{
    // Allocate memory for the previous state of inputs
    prevState = new bool[nInputs];
    
    // Initialize all previous states to false
    for (uint8_t i = 0; i < nInputs; ++i)
    {
        prevState[i] = false;
    }

    // Initialize the event log
    le_Time time = le_Time::GetTime();
    for (uint8_t i = 0; i < MAX_SER_HISTORY; ++i)
    {
        eventLog[i].e = nullptr;
        eventLog[i].type = le_SER_Event_Type::NONE;
        eventLog[i].time = time;
    }
}

void le_SER::Update(const le_Time& timeStamp)
{
    // Iterate through all inputs
    for (uint8_t i = 0; i < nInputs; ++i)
    {
        // Get the input element at the current index
        le_Base<bool>* e = this->template GetInput<le_Base<bool>>(i);

        // If the input element is null, skip to the next input
        if(e == nullptr)
            continue;

        // Get the current state of the input element
        bool currentState = e->GetValue(e->GetOutputSlot(i));

        // Check if the current state is different from the previous state
        if(currentState != prevState[i])
        {
            // Log the new event
            eventLog[eventLogIndex].e = e;
            eventLog[eventLogIndex].type = currentState ? le_SER_Event_Type::RISING_EDGE : le_SER_Event_Type::FALLING_EDGE;
            eventLog[eventLogIndex].time = timeStamp;
            // Update the previous state to the current state
            prevState[i] = currentState;

            // Increment the event log index, wrapping around if necessary
            eventLogIndex = (eventLogIndex + 1) % MAX_SER_HISTORY;
            if(eventLogCount < MAX_SER_HISTORY)
            {
                eventLogCount++;
            }
        }
    }
}

void le_SER::SetInput(le_Base<bool>* e, uint8_t outputSlot, uint8_t inputSlot)
{
    le_Base::Connect(e, outputSlot, this, inputSlot);
}

uint16_t le_SER::GetEventLog(le_SER_Event* eventBuffer, uint16_t nEvents)
{
    // Initialize the number of retrieved events to 0
    uint16_t nRetrievedEvents = 0;

    // Start from the current event log index
    uint16_t eventIndex = (eventLogIndex + MAX_SER_HISTORY - eventLogCount) % MAX_SER_HISTORY;

    // Iterate through the requested number of events
    for(uint16_t i = 0; i < nEvents && nRetrievedEvents < eventLogCount; i++)
    {
        // Copy the event from the event log to the event buffer
        eventBuffer[i] = eventLog[eventIndex];

        // Increment the event index, wrapping around if necessary
        eventIndex = (eventIndex + 1) % MAX_SER_HISTORY;

        // Increment the number of retrieved events
        nRetrievedEvents++;
    }

    // Return the number of retrieved events
    return nRetrievedEvents;
}

void le_SER::RemoveOldestEvents(uint16_t nEvents)
{
    if (nEvents >= eventLogCount)
    {
        // If the number of events to remove is greater than or equal to the number of logged events, clear the log
        eventLogIndex = 0;
        eventLogCount = 0;
    }
    else
    {
        // Otherwise, remove the specified number of oldest events
        eventLogCount -= nEvents;
    }
}

bool le_SER::IsEventLogFull() const
{
    return eventLogCount == MAX_SER_HISTORY;
}