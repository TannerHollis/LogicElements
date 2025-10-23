#include "Communication/le_AbstractConnectionHandler.hpp"

namespace LogicElements {

#define CONNECTION_TIMEOUT_MS 1000

AbstractConnectionHandler::AbstractConnectionHandler()
    : commandInProcess(false), historyIndex(-1), escapeKeyRequested(false) {}

/**
 * @brief Reads characters from the connection into the buffer.
 */
void AbstractConnectionHandler::ReadToBuffer()
{
    char buffer[64]; // Buffer to store incoming characters
    size_t bytesRead = read(buffer, sizeof(buffer)); // Read characters from the connection

    char returnBuffer[64]; // Buffer to store characters to be echoed back
    uint16_t returnBytes = 0; // Number of bytes to be echoed back

    // Process each character read from the connection
    for (size_t i = 0; i < bytesRead; ++i)
    {
        char c = buffer[i]; // Current character

        // Check for escape character, otherwise if command is in process break
        if (c == '\033' && bytesRead == 1)
        {
            escapeKeyRequested = true;
            break;
        }
        else if (this->commandInProcess)
        {
            break;
        }
        
        // Handle newline or carriage return characters
        if (c == '\n' || c == '\r')
        {
            // If the character buffer is not empty, save the command history
            if (!charBuffer.empty())
            {
                commandHistory.push_back(charBuffer);
                historyIndex = commandHistory.size();
            }
            // Push the complete line to the line buffer and clear the character buffer
            lineBuffer.push(charBuffer);
            charBuffer.clear();
            // Add carriage return and newline to the return buffer
            returnBuffer[returnBytes] = '\r';
            returnBuffer[returnBytes + 1] = '\n';
            returnBytes += 2;
            break;
        }
        // Handle escape sequences for special keys
        else if (c == '\033')
        {
            if (i + 2 < bytesRead && buffer[i + 1] == '[')
            {
                HandleSpecialKey(buffer[i + 2]);
                i += 2;
            }
            break;
        }
        // Handle backspace key
        else if (c == '\b' || c == 127)
        {
            if (!charBuffer.empty())
            {
                charBuffer.pop_back();
                returnBuffer[returnBytes] = c;
                returnBytes++;
            }
        }
        // Handle regular characters
        else
        {
            charBuffer += c;
            returnBuffer[returnBytes] = c;
            returnBytes++;
        }
    }

    // Echo back characters if any
    if(returnBytes > 0)
        write(returnBuffer, returnBytes);
}

/**
 * @brief Gets a complete line from the buffer.
 * @return The complete line as a string.
 */
std::string AbstractConnectionHandler::ReadLine()
{
    if(lineBuffer.empty())
    {
        ReadToBuffer();
    }

    if (!lineBuffer.empty())
    {
        std::string line = lineBuffer.front();
        lineBuffer.pop();
        return line;
    }

    return "";
}

/**
 * @brief Writes a line of output to the connection.
 * @param line The output line to write.
 */
uint16_t AbstractConnectionHandler::WriteLine(const std::string& line)
{
    return write(line.c_str(), line.length());
}

/**
 * @brief Sets the command in process state.
 * @param inProcess The state to set.
 */
void AbstractConnectionHandler::SetCommandInProcess(bool inProcess)
{
    commandInProcess = inProcess;
}

/**
 * @brief Handles special keys such as arrow keys.
 * @param key The special key code.
 */
void AbstractConnectionHandler::HandleSpecialKey(char key)
{
    if (key == 'A') // Up arrow
    {
        if (historyIndex > 0)
        {
            historyIndex--;
            charBuffer = commandHistory[historyIndex];
            WriteLine("\r>> " + charBuffer  + "\033[K"); // Clear line and print command
        }
    }
    else if (key == 'B') // Down arrow
    {
        if (historyIndex < commandHistory.size() - 1)
        {
            historyIndex++;
            charBuffer = commandHistory[historyIndex];
            WriteLine("\r>> " + charBuffer  + "\033[K"); // Clear line and print command
        }
        else
        {
            historyIndex = commandHistory.size();
            charBuffer.clear();
            WriteLine("\r>> \033[K"); // Clear line
        }
    }
}

/**
 * @brief Gets the escape key requested state.
 * @return The state of escape key requested.
 */
bool AbstractConnectionHandler::getEscapeKeyRequested() const
{
    return escapeKeyRequested;
}

/**
 * @brief Acknowledges the escape key request by setting it to false.
 */
void AbstractConnectionHandler::acknowledgeEscapeKeyRequest()
{
    escapeKeyRequested = false;
}

/**
 * @brief Constructor for the TCP connection handler.
 * @param connection The TCP connection object.
 */
TcpConnectionHandler::TcpConnectionHandler(MinimalSocket::tcp::TcpConnectionBlocking& connection)
    : connection(std::move(connection)) {}

/**
 * @brief Reads data from the TCP connection.
 * @param buffer The buffer to store the read data.
 * @param size The size of the buffer.
 * @return The number of bytes read.
 */
uint16_t TcpConnectionHandler::read(char* buffer, size_t size) 
{
    MinimalSocket::BufferView rxBuffer;
    rxBuffer.buffer = buffer;
    rxBuffer.buffer_size = size;
    return (uint16_t)connection.receive(rxBuffer, MinimalSocket::Timeout(CONNECTION_TIMEOUT_MS));
}

/**
 * @brief Writes data to the TCP connection.
 * @param buffer The buffer containing the data to write.
 * @param size The size of the buffer.
 * @return The number of bytes written.
 */
uint16_t TcpConnectionHandler::write(const char* buffer, size_t size) 
{
    MinimalSocket::BufferViewConst txBuffer;
    txBuffer.buffer = const_cast<char*>(buffer);
    txBuffer.buffer_size = size;
    
    if (!connection.send(txBuffer)) {
        return 0;
    }
    
    return static_cast<uint16_t>(size);
}

/**
 * @brief Closes the TCP connection.
 */
void TcpConnectionHandler::close() 
{
    connection.shutDown();
}

/**
 * @brief Constructor for the Serial connection handler.
 * @param connection The Serial connection object.
 */
SerialConnectionHandler::SerialConnectionHandler(serialib* connection)
    : connection(connection) {}

/**
 * @brief Reads data from the Serial connection.
 * @param buffer The buffer to store the read data.
 * @param size The size of the buffer.
 * @return The number of bytes read.
 */
uint16_t SerialConnectionHandler::read(char* buffer, size_t size) 
{
    //ioLock.lock();

    int ret = connection->readBytes(buffer, (unsigned int)size, 10);
    
    //ioLock.unlock();

    // Write the parameters and return -1 if an error occurred
    return ret <= 0 ? 0 : (uint16_t)ret;
}

/**
 * @brief Writes data to the Serial connection.
 * @param buffer The buffer containing the data to write.
 * @param size The size of the buffer.
 * @return The number of bytes written.
 */
uint16_t SerialConnectionHandler::write(const char* buffer, size_t size) 
{
    //ioLock.lock();

    int ret = connection->writeBytes(buffer, (unsigned int)size);
    
    //ioLock.unlock();
    
    return ret == 1 ? size : 0;
}

/**
 * @brief Closes the Serial connection.
 */
void SerialConnectionHandler::close() {}

} // namespace LogicElements
