#pragma once

// Include Minimal-Sockets library
#include <MinimalSocket/tcp/TcpServer.h>

// Include serialib library
#include "serialib.h"

#include <string>
#include <queue>
#include <vector>
#include <atomic>
#include <mutex>

/**
 * @brief Abstract class representing a connection handler for I/O operations.
 */
class le_AbstractConnectionHandler {
public:
    /**
     * @brief Constructor for the abstract connection handler.
     */
    le_AbstractConnectionHandler();
    
    virtual ~le_AbstractConnectionHandler() = default;

    /**
     * @brief Reads a line of input from the connection.
     * @return The input line as a string.
     */
    std::string ReadLine();

    /**
     * @brief Writes a line of output to the connection.
     * @param line The output line to write.
     */
    uint16_t WriteLine(const std::string& line);

    /**
     * @brief Sets the command in process state.
     * @param inProcess The state to set.
     */
    void SetCommandInProcess(bool inProcess);

    /**
     * @brief Gets the escape key requested state.
     * @return The state of escape key requested.
     */
    bool getEscapeKeyRequested() const;

    /**
     * @brief Acknowledges the escape key request by setting it to false.
     */
    void acknowledgeEscapeKeyRequest();

protected:
    /**
     * @brief Reads characters from the connection into the buffer.
     */
    void ReadToBuffer();

    /**
     * @brief Reads data from the connection.
     * @param buffer The buffer to store the read data.
     * @param size The size of the buffer.
     * @return The number of bytes read.
     */
    virtual uint16_t read(char* buffer, size_t size) = 0;

    /**
     * @brief Writes data to the connection.
     * @param buffer The buffer containing the data to write.
     * @param size The size of the buffer.
     * @return The number of bytes written.
     */
    virtual uint16_t write(const char* buffer, size_t size) = 0;

    /**
     * @brief Closes the device connection.
     */
    virtual void close() = 0;

    std::queue<std::string> lineBuffer; ///< Buffer to store complete lines.
    std::string charBuffer; ///< Buffer to store incoming characters.
    std::atomic<bool> commandInProcess;

    std::vector<std::string> commandHistory; ///< Buffer to store command history.
    int historyIndex; ///< Index for navigating command history.

    std::atomic<bool> escapeKeyRequested;

    /**
     * @brief Handles special keys such as arrow keys.
     * @param key The special key code.
     */
    void HandleSpecialKey(char key);    
};

class le_TcpConnectionHandler : public le_AbstractConnectionHandler {
public:
    /**
     * @brief Constructor for the TCP connection handler.
     * @param connection The TCP connection object.
     */
    le_TcpConnectionHandler(MinimalSocket::tcp::TcpConnectionBlocking& connection);

    void close() override;

protected:
    /**
     * @brief Reads data from the TCP connection.
     * @param buffer The buffer to store the read data.
     * @param size The size of the buffer.
     * @return The number of bytes read.
     */
    uint16_t read(char* buffer, size_t size) override;

    /**
     * @brief Writes data to the connection.
     * @param buffer The buffer containing the data to write.
     * @param size The size of the buffer.
     * @return The number of bytes written.
     */
    uint16_t write(const char* buffer, size_t size);

private:
    MinimalSocket::tcp::TcpConnectionBlocking connection;
};

class le_SerialConnectionHandler : public le_AbstractConnectionHandler {
public:
    /**
     * @brief Constructor for the Serial connection handler.
     * @param connection The Serial connection object.
     */
    le_SerialConnectionHandler(serialib* connection);

    void close() override;

protected:
    /**
     * @brief Reads data from the Serial connection.
     * @param buffer The buffer to store the read data.
     * @param size The size of the buffer.
     * @return The number of bytes read.
     */
    uint16_t read(char* buffer, size_t size) override;

    /**
     * @brief Writes data to the connection.
     * @param buffer The buffer containing the data to write.
     * @param size The size of the buffer.
     * @return The number of bytes written.
     */
    uint16_t write(const char* buffer, size_t size);

private:
    serialib* connection;
    std::mutex ioLock;
};