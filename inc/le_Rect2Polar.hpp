#pragma once

#include "le_Base.hpp"

#include <cmath>

/**
 * @brief Class representing a basic conversion from rectangular to polar phasor domain.
 *        Inherits from le_Base with float type.
 */
class le_Rect2Polar : protected le_Base<float>
{
LE_ELEMENT_ACCESS_MOD:
    le_Rect2Polar() : le_Base<float>(2, 2) {}

    void Update(float timeStep)
    {
        le_Base<float>* eReal = this->_inputs[0];
        le_Base<float>* eImag = this->_inputs[1];
        
        // Check null reference
        if(eReal != nullptr || eImag != nullptr)
        {
            float real = eReal->GetValue(this->_outputSlots[0]);
            float imag = eImag->GetValue(this->_outputSlots[1]);

            float mag = sqrtf(real * real + imag * imag);
            float angle = atan2f(imag, real) * 180.0f / M_PI;

            this->SetValue(0, mag);
            this->SetValue(1, angle);
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
};