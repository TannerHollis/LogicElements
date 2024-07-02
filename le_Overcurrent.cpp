#include "le_Overcurrent.hpp"

le_Overcurrent::le_Overcurrent_Curve le_Overcurrent::ParseCurveType(std::string& curve)
{
	if (curve == "C1") return le_Overcurrent_Curve::LE_C1;
	if (curve == "C2") return le_Overcurrent_Curve::LE_C2;
	if (curve == "C3") return le_Overcurrent_Curve::LE_C3;
	if (curve == "C4") return le_Overcurrent_Curve::LE_C4;
	if (curve == "C5") return le_Overcurrent_Curve::LE_C5;
	if (curve == "U1") return le_Overcurrent_Curve::LE_U1;
	if (curve == "U2") return le_Overcurrent_Curve::LE_U2;
	if (curve == "U3") return le_Overcurrent_Curve::LE_U3;
	if (curve == "U4") return le_Overcurrent_Curve::LE_U4;
	if (curve == "U5") return le_Overcurrent_Curve::LE_U5;
	if (curve == "DT") return le_Overcurrent_Curve::LE_DT;

	return le_Overcurrent_Curve::LE_INVALID;
}

void le_Overcurrent::SetCurveParameters(le_Overcurrent_Curve curve, float* parameters)
{
	switch (this->curve)
	{
	case le_Overcurrent_Curve::LE_C1:
		parameters[0] = 0.0f; // Adder inside 
		parameters[1] = 0.14f; // Trip numerator
		parameters[2] = 0.02f; // Trip denominator power
		parameters[4] = 13.5f; // Reset numerator
		parameters[5] = 2.0f; // Reset denominator power
		break;

	case le_Overcurrent_Curve::LE_C2:
		parameters[0] = 0.0f; // Adder inside 
		parameters[1] = 13.5f; // Trip numerator
		parameters[2] = 2.0f; // Trip denominator power
		parameters[4] = 47.3f; // Reset numerator
		parameters[5] = 2.0f; // Reset denominator power
		break;

	case le_Overcurrent_Curve::LE_C3:
		parameters[0] = 0.0f; // Adder inside 
		parameters[1] = 80.0f; // Trip numerator
		parameters[2] = 1.0f; // Trip denominator power
		parameters[4] = 80.0f; // Reset numerator
		parameters[5] = 2.0f; // Reset denominator power
		break;

	case le_Overcurrent_Curve::LE_C4:
		parameters[0] = 0.0f; // Adder inside 
		parameters[1] = 120.0f; // Trip numerator
		parameters[2] = 1.0f; // Trip denominator power
		parameters[4] = 120.0f; // Reset numerator
		parameters[5] = 1.0f; // Reset denominator power
		break;

	case le_Overcurrent_Curve::LE_C5:
		parameters[0] = 0.0f; // Adder inside 
		parameters[1] = 0.05f; // Trip numerator
		parameters[2] = 0.04f; // Trip denominator power
		parameters[4] = 4.85f; // Reset numerator
		parameters[5] = 2.0f; // Reset denominator power
		break;

	case le_Overcurrent_Curve::LE_U1:
		parameters[0] = 0.0226f; // Adder inside 
		parameters[1] = 0.0104f; // Trip numerator
		parameters[2] = 0.02f; // Trip denominator power
		parameters[4] = 1.08f; // Reset numerator
		parameters[5] = 2.0f; // Reset denominator power
		break;

	case le_Overcurrent_Curve::LE_U2:
		parameters[0] = 0.180f; // Adder inside 
		parameters[1] = 5.95f; // Trip numerator
		parameters[2] = 2.0f; // Trip denominator power
		parameters[4] = 5.95f; // Reset numerator
		parameters[5] = 2.0f; // Reset denominator power
		break;

	case le_Overcurrent_Curve::LE_U3:
		parameters[0] = 0.0963f; // Adder inside 
		parameters[1] = 3.88f; // Trip numerator
		parameters[2] = 2.0f; // Trip denominator power
		parameters[4] = 3.88f; // Reset numerator
		parameters[5] = 2.0f; // Reset denominator power
		break;

	case le_Overcurrent_Curve::LE_U4:
		parameters[0] = 0.0352f; // Adder inside 
		parameters[1] = 5.67f; // Trip numerator
		parameters[2] = 2.0f; // Trip denominator power
		parameters[4] = 5.67f; // Reset numerator
		parameters[5] = 2.0f; // Reset denominator power
		break;

	case le_Overcurrent_Curve::LE_U5:
		parameters[0] = 0.00262f; // Adder inside 
		parameters[1] = 0.00342f; // Trip numerator
		parameters[2] = 0.02f; // Trip denominator power
		parameters[4] = 0.323f; // Reset numerator
		parameters[5] = 2.0f; // Reset denominator power
		break;

	case le_Overcurrent_Curve::LE_DT:
		parameters[0] = 0.0f; // Adder inside 
		parameters[1] = 0.0f; // Trip numerator
		parameters[2] = 1.0f; // Trip denominator power
		parameters[4] = 0.0f; // Reset numerator
		parameters[5] = 1.0f; // Reset denominator power
		break;

	default:
		parameters[0] = 0.0f; // Adder inside 
		parameters[1] = 0.0f; // Trip numerator
		parameters[2] = 1.0f; // Trip denominator power
		parameters[4] = 0.0f; // Reset numerator
		parameters[5] = 1.0f; // Reset denominator power
		break;
	}
}

le_Overcurrent::le_Overcurrent(std::string curve, float pickup, float timeDial, float timeAdder, bool emReset) : 
	le_Base<bool>(1, 1)
{
	// Set extrinsic variables
	this->curve = ParseCurveType(curve);
	this->fPickup = pickup;
	this->fTimeDial = timeDial;
	this->fTimeAdder = timeAdder;
	this->bEMReset = emReset;

	// Set intrinsic variables
	SetCurveParameters(this->curve, this->_fParameters);
	this->fPercent = 0.0f;
}

void le_Overcurrent::Update(float timeStep)
{
	le_Base<float>* e = (le_Base<float>*)this->_inputs[0];
	if (e != nullptr)
	{
		// Calculate multiple
		float m = e->GetValue(0) / this->fPickup;

		// Get time dial
		float td = this->fTimeDial;

		// Get parameters
		float* ps = this->_fParameters;

		// Calculate trip/reset time
		if (m > 1.0f)
		{
			float tripTime = this->fTimeAdder + td * (ps[0] + (ps[1]) / (powf(m, ps[2]) - 1.0f));
			this->fPercent += timeStep / tripTime; // Increment dial spin percentage
		}
		else if (m <= 1.0f && this->bEMReset)
		{
			if (m < 1.0f)
			{
				float tripTime = td * (ps[3]) / (1.0f - powf(m, ps[4]));
				this->fPercent -= timeStep / tripTime; // Decrement dial spin percentage
			}
		}
		else
			this->fPercent = 0.0f;

		// Clamp percentage
		if (this->fPercent > 100.0f)
			this->fPercent = 100.0f;
		if (this->fPercent < 0.0f)
			this->fPercent = 0.0f;

		// Set the output of the timer
		this->SetValue(0, this->fPercent == 100.0f);
	}
}

void le_Overcurrent::SetInput(le_Base<float>* e)
{
	// Link input value
	this->_inputs[0] = e;
}
