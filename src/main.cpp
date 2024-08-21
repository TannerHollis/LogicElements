//Include project files
#include "le_Builder/le_Builder.hpp"
#include "le_Engine_Test.hpp"
#include "le_DNP3Outstation_Test.hpp"

// Include threading
#include <thread>

int main(int argc, char* argv[])
{
    if (argc > 1)
    {
        printf("Loading file: %s\n", argv[1]);

        // Pointers to hold the engine and DNP3 configuration
        le_Engine* engine = nullptr;
        le_DNP3Outstation_Config* dnp3Config = nullptr;

        // Load engine and DNP3 configuration from file
        if (le_Builder::LoadFromFile(argv[1], engine, dnp3Config))
        {
            printf("Engine loaded successfully.\n");

            if (engine)
            {
                engine->ConfigureTimer(1e6);
                engine->Update(0.0001f);

                char buffer[1024];

                engine->GetInfo(buffer, 1024);

                printf("%s", buffer);

                bool running = true;

                // Check if DNP3 configuration was loaded
                if (dnp3Config)
                {
                    printf("DNP3 configuration loaded successfully.\n");

                    // Load DNP3 outstation with the configuration
                    le_DNP3Outstation outstation(*dnp3Config);
                    outstation.ValidatePoints(engine);
                    outstation.Enable();

                    while (running)
                    {
                        engine->Update(0.020f);
                        outstation.Update();
                        std::this_thread::sleep_for(std::chrono::milliseconds(20));
                    }
                }
                else
                {
                    printf("No DNP3 configuration found.\n");
                }
            }
            else
            {
                printf("Failed to load engine.\n");
            }
        }
        else
        {
            // Error occurred, retrieve and print the error details
            char errorBuffer[512];
            le_Builder::GetErrorString(errorBuffer, sizeof(errorBuffer));
            printf("Error: %s\n", errorBuffer);
        }

        // Clean up resources if necessary
        delete engine;
        delete dnp3Config;
    }
	else
	{
		printf("Loading default test case.\n");
		bool running = true;

		// Load test engine
		le_Engine_Test engine = le_Engine_Test();
		le_DNP3Outstation_Test dnp3Session = le_DNP3Outstation_Test();
		dnp3Session.ValidatePoints(&engine);
		dnp3Session.Enable();

		while (running)
		{
			engine.Update(0.020f);
			dnp3Session.Update();
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
		}
	}

	return 0;
}