#pragma once

#include "le_Base.hpp"

class le_Counter : protected le_Base<bool>
{
private:
	le_Counter(uint16_t countFinal);

	void Update(float timeStep);
	
public:
	void SetInput_CountUp(le_Base<bool>* e, uint16_t outputSlot);
	
	void SetInput_Reset(le_Base<bool>* e, uint16_t outputSlot);
	
private:
	uint16_t uCountFinal;
	uint16_t uCount;
	
	bool _inputStates[2];

	friend class le_Engine;
};