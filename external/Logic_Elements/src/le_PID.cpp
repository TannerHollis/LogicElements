#include "le_PID.hpp"

#ifdef LE_ELEMENTS_ANALOG
#ifdef LE_ELEMENTS_PID

/**
 * @brief Constructs a PID controller with given parameters.
 * @param p Proportional gain.
 * @param i Integral gain.
 * @param d Derivative gain.
 * @param outputMin Minimum output value.
 * @param outputMax Maximum output value.
 * @param derivativeTerms Number of terms for derivative calculation.
 */
le_PID::le_PID(float p, float i, float d, float outputMin, float outputMax, uint8_t derivativeTerms)
    : le_Base<float>(le_Element_Type::LE_PID, 2, 1), fP(p), fI(i), fD(d), fOutputMin(outputMin), fOutputMax(outputMax), uDerivativeTerms(derivativeTerms)
{
    // Set extrinsic variables
    this->_dBufferIn = new float[derivativeTerms];
    this->_dBufferOut = new float[derivativeTerms];
    this->uDerivativeWrite = this->uDerivativeTerms - 1;
    this->fDerivativeCoefficient = 1.0f / (float)derivativeTerms;
    this->fIntegral = 0.0f;

    // Initialize last timestamp
    this->lastTimeStamp = le_Time();
}

/**
 * @brief Destructor to clean up allocated memory.
 */
le_PID::~le_PID()
{
    delete[] this->_dBufferIn;
    delete[] this->_dBufferOut;
}

/**
 * @brief Updates the PID controller.
 * @param timeStamp The current timestamp.
 */
void le_PID::Update(const le_Time& timeStamp)
{
    // Calculate timeStep
    float timeStep = static_cast<float>((le_Time&)timeStamp - this->lastTimeStamp) / 1000000.0f;
    this->lastTimeStamp = timeStamp;

    // Get input components for count-up and reset
    le_Base<float>* setpoint = this->template GetInput<le_Base<float>>(0);
    le_Base<float>* feedback = this->template GetInput<le_Base<float>>(1);

    // If inputs are invalid, skip update
    if (setpoint == nullptr || feedback == nullptr)
        return;

    // Calculate error
    float error = setpoint->GetValue(this->GetOutputSlot(0)) - feedback->GetValue(this->GetOutputSlot(1));

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
    this->SetValue(0, output);
}

/**
 * @brief Sets the setpoint input for the PID controller.
 * @param e Pointer to the element providing the setpoint.
 * @param outputSlot The slot of the output in the providing element.
 */
void le_PID::SetInput_Setpoint(le_Base<float>* e, uint8_t outputSlot)
{
    le_Element::Connect(e, outputSlot, this, 0);
}

/**
 * @brief Sets the feedback input for the PID controller.
 * @param e Pointer to the element providing the feedback.
 * @param outputSlot The slot of the output in the providing element.
 */
void le_PID::SetInput_Feedback(le_Base<float>* e, uint8_t outputSlot)
{
    le_Element::Connect(e, outputSlot, this, 1);
}

/**
 * @brief Gets the proportional component of the PID controller.
 * @param error The current error.
 * @return The proportional component.
 */
float le_PID::GetProportional(float error)
{
    return this->Clamp(error, this->fOutputMin, this->fOutputMax);
}

/**
 * @brief Gets the integral component of the PID controller.
 * @param error The current error.
 * @param timeStep The current timestamp.
 * @return The integral component.
 */
float le_PID::GetIntegral(float error, float timeStep)
{
    this->fIntegral += this->fI * error * timeStep;
    this->fIntegral = this->Clamp(this->fIntegral, this->fOutputMin, this->fOutputMax);
    return this->fIntegral;
}

/**
 * @brief Gets the derivative component of the PID controller.
 * @param error The current error.
 * @param timeStep The current timestamp.
 * @return The derivative component.
 */
float le_PID::GetDerivative(float error, float timeStep)
{
    // Store latest derivative term
    this->_dBufferIn[this->uDerivativeWrite] = error;

    // Apply basic averaging filter
    this->_dBufferOut[this->uDerivativeWrite] = 0.0f;
    for (uint8_t i = 0; i < this->uDerivativeTerms; i++)
    {
        this->_dBufferOut[this->uDerivativeWrite] +=
            this->_dBufferIn[(this->uDerivativeWrite + i) % this->uDerivativeTerms] * this->fDerivativeCoefficient;
    }

    // Calculate derivative
    float d = (this->_dBufferOut[this->uDerivativeWrite] - this->_dBufferOut[(this->uDerivativeWrite - 1 + this->uDerivativeTerms) % this->uDerivativeTerms]);

    // Increment write pointer
    this->uDerivativeWrite = (this->uDerivativeWrite + 1) % this->uDerivativeTerms;
    return this->Clamp(d * this->fD / timeStep, this->fOutputMin, this->fOutputMax);
}

/**
 * @brief Clamps a value between a minimum and maximum value.
 * @param value The value to clamp.
 * @param min The minimum value.
 * @param max The maximum value.
 * @return The clamped value.
 */
inline float le_PID::Clamp(float value, float min, float max)
{
    return (value > max) ? max : (value < min) ? min : value;
}

/**
 * @brief Constructs a PI controller with given parameters.
 * @param p Proportional gain.
 * @param i Integral gain.
 * @param outputMin Minimum output value.
 * @param outputMax Maximum output value.
 */
le_PI::le_PI(float p, float i, float outputMin, float outputMax)
    : le_PID(p, i, 0.0f, outputMin, outputMax) {}

/**
 * @brief Destructor to clean up allocated memory.
 */
le_PI::~le_PI()
{
    le_PID::~le_PID();
}

#endif // LE_ELEMENTS_PID
#endif // LE_ELEMENTS_ANALOG