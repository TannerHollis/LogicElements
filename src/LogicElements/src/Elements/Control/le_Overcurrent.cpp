#include "Elements/Control/le_Overcurrent.hpp"

#ifdef LE_ELEMENTS_ANALOG

namespace LogicElements {

/**
 * @brief Parses the curve type from a string.
 * @param curve The string representing the curve type.
 * @return The corresponding OvercurrentCurve type.
 */
Overcurrent::OvercurrentCurve Overcurrent::ParseCurveType(std::string& curve)
{
    if (curve == "C1") return OvercurrentCurve::LE_C1;
    if (curve == "C2") return OvercurrentCurve::LE_C2;
    if (curve == "C3") return OvercurrentCurve::LE_C3;
    if (curve == "C4") return OvercurrentCurve::LE_C4;
    if (curve == "C5") return OvercurrentCurve::LE_C5;
    if (curve == "U1") return OvercurrentCurve::LE_U1;
    if (curve == "U2") return OvercurrentCurve::LE_U2;
    if (curve == "U3") return OvercurrentCurve::LE_U3;
    if (curve == "U4") return OvercurrentCurve::LE_U4;
    if (curve == "U5") return OvercurrentCurve::LE_U5;
    if (curve == "DT") return OvercurrentCurve::LE_DT;

    return OvercurrentCurve::LE_INVALID;
}

/**
 * @brief Sets the curve parameters based on the curve type.
 * @param curve The curve type.
 * @param parameters The array to store the curve parameters.
 */
void Overcurrent::SetCurveParameters(OvercurrentCurve curve, float* parameters)
{
    switch (curve)
    {
    case OvercurrentCurve::LE_C1:
        parameters[0] = 0.0f;  parameters[1] = 0.14f; parameters[2] = 0.02f;
        parameters[3] = 13.5f; parameters[4] = 2.0f;
        break;
    case OvercurrentCurve::LE_C2:
        parameters[0] = 0.0f;  parameters[1] = 13.5f; parameters[2] = 2.0f;
        parameters[3] = 47.3f; parameters[4] = 2.0f;
        break;
    case OvercurrentCurve::LE_C3:
        parameters[0] = 0.0f;  parameters[1] = 80.0f; parameters[2] = 2.0f;
        parameters[3] = 80.0f; parameters[4] = 2.0f;
        break;
    case OvercurrentCurve::LE_C4:
        parameters[0] = 0.0f;  parameters[1] = 120.0f; parameters[2] = 2.0f;
        parameters[3] = 120.0f; parameters[4] = 2.0f;
        break;
    case OvercurrentCurve::LE_C5:
        parameters[0] = 0.0f;  parameters[1] = 0.0515f; parameters[2] = 0.02f;
        parameters[3] = 4.85f; parameters[4] = 2.0f;
        break;
    case OvercurrentCurve::LE_U1:
        parameters[0] = 0.0f;  parameters[1] = 0.0104f; parameters[2] = 0.02f;
        parameters[3] = 2.261f; parameters[4] = 2.0f;
        break;
    case OvercurrentCurve::LE_U2:
        parameters[0] = 0.0f;  parameters[1] = 5.95f; parameters[2] = 2.0f;
        parameters[3] = 18.00f; parameters[4] = 2.0f;
        break;
    case OvercurrentCurve::LE_U3:
        parameters[0] = 0.0f;  parameters[1] = 3.88f; parameters[2] = 2.0f;
        parameters[3] = 21.60f; parameters[4] = 2.0f;
        break;
    case OvercurrentCurve::LE_U4:
        parameters[0] = 0.0f;  parameters[1] = 5.67f; parameters[2] = 2.0f;
        parameters[3] = 29.10f; parameters[4] = 2.0f;
        break;
    case OvercurrentCurve::LE_U5:
        parameters[0] = 0.0f;  parameters[1] = 0.00342f; parameters[2] = 0.02f;
        parameters[3] = 0.323f; parameters[4] = 2.0f;
        break;
    case OvercurrentCurve::LE_DT:
        parameters[0] = 0.0f;  parameters[1] = 0.0f; parameters[2] = 1.0f;
        parameters[3] = 0.0f; parameters[4] = 1.0f;
        break;
    default:
        parameters[0] = 0.0f;  parameters[1] = 0.0f; parameters[2] = 1.0f;
        parameters[3] = 0.0f; parameters[4] = 1.0f;
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
Overcurrent::Overcurrent(std::string curve, float pickup, float timeDial, float timeAdder, bool emReset)
    : Element(ElementType::Overcurrent)
{
    // HETEROGENEOUS PORTS: float input, bool output!
    pCurrent = AddInputPort<float>("current");
    pTrip = AddOutputPort<bool>("trip");

    // Set extrinsic variables
    this->curve = ParseCurveType(curve);
    this->fPickup = pickup;
    this->fTimeDial = timeDial;
    this->fTimeAdder = timeAdder;
    this->bEMReset = emReset;

    // Set intrinsic variables
    SetCurveParameters(this->curve, this->_fParameters);
    this->fPercent = 0.0f;

    // Initialize timestamp
    this->lastTimeStamp = Time();
}

/**
 * @brief Updates the overcurrent element.
 * @param timeStamp The current timestamp.
 */
void Overcurrent::Update(const Time& timeStamp)
{
    // Calculate timeStep (int64_t to float conversion is intentional - microseconds to seconds)
    float timeStep = static_cast<float>((Time&)timeStamp - this->lastTimeStamp) / 1000000.0f;
    this->lastTimeStamp = timeStamp;

    if (pCurrent->IsConnected())
    {
        // Aliasing frequently accessed member variables to local variables
        float timeDial = this->fTimeDial;
        float timeAdder = this->fTimeAdder;
        bool emReset = this->bEMReset;
        float* ps = this->_fParameters;

        // Calculate multiple
        float m = pCurrent->GetValue() / this->fPickup;

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
            this->fPercent = 100.0f;
        if (this->fPercent < 0.0f)
            this->fPercent = 0.0f;

        // Set the trip output
        pTrip->SetValue(this->fPercent == 100.0f);
    }
}

/**
 * @brief Sets the input element for the overcurrent protection.
 * @param sourceElement The input element providing the current value.
 * @param sourcePortName The name of the output port on the source element.
 */
void Overcurrent::SetInput(Element* sourceElement, const std::string& sourcePortName)
{
    Element::Connect(sourceElement, sourcePortName, this, "current");
}

/**
 * @brief Gets the trip output (bool).
 * @return True if tripped, false otherwise.
 */
bool Overcurrent::GetTrip() const
{
    return pTrip->GetValue();
}

#endif

} // namespace LogicElements
