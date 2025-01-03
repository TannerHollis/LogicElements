#include "le_AbstractConnectionServer.hpp"
#include <cstdio>

/**
 * @brief Constructor for the TCP connection server.
 * @param port The port number to listen on.
 */
le_TcpConnectionServer::le_TcpConnectionServer(uint16_t port)
    : tcp_server(MinimalSocket::Port(port), MinimalSocket::AddressFamily::IP_V4) {}

/**
 * @brief Opens the TCP server for accepting new client connections.
 * @return True if the server was successfully opened, false otherwise.
 */
bool le_TcpConnectionServer::open() 
{
    return tcp_server.open();
}

/**
 * @brief Closes the TCP server and stops accepting new client connections.
 */
void le_TcpConnectionServer::close() 
{
    tcp_server.shutDown();
}

/**
 * @brief Accepts a new client connection and returns a connection handler.
 * @return A unique pointer to the connection handler for the new client.
 */
std::unique_ptr<le_AbstractConnectionHandler> le_TcpConnectionServer::acceptNewClient() 
{
    std::optional<MinimalSocket::tcp::TcpConnectionBlocking> maybe_accepted_connection = tcp_server.acceptNewClient();
    
    if (!maybe_accepted_connection.has_value()) {
        return nullptr;
    }
    
#ifdef DEBUG_SERVER_CONNECTION
    std::printf("New client accepted on TCP port: %d\n", maybe_accepted_connection.value().getSocketDescriptor());
#endif

    return std::make_unique<le_TcpConnectionHandler>(maybe_accepted_connection.value());
}

/**
 * @brief Gets the name of the TCP server.
 * @return The name of the server as a string.
 */
std::string le_TcpConnectionServer::getName() const
{
    return "TCP Connection Server";
}

/**
 * @brief Constructor for the Serial connection server.
 * @param portName The name of the serial port.
 * @param baudRate The baud rate for the serial connection.
 */
le_SerialConnectionServer::le_SerialConnectionServer(const std::string& portName, unsigned int baudRate)
    : portName(portName), baudRate(baudRate), initialized(false) {}

/**
 * @brief Opens the Serial server for accepting new client connections.
 * @return True if the server was successfully opened, false otherwise.
 */
bool le_SerialConnectionServer::open() 
{
    return serial_port.openDevice(portName.c_str(), baudRate, SERIAL_DATABITS_8, SERIAL_PARITY_NONE, SERIAL_STOPBITS_1, 1000) == 1;
}

/**
 * @brief Closes the Serial server and stops accepting new client connections.
 */
void le_SerialConnectionServer::close() {}

/**
 * @brief Accepts a new client connection and returns a connection handler.
 * @return A unique pointer to the connection handler for the new client.
 */
std::unique_ptr<le_AbstractConnectionHandler> le_SerialConnectionServer::acceptNewClient() 
{

    // Check if the serial port is open. If not, return nullptr.
    if (initialized) {
        return nullptr;
    }

#ifdef DEBUG_SERVER_CONNECTION
    std::printf("New client accepted on serial port: %s\n", portName.c_str());
#endif

    initialized = true;

    // For serial connections, we don't accept new clients in the same way as TCP.
    // Instead, we just return a connection handler for the already opened serial port.
    return std::make_unique<le_SerialConnectionHandler>(&serial_port);
}

/**
 * @brief Gets the name of the Serial server.
 * @return The name of the server as a string.
 */
std::string le_SerialConnectionServer::getName() const
{
    return "Serial Connection Server";
}