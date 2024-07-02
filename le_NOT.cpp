#include "le_NOT.hpp"

le_NOT::le_NOT() : le_Base<bool>(1, 1)
{
	// Do nothing...
}

void le_NOT::Update(float timeStep)
{
	// Get input value
	le_Base<bool>* e = (le_Base<bool>*)this->_inputs[0];
	if (e != nullptr)
	{
		bool inputValue = e->GetValue(this->_outputSlots[0]);

		// Set next value
		this->SetValue(0, !inputValue);
	}
}

void le_NOT::Connect(le_Base<bool>* e, uint16_t outputSlot)
{
	// Use default connection function
	le_Base::Connect(e, outputSlot, this, 0);
}
