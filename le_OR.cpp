#include "le_OR.hpp"

le_OR::le_OR(uint16_t nInputs) : le_Base(nInputs, 1)
{
	// Do nothing...
}

void le_OR::Update(float timeStep)
{
	// Set default to false
	bool nextValue = false;

	// Iterate through all input values and apply inversion if necessary
	for (uint16_t i = 0; i < this->nInputs; i++)
	{
		le_Base<bool>* e = (le_Base<bool>*)this->_inputs[i];
		if (e != nullptr)
		{
			bool inputValue = e->GetValue(this->_outputSlots[i]);
			nextValue |= inputValue;
		}
	}

	// Set next value
	this->SetValue(0, nextValue);
}

void le_OR::Connect(le_Base* e, uint16_t outputSlot, uint16_t inputSlot)
{
	// Use default connection function
	le_Base::Connect(e, outputSlot, this, inputSlot);
}
