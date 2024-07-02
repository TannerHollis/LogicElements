#include "le_Counter.hpp"

le_Counter::le_Counter(uint16_t countFinal) : le_Base<bool>(2, 1)
{
	// Set extrinsic variables
	this->uCountFinal = countFinal;

	// Set intrinsic variables
	this->uCount = 0;
	this->_inputStates[0] = false;
	this->_inputStates[1] = false;
}

void le_Counter::Update(float timeStep)
{
	// Get input component
	le_Base<bool>* cu = (le_Base<bool>*)this->_inputs[0];
	le_Base<bool>* reset = (le_Base<bool>*)this->_inputs[1];

	// If inputs are invalid, skip
	if (cu == nullptr || reset == nullptr)
		return;

	// Update input states for rising trigger
	this->_inputStates[1] = this->_inputStates[0];
	this->_inputStates[0] = cu->GetValue(this->_outputSlots[0]);

	// Detect rising trigger
	bool rtrig = this->_inputStates[0] && !this->_inputStates[1];

	if (reset->GetValue(this->_outputSlots[1]))
		this->uCount = 0;
	else if (rtrig)
		this->uCount += 1;

	// Set next state
	this->SetValue(0, this->uCount >= this->uCountFinal);
}

void le_Counter::SetInput_CountUp(le_Base<bool>* e, uint16_t outputSlot)
{
	// Use default connection function
	le_Base::Connect(e, outputSlot, this, 0);
}

void le_Counter::SetInput_Reset(le_Base<bool>* e, uint16_t outputSlot)
{
	// Use default connection function
	le_Base::Connect(e, outputSlot, this, 1);
}
