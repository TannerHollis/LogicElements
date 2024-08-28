#pragma once

#include "le_AbstractConnectionHandler.hpp"
#include <string>

#define DEBUG_SERVER_CONNECTION

/**
 * @brief Abstract base class for connection servers.
 */
class le_AbstractConnectionServer {
public:
    virtual ~le_AbstractConnectionServer() = default;

    /**
     * @brief Opens the server for accepting new client connections.
     * @return True if the server was successfully opened, false otherwise.
     */
    virtual bool open() = 0;

    /**
     * @brief Closes the server and stops accepting new client connections.
     */
    virtual void close() = 0;

    /**
     * @brief Accepts a new client connection and returns a connection handler.
     * @return A unique pointer to the connection handler for the new client.
     */
    virtual std::unique_ptr<le_AbstractConnectionHandler> acceptNewClient() = 0;

    /**
     * @brief Gets the name of the server.
     * @return The name of the server as a string.
     */
    virtual std::string getName() const = 0;
};

/**
 * @brief TCP connection server class.
 */
class le_TcpConnectionServer : public le_AbstractConnectionServer {
public:
    /**
     * @brief Constructor for the TCP connection server.
     * @param port The port number to listen on.
     */
    le_TcpConnectionServer(uint16_t port);

    /**
     * @brief Opens the TCP server for accepting new client connections.
     * @return True if the server was successfully opened, false otherwise.
     */
    bool open() override;

    /**
     * @brief Closes the TCP server and stops accepting new client connections.
     */
    void close() override;

    /**
     * @brief Accepts a new client connection and returns a connection handler.
     * @return A unique pointer to the connection handler for the new client.
     */
    std::unique_ptr<le_AbstractConnectionHandler> acceptNewClient() override;

    /**
     * @brief Gets the name of the server.
     * @return The name of the server as a string.
     */
    std::string getName() const override;

private:
    MinimalSocket::tcp::TcpServer<true> tcp_server;
};

/**
 * @brief Serial connection server class.
 */
class le_SerialConnectionServer : public le_AbstractConnectionServer {
public:
    /**
     * @brief Constructor for the Serial connection server.
     * @param portName The name of the serial port.
     * @param baudRate The baud rate for the serial connection.
     */
    le_SerialConnectionServer(const std::string& portName, unsigned int baudRate);

    /**
     * @brief Opens the Serial server for accepting new client connections.
     * @return True if the server was successfully opened, false otherwise.
     */
    bool open() override;

    /**
     * @brief Closes the Serial server and stops accepting new client connections.
     */
    void close() override;

    /**
     * @brief Accepts a new client connection and returns a connection handler.
     * @return A unique pointer to the connection handler for the new client.
     */
    std::unique_ptr<le_AbstractConnectionHandler> acceptNewClient() override;

    /**
     * @brief Gets the name of the server.
     * @return The name of the server as a string.
     */
    std::string getName() const override;

private:
    std::string portName;
    unsigned int baudRate;
    serialib serial_port;
    bool initialized;
    std::mutex serial_port_mutex;
};

