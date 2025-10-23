//Include project files
#include "le_DeviceSettings.hpp"
#include "le_Builder/le_Builder.hpp"
#include "le_Board.hpp"
#include "le_DeviceUtility.hpp"
#include "le_DeviceCommandHandler.hpp"

// Include test cases
#include "le_Engine_Test.hpp"
#include "le_DNP3Outstation_Test.hpp"

// Include threading
#include <thread>

using namespace LogicElements;

int main(int argc, char* argv[])
{
    // Set board configuration
    Board::BoardConfig boardConfig("Offset Power Relay Test Bench", "OS-A1-B0-C0-D0-E1");
    Board* board = new Board(boardConfig);
    
    // Test Time
    Time time = Time::GetTime();
    char timeStringBuffer[64];
    time.PrintShortTime(timeStringBuffer, sizeof(timeStringBuffer));
    printf("Time: %s\n", timeStringBuffer);

    if (argc == 2)
    {
        // Instantiate DeviceSettings
        DeviceSettings deviceSettings("settings.json");

        // Start Serial command handler
        SerialConnectionServer serialPort(deviceSettings.getSerialPort(), deviceSettings.getSerialSpeed());
        DeviceCommandHandler serialCommandHandler(&serialPort, board);
        if (deviceSettings.getSerialPortEnable())
            serialCommandHandler.start();

        // Start TCP command handler
        TcpConnectionServer tcpConnectionServer(deviceSettings.getSocketPort());
        DeviceCommandHandler socketCommandHandler(&tcpConnectionServer, board);
        if (deviceSettings.getSocketEnable())
            socketCommandHandler.start();

        // Load the active configuration
        std::string configPath = deviceSettings.getActiveConfig();
        bool configParseResult = Builder::LoadFromFile(configPath, board);

        if (!configParseResult)
        {
            char errorBuffer[512];
            Builder::GetErrorString(errorBuffer, sizeof(errorBuffer));
            printf("Failed to load configuration: %s\n", errorBuffer);
        }
        else
        {
            // Check for major and minor errors
            if (Builder::GetMajorError() != Builder::MajorError::NONE || Builder::GetMinorError() != Builder::MinorError::NONE)
            {
                char errorBuffer[512];
                Builder::GetErrorString(errorBuffer, sizeof(errorBuffer));
                printf("Configuration loaded with errors: %s\n", errorBuffer);
            }
            else
            {
                board->Start();
                bool running = true;

                auto nextFrame = std::chrono::steady_clock::now();
                while (running)
                {
                    nextFrame += std::chrono::microseconds(100000); // 10 Hz
                    Time timeStamp = Time::GetTime();
                    board->Update(timeStamp);
                    std::this_thread::sleep_until(nextFrame);
                }
            }
        }
    }
	else
	{
		printf("Loading default test case.\n");
		bool running = true;

		// Load test engine
		le_Engine_Test engine = le_Engine_Test();
#ifdef LE_DNP3
		le_DNP3Outstation_Test dnp3Session = le_DNP3Outstation_Test();
		dnp3Session.ValidatePoints(&engine);
		dnp3Session.Enable();
#endif

		auto nextFrame = std::chrono::steady_clock::now();
        while (running)
        {
            nextFrame += std::chrono::microseconds(16667); // 60 Hz
            Time timeStamp = Time::GetTime();
			engine.Update(timeStamp);
#ifdef LE_DNP3
			dnp3Session.Update();
#endif
			std::this_thread::sleep_until(nextFrame);
		}
	}

    delete board;

	return 0;
}