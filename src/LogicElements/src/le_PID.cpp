#include "le_PID.hpp"

#ifdef LE_ELEMENTS_ANALOG
#ifdef LE_ELEMENTS_PID

namespace LogicElements {

/**
 * @brief Constructs a PID controller with given parameters.
 * @param p Proportional gain.
 * @param i Integral gain.
 * @param d Derivative gain.
 * @param outputMin Minimum output value.
 * @param outputMax Maximum output value.
 * @param derivativeTerms Number of terms for derivative calculation.
 */
PID::PID(float p, float i, float d, float outputMin, float outputMax, uint8_t derivativeTerms)
    : Element(ElementType::PID), fP(p), fI(i), fD(d), fOutputMin(outputMin), fOutputMax(outputMax), uDerivativeTerms(derivativeTerms)
{
    // Create ports
    pSetpoint = AddInputPort<float>("setpoint");
    pFeedback = AddInputPort<float>("feedback");
    pOutput = AddOutputPort<float>("output");

    // Allocate derivative buffers
    this->_dBufferIn = new float[derivativeTerms];
    this->_dBufferOut = new float[derivativeTerms];
    this->uDerivativeWrite = this->uDerivativeTerms - 1;
    this->fDerivativeCoefficient = 1.0f / (float)derivativeTerms;
    this->fIntegral = 0.0f;

    // Initialize last timestamp
    this->lastTimeStamp = Time();
}

/**
 * @brief Destructor to clean up allocated memory.
 */
PID::~PID()
{
    delete[] this->_dBufferIn;
    delete[] this->_dBufferOut;
}

/**
 * @brief Updates the PID controller.
 * @param timeStamp The current timestamp.
 */
void PID::Update(const Time& timeStamp)
{
    // Calculate timeStep (int64_t to float conversion - microseconds to seconds)
    float timeStep = static_cast<float>((Time&)timeStamp - this->lastTimeStamp) / 1000000.0f;
    this->lastTimeStamp = timeStamp;

    // Check if inputs are connected
    if (!pSetpoint->IsConnected() || !pFeedback->IsConnected())
        return;

    // Calculate error
    float error = pSetpoint->GetValue() - pFeedback->GetValue();

    // Ignore derivative if set to zero, instead a PI controller.
    float output;
    if (this->fD == 0.0f)
        // Get PI values
        output = this->GetProportional(error) + this->GetIntegral(error, timeStep);
    else
        // Get PID values
        output = this->GetProportional(error) + this->GetIntegral(error, timeStep) + this->GetDerivative(error, timeStep);

    // Clamp output
    output = this->Clamp(output, this->fOutputMin, this->fOutputMax);

    // Update output
    pOutput->SetValue(output);
}

/**
 * @brief Sets the setpoint input for the PID controller.
 * @param sourceElement Pointer to the element providing the setpoint.
 * @param sourcePortName The name of the output port on the source element.
 */
void PID::SetInput_Setpoint(Element* sourceElement, const std::string& sourcePortName)
{
    Element::Connect(sourceElement, sourcePortName, this, "setpoint");
}

/**
 * @brief Sets the feedback input for the PID controller.
 * @param sourceElement Pointer to the element providing the feedback.
 * @param sourcePortName The name of the output port on the source element.
 */
void PID::SetInput_Feedback(Element* sourceElement, const std::string& sourcePortName)
{
    Element::Connect(sourceElement, sourcePortName, this, "feedback");
}

/**
 * @brief Gets the control output value.
 * @return The control output.
 */
float PID::GetOutput() const
{
    return pOutput->GetValue();
}

/**
 * @brief Gets the proportional component of the PID controller.
 * @param error The current error.
 * @return The proportional component.
 */
float PID::GetProportional(float error) const
{
    return this->fP * error;
}

/**
 * @brief Gets the integral component of the PID controller.
 * @param error The current error.
 * @param timeStep The current timestamp.
 * @return The integral component.
 */
float PID::GetIntegral(float error, float timeStep)
{
    this->fIntegral += error * timeStep;
    return this->fI * this->fIntegral;
}

/**
 * @brief Gets the derivative component of the PID controller.
 * @param error The current error.
 * @param timeStep The current timestamp.
 * @return The derivative component.
 */
float PID::GetDerivative(float error, float timeStep)
{
    // Write to circular buffer
    this->_dBufferIn[this->uDerivativeWrite] = error;
    
    // Calculate derivative
    float derivative = 0.0f;
    for (uint8_t i = 0; i < this->uDerivativeTerms; i++)
    {
        uint8_t index = (this->uDerivativeWrite + i + 1) % this->uDerivativeTerms;
        derivative += (this->_dBufferIn[index] - this->_dBufferOut[index]) * this->fDerivativeCoefficient;
    }
    
    // Update output buffer
    this->_dBufferOut[this->uDerivativeWrite] = error;
    
    // Shift write head
    this->uDerivativeWrite = (this->uDerivativeWrite - 1 + this->uDerivativeTerms) % this->uDerivativeTerms;
    
    return this->fD * derivative / timeStep;
}

/**
 * @brief Clamps a value between a minimum and maximum value.
 * @param value The value to clamp.
 * @param min The minimum value.
 * @param max The maximum value.
 * @return The clamped value.
 */
inline float PID::Clamp(float value, float min, float max)
{
    if (value < min)
        return min;
    if (value > max)
        return max;
    return value;
}

/**
 * @brief Constructs a PI controller with given parameters.
 * @param p Proportional gain.
 * @param i Integral gain.
 * @param outputMin Minimum output value.
 * @param outputMax Maximum output value.
 */
le_PI::le_PI(float p, float i, float outputMin, float outputMax)
    : PID(p, i, 0.0f, outputMin, outputMax, 0)
{
}

/**
 * @brief Destructor to clean up allocated memory.
 */
le_PI::~le_PI()
{
}

#endif // LE_ELEMENTS_PID
#endif // LE_ELEMENTS_ANALOG

} // namespace LogicElements
