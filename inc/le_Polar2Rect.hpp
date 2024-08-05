#pragma once

#include "le_Base.hpp"

#include <cmath>

/**
 * @brief Class representing a basic conversion from polar to rectangular phasor domain.
 *        Inherits from le_Base with float type.
 */
class le_Polar2Rect : protected le_Base<float>
{
LE_ELEMENT_ACCESS_MOD:
    le_Polar2Rect() : le_Base<float>(2, 2) {}

    void Update(float timeStep)
    {
        le_Base<float>* eMag = this->_inputs[0];
        le_Base<float>* eAngle = this->_inputs[1];
        
        // Check null reference
        if(eMag != nullptr || eAngle != nullptr)
        {
            float mag = eMag->GetValue(this->_outputSlots[0]);
            float angle = eAngle->GetValue(this->_outputSlots[1]) / 180.0f * M_PI;

            float real = mag * cosf(angle);
            float imag = mag * sinf(angle);

            this->SetValue(0, real);
            this->SetValue(1, imag);
        }
    }

public:
    void SetInput_Mag(le_Base<float>* e, uint8_t outputSlot)
    {
        le_Element::Connect(e, outputSlot, this, 0);
    }

    void SetInput_Angle(le_Base<float>* e, uint8_t outputSlot)
    {
        le_Element::Connect(e, outputSlot, this, 1);
    }
};