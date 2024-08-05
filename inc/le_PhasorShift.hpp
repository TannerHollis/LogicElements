#pragma once

#include "le_Base.hpp"

#include <cmath>

/**
 * @brief Class representing a basic shift, using rectangular input.
 *        Inherits from le_Base with float type.
 */
class le_PhasorShift : protected le_Base<float>
{
LE_ELEMENT_ACCESS_MOD:
    le_PhasorShift(float shiftMagnitude, float shiftAngleClockwise) : le_Base<float>(2, 2) 
    {
        // Set explicit variables
        this->shiftMag = shiftMagnitude;
        this->shiftAngle = shiftAngleClockwise;

        // Set implicit variables
        this->unitShiftReal = shiftMagnitude * cosf(shiftAngleClockwise / 180.0f * M_PI);
        this->unitShiftImag = shiftMagnitude * -sinf(shiftAngleClockwise / 180.0f * M_PI);
    }

    void Update(float timeStep)
    {
        le_Base<float>* eReal = this->_inputs[0];
        le_Base<float>* eImag = this->_inputs[1];
        
        // Check null reference
        if(eReal != nullptr || eImag != nullptr)
        {
            float real = eReal->GetValue(this->_outputSlots[0]);
            float imag = eImag->GetValue(this->_outputSlots[1]);

            float newReal = real * unitShiftReal + imag * unitShiftImag;
            float newImag = imag * unitShiftReal - real * unitShiftImag;

            this->SetValue(0, newReal);
            this->SetValue(1, newImag);
        }
    }

public:
    void SetInput_Real(le_Base<float>* e, uint8_t outputSlot)
    {
        le_Element::Connect(e, outputSlot, this, 0);
    }

    void SetInput_Imag(le_Base<float>* e, uint8_t outputSlot)
    {
        le_Element::Connect(e, outputSlot, this, 1);
    }

private:
    float shiftMag;
    float shiftAngle;
    float unitShiftReal;
    float unitShiftImag;
};