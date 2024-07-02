#pragma once

#include "le_Base.hpp"

class le_AND : protected le_Base<bool>
{
private:
	le_AND(uint16_t nInputs);
	
	void Update(float timeStep);
	
public:
	void Connect(le_Base<bool>* e, uint16_t outputSlot, uint16_t inputSlot);
	
private:
	friend class le_Engine;
};