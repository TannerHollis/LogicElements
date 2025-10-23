#pragma once

#include "Core/le_Engine.hpp"

using namespace LogicElements;

// Test engine to test elements and functions
class le_Engine_Test : public Engine
{
public:
	le_Engine_Test() : Engine("Test Engine")
	{
		// Add inputs
		ElementTypeDef in0("IN0", ElementType::NodeDigital);
		ElementTypeDef in1("IN1", ElementType::NodeDigital);
		
		// Add logic
		ElementTypeDef or0("OR0", ElementType::OR);
		or0.args[0].uArg = 2;

		ElementTypeDef and0("AND0", ElementType::AND);
		and0.args[0].uArg = 2;

		// Add outputs
		ElementTypeDef out0("OUT0", ElementType::NodeDigital);
		ElementTypeDef out1("OUT1", ElementType::NodeDigital);

		// Add elements
		this->AddElement(&in0);
		this->AddElement(&in1);
		this->AddElement(&or0);
		this->AddElement(&and0);
		this->AddElement(&out0);
		this->AddElement(&out1);

		// Create net connections
		Engine::ElementNetTypeDef n0("IN0", "output");
		n0.AddInput("OR0", "input_0");
		n0.AddInput("AND0", "input_0");

		Engine::ElementNetTypeDef n1("IN1", "output");
		n1.AddInput("OR0", "input_1");
		n1.AddInput("AND0", "input_1");

		Engine::ElementNetTypeDef n2("OR0", "output");
		n2.AddInput("OUT0", "input");

		Engine::ElementNetTypeDef n3("AND0", "output");
		n3.AddInput("OUT1", "input");

		// Add nets
		this->AddNet(&n0);
		this->AddNet(&n1);
		this->AddNet(&n2);
		this->AddNet(&n3);

		// Update once to initialize the engine
		Time time = Time::GetTime();
		this->Update(time);

		time = Time::GetTime();
		this->Update(time);

		// Print resulting engine info
		char buffer[1024];
		int cnt = this->GetInfo(buffer, 1024);
		printf("Info length %u\r\n%s", cnt, buffer);
	}
};