#pragma once

#include "le_Base.hpp"

class le_FTrig : protected le_Base<bool>
{
private:
	le_FTrig();
	
	void Update(float timeStep);
	
public:
	void Connect(le_Base<bool>* e, uint16_t outputSlot);
	
private:
	bool _inputStates[2];
	
	friend class le_Engine;
};