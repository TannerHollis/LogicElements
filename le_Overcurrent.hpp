#pragma once

#include "le_Base.hpp"

// Include STD C++ libs
#include <cmath>
#include <string>

class le_Overcurrent : protected le_Base<bool>
{
private:
	typedef enum le_Overcurrent_Curve
	{
		LE_C1 = 0,
		LE_C2 = 1,
		LE_C3 = 2,
		LE_C4 = 3,
		LE_C5 = 4,
		LE_U1 = 10,
		LE_U2 = 11,
		LE_U3 = 12,
		LE_U4 = 13,
		LE_U5 = 14,
		LE_DT = 20,
		LE_INVALID = -1
	} le_Overcurrent_Curve;
	
	le_Overcurrent_Curve ParseCurveType(std::string& curve);
	
	void SetCurveParameters(le_Overcurrent_Curve curve, float* parameters);
	
	le_Overcurrent(std::string curve, float pickup, float timeDial, float timeAdder, bool emReset);
	
	void Update(float timeStep);
	
public:
	void SetInput(le_Base<float>* e);
	
private:
	le_Overcurrent_Curve curve;
	float fPickup;
	float fTimeDial;
	float fTimeAdder;
	bool bEMReset;
	
	float fPercent;
	float _fParameters[5];

	friend class le_Engine;
};