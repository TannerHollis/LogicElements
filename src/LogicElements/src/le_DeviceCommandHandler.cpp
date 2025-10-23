#include "le_DeviceCommandHandler.hpp"

#include <algorithm>
#include <cstdio>

namespace LogicElements {

#define ECHO_COMMAND_BUFFER_SIZE 64
#define ID_COMMAND_BUFFER_SIZE 256
#define STATUS_COMMAND_BUFFER_SIZE 8192
#define TARGET_COMMAND_BUFFER_SIZE 256
#define PULSE_COMMAND_BUFFER_SIZE 256
#define SER_COMMAND_BUFFER_SIZE 256

DeviceCommandHandler::DeviceCommandHandler(AbstractConnectionServer* connectionServer, Board* board, bool multipleConnectionCapability)
    : connectionServer(connectionServer), board(board), running(false), multipleConnectionCapability(multipleConnectionCapability) {}

DeviceCommandHandler::~DeviceCommandHandler()
{
    stop();
}

void DeviceCommandHandler::start()
{
    running = true;
    bool success = connectionServer->open();

    if (success)
    {
#ifdef DEBUG_SERVER_CONNECTION
        std::printf("%s was successfully opened.\n", connectionServer->getName().c_str());
#endif
        // Start a thread to handle client connections
        server_thread = std::thread(&DeviceCommandHandler::acceptClients, this);
    }
}

void DeviceCommandHandler::stop()
{
#ifdef DEBUG_SERVER_CONNECTION
    std::printf("Stopping %s.\n", connectionServer->getName().c_str());
#endif

    running = false;

    // Wait for the server thread to finish
    if (server_thread.joinable())
    {
        server_thread.join();
    }

    // Wait for all client threads to finish
    for (std::thread& client_thread : client_threads)
    {
        if (client_thread.joinable())
        {
            client_thread.join();
        }
    }

    connectionServer->close();
}

void DeviceCommandHandler::acceptClients()
{
    bool acceptMultipleClients = true;
    while (acceptMultipleClients)
    {
        // Accept new clients
        std::unique_ptr<AbstractConnectionHandler> connectionHandler = connectionServer->acceptNewClient();

        if(connectionHandler == nullptr)
        {
#ifdef DEBUG_SERVER_CONNECTION
            std::printf("Failed to accept new client on %s\n", connectionServer->getName().c_str());
#endif
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            continue;
        }

        acceptMultipleClients = multipleConnectionCapability;

        // Create a new thread for the client
        client_threads.emplace_back(&DeviceCommandHandler::handleClient, this, connectionHandler.release());
    }
}

void DeviceCommandHandler::handleClient(AbstractConnectionHandler* connectionHandler)
{
    std::thread responseThread;

    while (running)
    {       
        // Read a line from the connection
        std::string line = connectionHandler->ReadLine();
        
        // If the line is empty, continue to the next iteration
        if (line.empty())
            continue;

        // Parse the request message from the line
        std::unique_ptr<comms::msg_req> request = comms::parse_msg_req_command(line);

        // If the request is null, continue to the next iteration
        if (request == nullptr)
            continue;

        // If the request type is unknown, send the error message and continue
        if (request->GetType() == comms::msg_type::UNKNOWN)
        {
            comms::msg_req_unknown* unknown = static_cast<comms::msg_req_unknown*>(request.get());
            connectionHandler->WriteLine(unknown->GetFullError());
            connectionHandler->WriteLine("\r\n>> ");
            continue;
        }

        // If the request category is REQUEST, handle the command
        if (request->GetCategory() == comms::msg_category::REQUEST)
        {
            // Join the previous response thread if it is joinable
            if (responseThread.joinable())
            {
                responseThread.join();
            }
            // Create a new thread to handle the command
            responseThread = std::thread(&DeviceCommandHandler::handleCommand, this, connectionHandler, request.release());
        }
    }
}

void DeviceCommandHandler::handleCommand(AbstractConnectionHandler* connection, comms::msg_req* request)
{
    // Set the command in process state to true
    connection->SetCommandInProcess(true);

    // Handle the request based on its type
    switch (request->GetType())
    {
        // Echo command
        case comms::msg_type::MSG_ECHO:
        {
            executeEchoCommand(connection, (comms::msg_req_echo*)request);
            break;
        }

        // ID command
        case comms::msg_type::MSG_ID:
        {
            executeIDCommand(connection, request);
            break;
        }

        // Status command
        case comms::msg_type::MSG_STATUS:
        {
            executeStatusCommand(connection, request);
            break;
        }

        // Target command
        case comms::msg_type::MSG_TARGET:
        {
            executeTargetCommand(connection, (comms::msg_req_target*)request);
            break;
        }

        // Pulse command
        case comms::msg_type::MSG_PULSE:
        {
            executePulseCommand(connection, (comms::msg_req_pulse*)request);
            break;
        }

        // SER command
        case comms::msg_type::MSG_SER:
        {
            executeSERCommand(connection, (comms::msg_req_ser*)request);
            break;
        }

        // Default case for unknown message types
        default:
#ifdef DEBUG_SERVER_CONNECTION
            std::fprintf(stderr, "Unknown message type.\n");
#endif
            break;
    }

    // Write next line character
    connection->WriteLine("\r\n>> ");

    // Set the command in process state to false
    connection->SetCommandInProcess(false);
}

void DeviceCommandHandler::executeEchoCommand(AbstractConnectionHandler* connection, comms::msg_req_echo* request)
{
    // Echo back only first argument
    static char buffer[ECHO_COMMAND_BUFFER_SIZE];
    uint16_t len = snprintf(buffer, ECHO_COMMAND_BUFFER_SIZE, "%s\r\n", request->GetEcho().c_str());

    std::vector<comms::msg_resp> responses = comms::msg_resp::partialize_msg(request->GetType(), buffer, len, false);

    sendResponses(connection, responses, 10);
}

void DeviceCommandHandler::executeIDCommand(AbstractConnectionHandler* connection, comms::msg_req* request)
{
    // Get ID string
    static char buffer[ID_COMMAND_BUFFER_SIZE];
    uint16_t len = board->GetInfo(buffer, ID_COMMAND_BUFFER_SIZE);

    std::vector<comms::msg_resp> responses = comms::msg_resp::partialize_msg(request->GetType(), buffer, len, false);

    sendResponses(connection, responses, 10);
}

void DeviceCommandHandler::executeStatusCommand(AbstractConnectionHandler* connection, comms::msg_req* request)
{
    static char buffer[STATUS_COMMAND_BUFFER_SIZE];

    // Get engine
    Engine* engine = board->GetEngine();

    std::vector<comms::msg_resp> responses;

    // Check if engine is attached
    if (engine == nullptr)
    {
        uint16_t len = snprintf(buffer, STATUS_COMMAND_BUFFER_SIZE, "Could not get engine status, no engine is currently attached.\r\n");
        responses = comms::msg_resp::partialize_msg(request->GetType(), buffer, len, true);
        sendResponses(connection, responses, 10);
        return;
    }

    // Get engine status
    uint16_t len = engine->GetInfo(buffer, STATUS_COMMAND_BUFFER_SIZE);

    // Send response
    responses = comms::msg_resp::partialize_msg(request->GetType(), buffer, len, false);

    sendResponses(connection, responses, 100);
}

void DeviceCommandHandler::executeTargetCommand(AbstractConnectionHandler* connection, comms::msg_req_target* request)
{
    static char buffer[TARGET_COMMAND_BUFFER_SIZE];

    std::vector<comms::msg_resp> responses;

    // Check if engine is attached
    if (!checkEngineExists())
    {
        uint16_t len = snprintf(buffer, TARGET_COMMAND_BUFFER_SIZE, "Engine not attached\r\n");
        responses =  comms::msg_resp::partialize_msg(request->GetType(), buffer, len, true);
        sendResponses(connection, responses, 10);
        return;
    }
    
    // Check if element exists
    if (!checkElementExists(request->GetElementName()))
    {
        uint16_t len = snprintf(buffer, TARGET_COMMAND_BUFFER_SIZE, "Could not find element: %s\r\n", request->GetElementName().c_str());
        responses =  comms::msg_resp::partialize_msg(request->GetType(), buffer, len, true);
        sendResponses(connection, responses, 10);
        return;
    }

    // Get engine
    Engine* engine = board->GetEngine();

    // Get element
    Element* element = engine->GetElement(request->GetElementName());

    // Get output slot (port index)
    uint8_t outputSlot = (uint8_t)request->GetOutputSlot();

    // Check if outputSlot is within the number of output ports on the element
    if (outputSlot >= element->GetOutputPortCount())
    {
        uint16_t len = snprintf(buffer, TARGET_COMMAND_BUFFER_SIZE, "Output port %u is out of range for element %s\r\n", outputSlot, request->GetElementName().c_str());
        responses =  comms::msg_resp::partialize_msg(request->GetType(), buffer, len, true);
        sendResponses(connection, responses, 10);
        return;
    }

    // Get the output port by index
    Port* outputPort = element->GetOutputPorts()[outputSlot];
    if (outputPort == nullptr)
    {
        uint16_t len = snprintf(buffer, TARGET_COMMAND_BUFFER_SIZE, "Output port %u not found on element %s\r\n", outputSlot, request->GetElementName().c_str());
        responses =  comms::msg_resp::partialize_msg(request->GetType(), buffer, len, true);
        sendResponses(connection, responses, 10);
        return;
    }

    // Send target message
    for(uint16_t i = 0; i < request->GetRepetition(); i++)
    {
        if(connection->getEscapeKeyRequested())
        {
            connection->acknowledgeEscapeKeyRequest();
            break;
        }

        // Access value based on port type
        if (outputPort->GetType() == PortType::DIGITAL)
        {
            OutputPort<bool>* boolPort = dynamic_cast<OutputPort<bool>*>(outputPort);
            if (boolPort)
            {
                uint16_t len = snprintf(buffer, TARGET_COMMAND_BUFFER_SIZE, "%s\t= %u\r\n", request->GetElementName().c_str(), boolPort->GetValue());
                responses =  comms::msg_resp::partialize_msg(request->GetType(), buffer, len, true);
                sendResponses(connection, responses, 10);
            }
        }
        else if (outputPort->GetType() == PortType::ANALOG)
        {
            OutputPort<float>* floatPort = dynamic_cast<OutputPort<float>*>(outputPort);
            if (floatPort)
            {
                uint16_t len = snprintf(buffer, TARGET_COMMAND_BUFFER_SIZE, "%s\t= %.4f\r\n", request->GetElementName().c_str(), floatPort->GetValue());
                responses =  comms::msg_resp::partialize_msg(request->GetType(), buffer, len, true);
                sendResponses(connection, responses, 10);
            }
        }
        else if (outputPort->GetType() == PortType::COMPLEX)
        {
            OutputPort<std::complex<float>>* complexPort = dynamic_cast<OutputPort<std::complex<float>>*>(outputPort);
            if (complexPort)
            {
                std::complex<float> val = complexPort->GetValue();
                uint16_t len = snprintf(buffer, TARGET_COMMAND_BUFFER_SIZE, "%s\t= %.4f + j%.4f\r\n", request->GetElementName().c_str(), val.real(), val.imag());
                responses =  comms::msg_resp::partialize_msg(request->GetType(), buffer, len, true);
                sendResponses(connection, responses, 10);
            }
        }

        if(i != request->GetRepetition() - 1)
            std::this_thread::sleep_for(std::chrono::milliseconds(request->GetDelayMS()));
    }
}

void DeviceCommandHandler::executePulseCommand(AbstractConnectionHandler* connection, comms::msg_req_pulse* request)
{
    static char buffer[PULSE_COMMAND_BUFFER_SIZE];

    std::vector<comms::msg_resp> responses;

    // Check if engine is attached
    if (!checkEngineExists())
    {
        uint16_t len = snprintf(buffer, PULSE_COMMAND_BUFFER_SIZE, "Engine not attached\r\n");
        responses =  comms::msg_resp::partialize_msg(request->GetType(), buffer, len, true);
        sendResponses(connection, responses, 10);
        return;
    }
    
    // Check if element exists
    if (!checkElementExists(request->GetElementName()))
    {
        uint16_t len = snprintf(buffer, PULSE_COMMAND_BUFFER_SIZE, "Could not find element: %s\r\n", request->GetElementName().c_str());
        responses =  comms::msg_resp::partialize_msg(request->GetType(), buffer, len, true);
        sendResponses(connection, responses, 10);
        return;
    }

    // Get engine
    Engine* engine = board->GetEngine();

    // Get element
    Element* element = engine->GetElement(request->GetElementName());

    // Get element type
    ElementType type = Element::GetType(element);

    if (type != ElementType::NodeDigital && type != ElementType::NodeAnalog)
    {
        uint16_t len = snprintf(buffer, PULSE_COMMAND_BUFFER_SIZE, "Element %s is not of type: ElementType::NodeDigital or ElementType::NodeAnalog\r\n", request->GetElementName().c_str());
        responses =  comms::msg_resp::partialize_msg(request->GetType(), buffer, len, true);
        sendResponses(connection, responses, 10);
        return;
    }

    // Pulse node if available
    if (type == ElementType::NodeDigital)
    {
        Node<bool>* elementNode = (Node<bool>*)element;
        elementNode->OverrideValue(request->GetValue() > 0.5f, request->GetDuration());
    }
    else
    {
        Node<float>* elementBase = (Node<float>*)element;
        elementBase->OverrideValue(request->GetValue(), request->GetDuration()); 
    }

    uint16_t len = snprintf(buffer, PULSE_COMMAND_BUFFER_SIZE, "Pulse command executed\r\n");
    responses =  comms::msg_resp::partialize_msg(request->GetType(), buffer, len, true);
    sendResponses(connection, responses, 10);
}

void DeviceCommandHandler::executeSERCommand(AbstractConnectionHandler* connection, comms::msg_req_ser* request)
{
    static char buffer[SER_COMMAND_BUFFER_SIZE];

    std::vector<comms::msg_resp> responses;

    // Check if engine is attached
    if (!checkEngineExists())
    {
        uint16_t len = snprintf(buffer, SER_COMMAND_BUFFER_SIZE, "Engine not attached\r\n");
        responses =  comms::msg_resp::partialize_msg(request->GetType(), buffer, len, true);
        sendResponses(connection, responses, 10);
        return;
    }
    
    // Check if element exists
    if (!checkElementExists("__SER__"))
    {
        uint16_t len = snprintf(buffer, SER_COMMAND_BUFFER_SIZE, "No SER active\r\n");
        responses =  comms::msg_resp::partialize_msg(request->GetType(), buffer, len, true);
        sendResponses(connection, responses, 10);
        return;
    }

    // Get engine
    Engine* engine = board->GetEngine();

    // Get element named "SER"
    Element* element = engine->GetElement("__SER__");

    // Check if the element is of type ElementType::SER
    if(Element::GetType(element) == ElementType::SER)
    {
        SER* elementSER = (SER*)element;

        // Get the number of SERs requested
        uint16_t nSERs = request->GetNumSERs();

        // Allocate memory for the event buffer
        SER::SEREvent* eventBuffer = new SER::SEREvent[nSERs];

        // Get the total number of events
        uint16_t eventsTotal = elementSER->GetEventLog(eventBuffer, nSERs);

        // If there are events, send a response with the number of records
        if(eventsTotal > 0)
        {
            uint16_t len = snprintf(buffer, SER_COMMAND_BUFFER_SIZE, "Sequential Event Recorder Records (%u Records):\r\n", eventsTotal);
            responses =  comms::msg_resp::partialize_msg(request->GetType(), buffer, len, true);
            sendResponses(connection, responses, 10);
        }

        // Iterate through the events and send responses
        for(uint16_t i = 0; i < eventsTotal; i++)
        {
            // Check if the escape key was requested
            if(connection->getEscapeKeyRequested())
            {
                connection->acknowledgeEscapeKeyRequest();
                break;
            }

            SER::SEREvent* event = &eventBuffer[i];
            char timeBuffer[64];

            // Print the short time of the event
            event->time.PrintShortTime(timeBuffer, 64);

            uint16_t len = 0;

            // Check the event type and format the response accordingly
            if(eventBuffer[i].type == SER::SEREventType::RISING_EDGE)
                len = snprintf(buffer, SER_COMMAND_BUFFER_SIZE, " [%-3u] %s\t_/⎺ ASSERTED\t%s\r\n", 
                    i, 
                    engine->GetElementName(event->e).c_str(),
                    timeBuffer);
            else if(eventBuffer[i].type == SER::SEREventType::FALLING_EDGE)
                len = snprintf(buffer, SER_COMMAND_BUFFER_SIZE, " [%-3u] %s\t⎺\\_ DEASSRTED\t%s\r\n",
                    i,
                    engine->GetElementName(event->e).c_str(),
                    timeBuffer);

            // Send the response
            responses =  comms::msg_resp::partialize_msg(request->GetType(), buffer, len, true);
            sendResponses(connection, responses, 10);
        }

        // Free the allocated memory for the event buffer
        delete[] eventBuffer;
    }
}


bool DeviceCommandHandler::checkEngineExists()
{
    // Get engine
    Engine* engine = board->GetEngine();

    // Check if engine is attached
    return engine != nullptr;
}

bool DeviceCommandHandler::checkElementExists(const std::string& elementName)
{
    // Get engine
    Engine* engine = board->GetEngine();

    // Get element
    Element* element = engine->GetElement(elementName);

    // Check if element exists
    return element != nullptr;
}

void DeviceCommandHandler::sendResponses(AbstractConnectionHandler* connectionHandler, const std::vector<comms::msg_resp>& responses, uint16_t delayMS)
{
    // Send responses
    for (const comms::msg_resp& response : responses)
    {    
        // Send buffer
        connectionHandler->WriteLine(response.buffer);

        // Sleep
        if (delayMS > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(delayMS));
    }
}

} // namespace LogicElements
