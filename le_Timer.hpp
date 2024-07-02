#pragma once

#include "le_Base.hpp"

class le_Timer : protected le_Base<bool>
{
private:
	typedef enum le_Timer_State
	{
		TIM_IDLE = 0,
		TIM_PICKUP = 1,
		TIM_DROPOUT = 2
	} le_Timer_State;
	
private:
	le_Timer(float pickup, float dropout);
	
	void Update(float timeStep);
	
public:
	void Connect(le_Base<bool>* e, uint16_t outputSlot);
	
private:
	float fPickup;
	float fDropout;
	
	float fTimer;
	le_Timer_State state;

	friend class le_Engine;
};