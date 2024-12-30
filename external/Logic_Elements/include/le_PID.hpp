#pragma once

#include "le_Base.hpp"

#ifdef LE_ELEMENTS_ANALOG
#ifdef LE_ELEMENTS_PID

#include <cmath>
#include <string>

/**
 * @brief Class representing a PID Controller.
 *        Inherits from le_Base with float type.
 */
class le_PID : protected le_Base<float>
{
LE_ELEMENT_ACCESS_MOD:
    /**
     * @brief Constructs a PID controller with given parameters.
     * @param p Proportional gain.
     * @param i Integral gain.
     * @param d Derivative gain.
     * @param outputMin Minimum output value.
     * @param outputMax Maximum output value.
     * @param derivativeTerms Number of terms for derivative calculation.
     */
    le_PID(float p, float i, float d, float outputMin, float outputMax, uint8_t derivativeTerms = 3);

    /**
     * @brief Destructor to clean up allocated memory.
     */
    ~le_PID();

    /**
     * @brief Updates the PID controller.
     * @param timeStamp The current timestamp.
     */
    void Update(const le_Time& timeStamp);

    /**
     * @brief Sets the setpoint input for the PID controller.
     * @param e Pointer to the element providing the setpoint.
     * @param outputSlot The slot of the output in the providing element.
     */
    void SetInput_Setpoint(le_Base<float>* e, uint8_t outputSlot);

    /**
     * @brief Sets the feedback input for the PID controller.
     * @param e Pointer to the element providing the feedback.
     * @param outputSlot The slot of the output in the providing element.
     */
    void SetInput_Feedback(le_Base<float>* e, uint8_t outputSlot);

private:
    /**
     * @brief Gets the proportional component of the PID controller.
     * @param error The current error.
     * @return The proportional component.
     */
    float GetProportional(float error);

    /**
     * @brief Gets the integral component of the PID controller.
     * @param error The current error.
     * @param timeStep The current timestamp.
     * @return The integral component.
     */
    float GetIntegral(float error, float timeStep);

    /**
     * @brief Gets the derivative component of the PID controller.
     * @param error The current error.
     * @param timeStep The current timestamp.
     * @return The derivative component.
     */
    float GetDerivative(float error, float timeStep);

    /**
     * @brief Clamps a value between a minimum and maximum value.
     * @param value The value to clamp.
     * @param min The minimum value.
     * @param max The maximum value.
     * @return The clamped value.
     */
    inline float Clamp(float value, float min, float max);

    // PID parameters
    float fP, fI, fD;

    // Output clamping
    float fOutputMin, fOutputMax;

    // Integral value
    float fIntegral;

    // Derivative buffer
    uint8_t uDerivativeTerms;
    uint8_t uDerivativeWrite;
    float* _dBufferIn;
    float* _dBufferOut;
    float fDerivativeCoefficient;

    // Last timestamp
    le_Time lastTimeStamp;

    friend class le_Engine;
};

/**
 * @brief Class representing a PI Controller.
 *        Inherits from le_PID.
 */
class le_PI : public le_PID
{
LE_ELEMENT_ACCESS_MOD:
    /**
     * @brief Constructs a PI controller with given parameters.
     * @param p Proportional gain.
     * @param i Integral gain.
     * @param outputMin Minimum output value.
     * @param outputMax Maximum output value.
     */
    le_PI(float p, float i, float outputMin, float outputMax);

    /**
     * @brief Destructor to clean up allocated memory.
     */
    ~le_PI();
};

#endif // LE_ELEMENTS_PID
#endif // LE_ELEMENTS_ANALOG