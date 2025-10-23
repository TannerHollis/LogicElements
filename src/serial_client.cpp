#include "serialib.h"
#include "Communication/comms.hpp"
#include <iostream>
#include <thread>
#include <sstream>
#include <atomic>
#include <condition_variable>

// Atomic flags for running state and response waiting state
std::atomic<bool> running{ true };
std::atomic<bool> waiting_for_response{ false };

// Condition variable and mutex for synchronizing response handling
std::condition_variable cv;
std::mutex cv_m;

// Thread function to receive messages from the server
void receiveThread(serialib* serial)
{
    // Create response buffer
    char buffer[512];

    while (running)
    {
        // Receive data from the server
        memset(buffer, '/0', 512);
        int bytesRead = serial->readBytes(buffer, sizeof(comms::msg_resp), 100);
        if (bytesRead > 0)
        {
            snprintf(buffer, bytesRead, "%s", buffer);
            std::cout << std::string(buffer);
        }

        // Sleep for a short duration before checking again
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

// Function to print available commands
void print_help() {
    std::cout << "Available commands:\n";
    std::cout << "  TARGET <string elementName> <uint8_t outputSlot> <uint8_t repetition = 1> <uint16_t delayMS = 1000>\n";
    std::cout << "  PULSE <string elementName> <bool/float value> <float duration>\n";
    std::cout << "  ID\n";
    std::cout << "  STATUS\n";
    std::cout << "  QUIT\n";
}

int main(int argc, char* argv[])
{
    // Serial port and baud rate
    const char* port = "COM4";
    const unsigned int baudRate = 9600;

    // Create serial client
    serialib serial;

    // Attempt to open connection to the server
    char openStatus = serial.openDevice(port, baudRate);
    if (openStatus == 1)
    {
        std::cout << "\n>> ";

        // Start receiver thread
        std::thread receiver(receiveThread, &serial);

        while (true)
        {
            // Get user input
            char input;
            while (std::cin.get(input))
            {
                // Send the character
                int bytesWritten = serial.writeBytes(&input, 1);
                if (bytesWritten != 1) {
                    std::cerr << "Failed to send the character.\n";
                }
            }
        }

        // Join receiver thread and shut down client
        receiver.join();
        serial.closeDevice();
    }
    else
    {
        std::cerr << "Failed to connect to server.\n";
    }

    return 0;
}
