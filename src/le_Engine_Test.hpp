#include "le_Engine.hpp"

// Test engine to test elements and functions
class le_Engine_Test : public le_Engine
{
public:
	le_Engine_Test() : le_Engine("Test Engine")
	{
		// Add inputs
		le_Element_TypeDef in0("IN0", le_Element_Type::LE_NODE_DIGITAL);
		le_Element_TypeDef in1("IN1", le_Element_Type::LE_NODE_DIGITAL);
		
		// Add logic
		le_Element_TypeDef or0("OR0", le_Element_Type::LE_OR);
		or0.args[0].uArg = 2;

		le_Element_TypeDef and0("AND0", le_Element_Type::LE_AND);
		and0.args[0].uArg = 2;

		// Add outputs
		le_Element_TypeDef out0("OUT0", le_Element_Type::LE_NODE_DIGITAL);
		le_Element_TypeDef out1("OUT1", le_Element_Type::LE_NODE_DIGITAL);

		// Add elements
		this->AddElement(&in0);
		this->AddElement(&in1);
		this->AddElement(&or0);
		this->AddElement(&and0);
		this->AddElement(&out0);
		this->AddElement(&out1);

		// Create net connections
		le_Element_Net_TypeDef n0("IN0", 0);
		n0.AddInput("OR0", 0);
		n0.AddInput("AND0", 0);

		le_Element_Net_TypeDef n1("IN1", 0);
		n1.AddInput("OR0", 1);
		n1.AddInput("AND0", 1);

		le_Element_Net_TypeDef n2("OR0", 0);
		n2.AddInput("OUT0", 0);

		le_Element_Net_TypeDef n3("AND0", 0);
		n3.AddInput("OUT1", 0);

		// Add nets
		this->AddNet(&n0);
		this->AddNet(&n1);
		this->AddNet(&n2);
		this->AddNet(&n3);

		// Update once to initialize the engine
		this->Update(0.0001f);

		// Print resulting engine info
		char buffer[1024];
		this->GetInfo(buffer, 1024);
		printf("%s", buffer);
	}
};