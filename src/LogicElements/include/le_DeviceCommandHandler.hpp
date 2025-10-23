#pragma once

#include "le_AbstractConnectionServer.hpp"

// Include Logic_Elements library
#include "le_Board.hpp"

// Include communication messages
#include "comms.hpp"

// Include C++ libaries
#include <thread>
#include <vector>
#include <atomic>
#include <iostream>

namespace LogicElements {

/**
 * @brief Class to handle device commands.
 */
class DeviceCommandHandler
{
public:
    /**
     * @brief Constructor for the device command handler.
     * @param connectionServer Pointer to the connection server.
     * @param board Pointer to the board.
     * @param multipleConnectionCapability Flag indicating if multiple connections are supported.
     */
    DeviceCommandHandler(AbstractConnectionServer* connectionServer, Board* board, bool multipleConnectionCapability = false);

    /**
     * @brief Destructor for the device command handler.
     */
    ~DeviceCommandHandler();

    /**
     * @brief Starts the device command handler server.
     */
    void start();

    /**
     * @brief Stops the device command handler server.
     */
    void stop();

private:
    AbstractConnectionServer* connectionServer; /**< Pointer to the connection server. */
    Board* board; /**< Pointer to the board. */
    std::atomic<bool> running; /**< Atomic flag to indicate if the server is running. */
    std::thread server_thread; /**< Thread for the server. */
    std::vector<std::thread> client_threads; /**< Vector of threads for the clients. */
    bool multipleConnectionCapability; /**< Flag indicating if multiple connections are supported. */

    /**
     * @brief Accepts new clients in a loop and creates a thread for each client.
     */
    void acceptClients();

    /**
     * @brief Handles communication with a connected client.
     * @param connectionHandler Pointer to the connection handler.
     */
    void handleClient(AbstractConnectionHandler* connectionHandler);

    /**
     * @brief Handles a command request.
     * @param request Pointer to the request message.
     * @return A vector of response messages.
     */
    void handleCommand(AbstractConnectionHandler* connection, comms::msg_req* request);
    /**
     * @brief Executes the echo command.
     * @param connection Pointer to the connection handler.
     * @param request Pointer to the request message.
     */
    void executeEchoCommand(AbstractConnectionHandler* connection, comms::msg_req_echo* request);

    /**
     * @brief Executes the ID command.
     * @param connection Pointer to the connection handler.
     * @param request Pointer to the request message.
     */
    void executeIDCommand(AbstractConnectionHandler* connection, comms::msg_req* request);

    /**
     * @brief Executes the status command.
     * @param connection Pointer to the connection handler.
     * @param request Pointer to the request message.
     */
    void executeStatusCommand(AbstractConnectionHandler* connection, comms::msg_req* request);

    /**
     * @brief Executes the target command.
     * @param connection Pointer to the connection handler.
     * @param request Pointer to the request message.
     */
    void executeTargetCommand(AbstractConnectionHandler* connection, comms::msg_req_target* request);

    /**
     * @brief Executes the pulse command.
     * @param connection Pointer to the connection handler.
     * @param request Pointer to the request message.
     */
    void executePulseCommand(AbstractConnectionHandler* connection, comms::msg_req_pulse* request);

    /**
     * @brief Executes the SER command.
     * @param connection Pointer to the connection handler.
     * @param request Pointer to the request message.
     */
    void executeSERCommand(AbstractConnectionHandler* connection, comms::msg_req_ser* request);

    /**
     * @brief Checks if the engine exists.
     * @param request Pointer to the request message.
     * @param errorFormat Error format string.
     * @return True if the engine exists, false otherwise.
     */
    bool checkEngineExists();

    /**
     * @brief Checks if the element exists.
     * @param request Pointer to the request message.
     * @param errorFormat Error format string.
     * @return True if the element exists, false otherwise.
     */
    bool checkElementExists(const std::string& elementName);

    /**
     * @brief Sends response messages to the client.
     * @param connectionHandler Pointer to the connection handler.
     * @param responses Vector of response messages.
     * @param delayMS Delay in milliseconds between sending responses.
     */
    void sendResponses(AbstractConnectionHandler* connectionHandler, const std::vector<comms::msg_resp>& responses, uint16_t delayMS);
};

} // namespace LogicElements
