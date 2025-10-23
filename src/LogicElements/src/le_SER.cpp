#include "le_SER.hpp"

namespace LogicElements {

SER::SER(uint8_t nInputs) : Element(ElementType::SER), eventLogIndex(0), eventLogCount(0), nInputCount(nInputs)
{
    // Create named input ports (no outputs - this is a recorder element)
    for (uint8_t i = 0; i < nInputs; i++)
    {
        AddInputPort<bool>("input_" + std::to_string(i));
    }

    // Allocate memory for the previous state of inputs
    prevState = new bool[nInputs];
    
    // Initialize all previous states to false
    for (uint8_t i = 0; i < nInputs; ++i)
    {
        prevState[i] = false;
    }

    // Initialize the event log
    Time time = Time::GetTime();
    for (uint16_t i = 0; i < MAX_SER_HISTORY; ++i)
    {
        eventLog[i].e = nullptr;
        eventLog[i].type = SEREventType::NONE;
        eventLog[i].time = time;
    }
}

SER::~SER()
{
    delete[] prevState;
}

void SER::Update(const Time& timeStamp)
{
    // Iterate through all input ports
    for (size_t i = 0; i < _inputPorts.size(); ++i)
    {
        InputPort<bool>* inputPort = dynamic_cast<InputPort<bool>*>(_inputPorts[i]);
        
        // If the input port is not connected, skip to the next input
        if (inputPort == nullptr || !inputPort->IsConnected())
            continue;

        // Get the current state of the input
        bool currentState = inputPort->GetValue();

        // Check if the current state is different from the previous state
        if (currentState != prevState[i])
        {
            // Log the new event
            eventLog[eventLogIndex].e = inputPort->GetSource()->GetOwner();
            eventLog[eventLogIndex].type = currentState ? SEREventType::RISING_EDGE : SEREventType::FALLING_EDGE;
            eventLog[eventLogIndex].time = timeStamp;
            
            // Update the previous state to the current state
            prevState[i] = currentState;

            // Increment the event log index, wrapping around if necessary
            eventLogIndex = (eventLogIndex + 1) % MAX_SER_HISTORY;
            if (eventLogCount < MAX_SER_HISTORY)
            {
                eventLogCount++;
            }
        }
    }
}

void SER::SetInput(Element* sourceElement, const std::string& sourcePortName, const std::string& inputPortName)
{
    Element::Connect(sourceElement, sourcePortName, this, inputPortName);
}

uint16_t SER::GetEventLog(SEREvent* eventBuffer, uint16_t nEvents)
{
    // Initialize the number of retrieved events to 0
    uint16_t nRetrievedEvents = 0;

    // Start from the oldest event in the circular buffer
    uint16_t eventIndex = (eventLogIndex + MAX_SER_HISTORY - eventLogCount) % MAX_SER_HISTORY;

    // Iterate through the requested number of events
    for (uint16_t i = 0; i < nEvents && nRetrievedEvents < eventLogCount; i++)
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

void SER::RemoveOldestEvents(uint16_t nEvents)
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

bool SER::IsEventLogFull() const
{
    return eventLogCount == MAX_SER_HISTORY;
}

} // namespace LogicElements
