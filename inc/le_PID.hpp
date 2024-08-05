#pragma once

#include "le_Base.hpp"

// Include standard C++ libraries
#include <cmath>
#include <string>

/**
 * @brief Class representing a PID Controller.
 *        Inherits from le_Base with float type.
 */
class le_PID : protected le_Base<float>
{
LE_ELEMENT_ACCESS_MOD:
    le_PID(float p, float i, float d, float outputMin, float outputMax, uint8_t derivativeTerms = 3) : le_Base<float>(2, 1)
    {
        // Set extrinsic variables
        this->fP = p;
        this->fI = i;
        this->fD = d;
        this->fOutputMin = outputMin;
        this->fOutputMax = outputMax;
        this->uDerivativeTerms = derivativeTerms;

        // Set 
        this->_dBufferIn = new float[derivativeTerms];
        this->_dBufferOut = new float[derivativeTerms];
        this->uDerivativeWrite = this->uDerivativeTerms - 1;
        this->fDerivativeCoefficient = 1.0f / (float)derivativeTerms;
        this->fIntegral = 0.0f;
    }

    ~le_PID()
    {
        delete[] this->_dBufferIn;
        delete[] this->_dBufferOut;
    }

    void Update(float timeStep)
    {
        // Get input components for count-up and reset
        le_Base<float>* setpoint = (le_Base<float>*)this->_inputs[0];
        le_Base<float>* feedback = (le_Base<float>*)this->_inputs[1];

        // If inputs are invalid, skip update
        if (setpoint == nullptr || feedback == nullptr)
            return;

        // Calculate error
        float error = setpoint->GetValue(this->_inputSlots[0]) - setpoint->GetValue(this->_inputSlots[1]);

        // Ignore derivative if set to zero, instead a PI controller.
        if(this->fD == 0.0f)
            // Get PI values
            float output = this->GetProportional(error) + 
                this->GetIntegral(error, timeStep);
        else
            // Get PID values
            float output = this->GetProportional(error) + 
                this->GetIntegral(error, timeStep) + 
                this->GetDerivative(error, timeStep);

        // Clamp output
        output = this->Clamp(output, outputMin, outputMax);

        // Update output
        this->SetValue(0, output);
    }

public:
    void SetInput_Setpoint(le_Base<float>* e, uint16_t outputSlot)
    {
        le_Element::Connect(e, outputSlot, this, 0);
    }

    void SetInput_Feedback(le_Base<float>* e, uint16_t outputSlot)
    {
        le_Element::Connect(e, outputSlot, this, 1);
    }

private:
    float GetProportional(float error)
    {
        return this->Clamp(this->fP * error, outputMin, outputMax);
    }

    float GetIntegral(float error, float timeStep)
    {
        this->fIntegral += this->fI * error * timeStep;
        this->fIntegral = this->Clamp(this->fIntegral, outputMin, outputMax);
        return this->fIntegral;
    }

    float GetDerivative(float error, float timeStep)
    {
        // Store latest derivative term
        this->dBufferIn[this->uDerivativeWrite] = error;

        // Apply basic averaging filter
        this->dBufferOut[this->uDerivativeWrite] = 0.0f;
        for(uint8_t i = 0; i < this->uDerivativeTerms; i++)
        {
            this->dBufferOut[this->uDerivativeWrite] += 
                this->dBufferIn[(this->uDerivativeWrite + i) % this->uDerivativeTerms] * this->fDerivativeCoefficient;
        }

        // Calculate derivative
        float d = (this->dBufferOut[this->uDerivativeWrite] - this->dBufferOut[(this->uDerivativeWrite - 1 + this->uDerivativeTerms) % this->uDerivativeTerms]);

        // Increment write pointer
        this->uDerivativeWrite = (this->uDerivativeTerms + 1) % this->uDerivativeTerms;
        return this->Clamp(d * this->fD / timeStep, outputMin, outputMax);
    }

    inline float Clamp(float value, float min, float max)
    {
        return (value > max) ? max : (value < min) ? min : value;
    }

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
};

/**
 * @brief Class representing a PI Controller.
 *        Inherits from le_PID.
 */
class le_PI : public le_PID
{
protected:
    le_PI(float p, float i) : le_PID(p, i) {}
}