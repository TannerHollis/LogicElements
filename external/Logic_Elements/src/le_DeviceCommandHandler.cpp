#include "le_DeviceCommandHandler.hpp"

#include <algorithm>
#include <cstdio>

#define ECHO_COMMAND_BUFFER_SIZE 64
#define ID_COMMAND_BUFFER_SIZE 256
#define STATUS_COMMAND_BUFFER_SIZE 8192
#define TARGET_COMMAND_BUFFER_SIZE 256
#define PULSE_COMMAND_BUFFER_SIZE 256
#define SER_COMMAND_BUFFER_SIZE 256

le_DeviceCommandHandler::le_DeviceCommandHandler(le_AbstractConnectionServer* connectionServer, le_Board* board, bool multipleConnectionCapability)
    : connectionServer(connectionServer), board(board), running(false), multipleConnectionCapability(multipleConnectionCapability) {}

le_DeviceCommandHandler::~le_DeviceCommandHandler()
{
    stop();
}

void le_DeviceCommandHandler::start()
{
    running = true;
    bool success = connectionServer->open();

    if (success)
    {
#ifdef DEBUG_SERVER_CONNECTION
        std::printf("%s was successfully opened.\n", connectionServer->getName().c_str());
#endif
        // Start a thread to handle client connections
        server_thread = std::thread(&le_DeviceCommandHandler::acceptClients, this);
    }
}

void le_DeviceCommandHandler::stop()
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

void le_DeviceCommandHandler::acceptClients()
{
    bool acceptMultipleClients = true;
    while (acceptMultipleClients)
    {
        // Accept new clients
        std::unique_ptr<le_AbstractConnectionHandler> connectionHandler = connectionServer->acceptNewClient();

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
        client_threads.emplace_back(&le_DeviceCommandHandler::handleClient, this, connectionHandler.release());
    }
}

void le_DeviceCommandHandler::handleClient(le_AbstractConnectionHandler* connectionHandler)
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
            responseThread = std::thread(&le_DeviceCommandHandler::handleCommand, this, connectionHandler, request.release());
        }
    }
}

void le_DeviceCommandHandler::handleCommand(le_AbstractConnectionHandler* connection, comms::msg_req* request)
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

void le_DeviceCommandHandler::executeEchoCommand(le_AbstractConnectionHandler* connection, comms::msg_req_echo* request)
{
    // Echo back only first argument
    static char buffer[ECHO_COMMAND_BUFFER_SIZE];
    uint16_t len = snprintf(buffer, ECHO_COMMAND_BUFFER_SIZE, "%s\r\n", request->GetEcho().c_str());

    std::vector<comms::msg_resp> responses = comms::msg_resp::partialize_msg(request->GetType(), buffer, len, false);

    sendResponses(connection, responses, 10);
}

void le_DeviceCommandHandler::executeIDCommand(le_AbstractConnectionHandler* connection, comms::msg_req* request)
{
    // Get ID string
    static char buffer[ID_COMMAND_BUFFER_SIZE];
    uint16_t len = board->GetInfo(buffer, ID_COMMAND_BUFFER_SIZE);

    std::vector<comms::msg_resp> responses = comms::msg_resp::partialize_msg(request->GetType(), buffer, len, false);

    sendResponses(connection, responses, 10);
}

void le_DeviceCommandHandler::executeStatusCommand(le_AbstractConnectionHandler* connection, comms::msg_req* request)
{
    static char buffer[STATUS_COMMAND_BUFFER_SIZE];

    // Get engine
    le_Engine* engine = board->GetEngine();

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

void le_DeviceCommandHandler::executeTargetCommand(le_AbstractConnectionHandler* connection, comms::msg_req_target* request)
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
    le_Engine* engine = board->GetEngine();

    // Get element
    le_Element* element = engine->GetElement(request->GetElementName());

    // Check if element is digital (boolean)
    bool isDigital = dynamic_cast<le_Base<bool>*>(element);

    // Get output slot
    uint8_t outputSlot = (uint8_t)request->GetOutputSlot();

    // Check if outputSlot is within the number of output slots on the element
    if (isDigital)
    {
        le_Base<bool>* elementBase = (le_Base<bool>*)element;
        if (outputSlot >= elementBase->GetNumberOfOutputSlots())
        {
            uint16_t len = snprintf(buffer, TARGET_COMMAND_BUFFER_SIZE, "Output slot %u is out of range for element %s\r\n", outputSlot, request->GetElementName().c_str());
            responses =  comms::msg_resp::partialize_msg(request->GetType(), buffer, len, true);
            sendResponses(connection, responses, 10);
            return;
        }
    }
    else
    {
        le_Base<float>* elementBase = (le_Base<float>*)element;
        if (outputSlot >= elementBase->GetNumberOfOutputSlots())
        {
            uint16_t len = snprintf(buffer, TARGET_COMMAND_BUFFER_SIZE, "Output slot %u is out of range for element %s\r\n", outputSlot, request->GetElementName().c_str());
            responses =  comms::msg_resp::partialize_msg(request->GetType(), buffer, len, true);
            sendResponses(connection, responses, 10);
        }
    }

    // Send target message
    for(uint16_t i = 0; i < request->GetRepetition(); i++)
    {
        if(connection->getEscapeKeyRequested())
        {
            connection->acknowledgeEscapeKeyRequest();
            break;
        }

        if (isDigital)
        {
            le_Base<bool>* elementBase = (le_Base<bool>*)element;
            uint16_t len = snprintf(buffer, TARGET_COMMAND_BUFFER_SIZE, "%s\t= %u\r\n", request->GetElementName().c_str(), elementBase->GetValue(outputSlot));
            responses =  comms::msg_resp::partialize_msg(request->GetType(), buffer, len, true);
            sendResponses(connection, responses, 10);
        }
        else
        {
            le_Base<float>* elementBase = (le_Base<float>*)element;
            uint16_t len = snprintf(buffer, TARGET_COMMAND_BUFFER_SIZE, "%s\t= %.4f\r\n", request->GetElementName().c_str(), elementBase->GetValue(outputSlot));
            responses =  comms::msg_resp::partialize_msg(request->GetType(), buffer, len, true);
            sendResponses(connection, responses, 10);
        }

        if(i != request->GetRepetition() - 1)
            std::this_thread::sleep_for(std::chrono::milliseconds(request->GetDelayMS()));
    }
}

