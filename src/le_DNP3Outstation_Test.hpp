#pragma once

#include "Communication/le_DNP3Outstation.hpp"

using namespace LogicElements;

#ifdef LE_DNP3

#define OUTSTATION_IP "0.0.0.0"
#define OUTSTATION_DNP 10
#define CLIENT_IP "192.168.0.30"
#define CLIENT_DNP 1
#define PORT 20000

class le_DNP3Outstation_Test : public DNP3Outstation
{
public:
	le_DNP3Outstation_Test() : DNP3Outstation(CreateExampleConfig()) {}

private:
	static DNP3OutstationConfig CreateExampleConfig()
	{
		// Declare outstation config
		DNP3OutstationConfig config;
		config.name = "Test DNP3 Outstation";
		config.outstation.ip = OUTSTATION_IP;
		config.outstation.dnp = OUTSTATION_DNP;
		config.outstation.port = PORT;

		// Configure the session
		DNP3OutstationSessionConfig session;
		session.name = "Test Session";
		session.client.ip = CLIENT_IP;
		session.client.dnp = CLIENT_DNP;
		session.client.port = PORT;
		session.allowUnsolicited = true;
		session.eventBufferLength = 100;

		// Add I/O
		session.AddBinaryInput(0, "OUT0");
		session.AddBinaryInput(1, "OUT1");
		session.AddBinaryOutput(0, "IN0");
		session.AddBinaryOutput(1, "IN1");

		// Add session to outstation config
		config.AddSession(session);

		return config;
	}
};
#endif