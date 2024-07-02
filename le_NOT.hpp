#pragma once

#include "le_Base.hpp"

class le_NOT : protected le_Base<bool>
{
private:
	le_NOT();
	
	void Update(float timeStep);
	
public:
	void Connect(le_Base<bool>* e, uint16_t outputSlot);
	
private:
	friend class le_Engine;
};