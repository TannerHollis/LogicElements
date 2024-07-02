#include "le_RTrig.hpp"

le_RTrig::le_RTrig() : le_Base<bool>(1, 1)
{
	// Set intrinsic variables
	this->_inputStates[0] = false;
	this->_inputStates[1] = false;
}

void le_RTrig::Update(float timeStep)
{
	// Get input value
	le_Base<bool>* e = (le_Base<bool>*)this->_inputs[0];
	if (e != nullptr)
	{
		// Get input value
		bool inputValue = e->GetValue(this->_outputSlots[0]);

		// Shift states
		this->_inputStates[1] = this->_inputStates[0];
		this->_inputStates[0] = inputValue;

		// Set next value
		this->SetValue(0, this->_inputStates[0] && !this->_inputStates[1]);
	}
}

void le_RTrig::Connect(le_Base<bool>* e, uint16_t outputSlot)
{
	// Use default connection function
	le_Base::Connect(e, outputSlot, this, 0);
}