void le_DeviceCommandHandler::executePulseCommand(le_AbstractConnectionHandler* connection, comms::msg_req_pulse* request)
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
    le_Engine* engine = board->GetEngine();

    // Get element
    le_Element* element = engine->GetElement(request->GetElementName());

    // Get element type
    le_Element_Type type = le_Element::GetType(element);

    if (type != le_Element_Type::LE_NODE_DIGITAL && type != le_Element_Type::LE_NODE_ANALOG)
    {
        uint16_t len = snprintf(buffer, PULSE_COMMAND_BUFFER_SIZE, "Element %s is not of type: LE_NODE_DIGITAL or LE_NODE_ANALOG\r\n", request->GetElementName());
        responses =  comms::msg_resp::partialize_msg(request->GetType(), buffer, len, true);
        sendResponses(connection, responses, 10);
        return;
    }

    // Pulse node if available
    if (type == le_Element_Type::LE_NODE_DIGITAL)
    {
        le_Node<bool>* elementNode = (le_Node<bool>*)element;
        elementNode->OverrideValue(request->GetValue() > 0.5f, request->GetDuration());
    }
    else
    {
        le_Node<float>* elementBase = (le_Node<float>*)element;
        elementBase->OverrideValue(request->GetValue(), request->GetDuration()); 
    }

    uint16_t len = snprintf(buffer, PULSE_COMMAND_BUFFER_SIZE, "Pulse command executed\r\n");
    responses =  comms::msg_resp::partialize_msg(request->GetType(), buffer, len, true);
    sendResponses(connection, responses, 10);
}

void le_DeviceCommandHandler::executeSERCommand(le_AbstractConnectionHandler* connection, comms::msg_req_ser* request)
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
    le_Engine* engine = board->GetEngine();

    // Get element named "SER"
    le_Element* element = engine->GetElement("__SER__");

    // Check if the element is of type LE_SER
    if(le_Element::GetType(element) == le_Element_Type::LE_SER)
    {
        le_SER* elementSER = (le_SER*)element;

        // Get the number of SERs requested
        uint16_t nSERs = request->GetNumSERs();

        // Allocate memory for the event buffer
        le_SER::le_SER_Event* eventBuffer = new le_SER::le_SER_Event[nSERs];

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

            le_SER::le_SER_Event* event = &eventBuffer[i];
            char timeBuffer[64];

            // Print the short time of the event
            event->time.PrintShortTime(timeBuffer, 64);

            uint16_t len = 0;

            // Check the event type and format the response accordingly
            if(eventBuffer[i].type == le_SER::le_SER_Event_Type::RISING_EDGE)
                len = snprintf(buffer, SER_COMMAND_BUFFER_SIZE, " [%-3u] %s\t_/⎺ASSERTED\t%s\r\n", 
                    i, 
                    engine->GetElementName(event->e).c_str(),
                    timeBuffer);
            else if(eventBuffer[i].type == le_SER::le_SER_Event_Type::FALLING_EDGE)
                len = snprintf(buffer, SER_COMMAND_BUFFER_SIZE, " [%-3u] %s\t⎺\\_DEASSRTED\t%s\r\n",
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


bool le_DeviceCommandHandler::checkEngineExists()
{
    // Get engine
    le_Engine* engine = board->GetEngine();

    // Check if engine is attached
    return engine != nullptr;
}

bool le_DeviceCommandHandler::checkElementExists(const std::string& elementName)
{
    // Get engine
    le_Engine* engine = board->GetEngine();

    // Get element
    le_Element* element = engine->GetElement(elementName);

    // Check if element exists
    return element != nullptr;
}

void le_DeviceCommandHandler::sendResponses(le_AbstractConnectionHandler* connectionHandler, const std::vector<comms::msg_resp>& responses, uint16_t delayMS)
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