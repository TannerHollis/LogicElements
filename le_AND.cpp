#include "le_AND.hpp"

le_AND::le_AND(uint16_t nInputs) : le_Base<bool>(nInputs, 1)
{
	// Do nothing...
}

void le_AND::Update(float timeStep)
{
	// Set default to true
	bool nextValue = true;

	// Iterate through all input values and apply inversion if necessary
	for (uint16_t i = 0; i < this->nInputs; i++)
	{
		le_Base<bool>* e = (le_Base<bool>*)(this->_inputs[i]);
		if (e != nullptr)
		{
			bool inputValue = e->GetValue(this->_outputSlots[i]);
			nextValue &= inputValue;
		}
	}

	// Set next value
	this->SetValue(0, nextValue);
}

void le_AND::Connect(le_Base<bool>* e, uint16_t outputSlot, uint16_t inputSlot)
{
	// Use default connection function
	le_Base::Connect(e, outputSlot, this, inputSlot);
}
