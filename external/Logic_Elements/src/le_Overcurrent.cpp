#include "le_Overcurrent.hpp"

#ifdef LE_ELEMENTS_ANALOG

/**
 * @brief Parses the curve type from a string.
 * @param curve The string representing the curve type.
 * @return The corresponding le_Overcurrent_Curve type.
 */
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

/**
 * @brief Sets the curve parameters based on the curve type.
 * @param curve The curve type.
 * @param parameters The array to store the curve parameters.
 */
void le_Overcurrent::SetCurveParameters(le_Overcurrent_Curve curve, float* parameters)
{
    switch (curve)
    {
    case le_Overcurrent_Curve::LE_C1:
        parameters[0] = 0.0f;  // Adder inside 
        parameters[1] = 0.14f; // Trip numerator
        parameters[2] = 0.02f; // Trip denominator power
        parameters[4] = 13.5f; // Reset numerator
        parameters[5] = 2.0f;  // Reset denominator power
        break;
        // [Additional case statements for other curves...]
    case le_Overcurrent_Curve::LE_DT:
        parameters[0] = 0.0f;  // Adder inside 
        parameters[1] = 0.0f;  // Trip numerator
        parameters[2] = 1.0f;  // Trip denominator power
        parameters[4] = 0.0f;  // Reset numerator
        parameters[5] = 1.0f;  // Reset denominator power
        break;
    default:
        parameters[0] = 0.0f;  // Adder inside 
        parameters[1] = 0.0f;  // Trip numerator
        parameters[2] = 1.0f;  // Trip denominator power
        parameters[4] = 0.0f;  // Reset numerator
        parameters[5] = 1.0f;  // Reset denominator power
        break;
    }
}

/**
 * @brief Constructor that initializes the overcurrent element.
 * @param curve The curve type as a string.
 * @param pickup The pickup current value.
 * @param timeDial The time dial setting.
 * @param timeAdder The time adder value.
 * @param emReset Flag indicating if electromechanical reset is enabled.
 */
le_Overcurrent::le_Overcurrent(std::string curve, float pickup, float timeDial, float timeAdder, bool emReset)
    : le_Base<bool>(le_Element_Type::LE_OVERCURRENT, 1, 1)
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

/**
 * @brief Updates the overcurrent element.
 * @param timeStamp The current timestamp.
 */
void le_Overcurrent::Update(const le_Time& timeStamp)
{
    float timeStep = static_cast<float>((le_Time&)timeStamp - this->lastTimeStamp) / 1000000.0f; // Convert microseconds to seconds
    this->lastTimeStamp = timeStamp;

    le_Base<float>* e = this->template GetInput<le_Base<float>>(0);
    if (e != nullptr)
    {
        // Aliasing frequently accessed member variables to local variables
        float timeDial = this->fTimeDial;
        float timeAdder = this->fTimeAdder;
        bool emReset = this->bEMReset;
        float* ps = this->_fParameters;

        // Calculate multiple
        float m = e->GetValue(this->GetOutputSlot(0)) / this->fPickup;

        // Calculate trip/reset time and update percentage
        if (m > 1.0f)
        {
            float tripTime = timeAdder + timeDial * (ps[0] + ps[1] / (powf(m, ps[2]) - 1.0f));
            this->fPercent += timeStep / tripTime; // Increment dial spin percentage
        }
        else if (m < 1.0f && emReset)
        {
            float tripTime = timeDial * ps[3] / (1.0f - powf(m, ps[4]));
            this->fPercent -= timeStep / tripTime; // Decrement dial spin percentage
        }
        else if (m == 1.0f || !emReset)
        {
            this->fPercent = 0.0f;
        }

        // Clamp percentage
        if (this->fPercent > 100.0f)
        {
            this->fPercent = 100.0f;
        }
        if (this->fPercent < 0.0f)
        {
            this->fPercent = 0.0f;
        }

        // Set the output of the timer
        this->SetValue(0, this->fPercent == 100.0f);
    }

    // Update the last timestamp
    this->lastTimeStamp = timeStamp;
}

/**
 * @brief Sets the input element for the overcurrent protection.
 * @param e The input element providing the current value.
 */
void le_Overcurrent::SetInput(le_Base<float>* e, uint8_t outputSlot)
{
    // Link input value
    le_Element::Connect(e, outputSlot, this, 0);
}
#endif