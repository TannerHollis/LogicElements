#include <MinimalSocket/tcp/TcpClient.h>
#include "comms.hpp"
#include <iostream>
#include <thread>
#include <sstream>
#include <atomic>
#include <condition_variable>

using namespace MinimalSocket;

// Atomic flags for running state and response waiting state
std::atomic<bool> running{ true };
std::atomic<bool> waiting_for_response{ false };

// Condition variable and mutex for synchronizing response handling
std::condition_variable cv;
std::mutex cv_m;

// Thread function to receive messages from the server
void receiveThread(tcp::TcpClient<true>& tcp_client)
{
    // Create response buffer
    char buffer[512];
    BufferView bufferResp{ buffer, 512 };

    while (running)
    {
        // Receive data from the server
        if (tcp_client.receive(bufferResp) > 0)
        {
            // Dereference as msg
            comms::msg* msg = (comms::msg*)bufferResp.buffer;

            // Check if not a request
            if (msg->category == comms::msg_category::REQUEST)
                continue;

            // Dereference as response
            comms::msg_resp* resp = (comms::msg_resp*)bufferResp.buffer;

            // Handle partial response and complete
            if (resp->category == comms::msg_category::RESPONSE_PARTIAL || resp->category == comms::msg_category::RESPONSE_COMPLETE)
                std::cout << resp->buffer;

            // Notify waiting thread if response is complete or bad
            if (resp->category == comms::msg_category::RESPONSE_COMPLETE || resp->badResponse)
            {
                waiting_for_response = false;
                cv.notify_one();
            }
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
    // Server address and port
    std::string server_address = "192.168.0.205";
    Port server_port = 502;

    // Create TCP client
    tcp::TcpClient<true> tcp_client(Address{ server_address, server_port });

    // Attempt to open connection to the server
    bool success = tcp_client.open(Timeout(1000));
    if (success)
    {
        std::cout << "Connected!\n";

        // Start receiver thread
        std::thread receiver(receiveThread, std::ref(tcp_client));

        while (true)
        {
            
        }

        // Join receiver thread and shut down client
        receiver.join();
        tcp_client.shutDown();
    }
    else
    {
        std::cerr << "Failed to connect to server.\n";
    }

    return 0;
}
