#include "le_Polar2Rect.hpp"
#include <cmath>

/**
 * @brief Constructs a le_Polar2Rect object.
 */
le_Polar2Rect::le_Polar2Rect() : le_Base<float>(2, 2) {}

/**
 * @brief Updates the le_Polar2Rect element.
 * @param timeStep The current timestamp.
 */
void le_Polar2Rect::Update(float timeStep)
{
    UNUSED(timeStep);

    le_Base<float>* eMag = (le_Base<float>*)this->_inputs[0];
    le_Base<float>* eAngle = (le_Base<float>*)this->_inputs[1];

    // Check null reference
    if (eMag != nullptr && eAngle != nullptr)
    {
        float mag = eMag->GetValue(this->_outputSlots[0]);
        float angle = eAngle->GetValue(this->_outputSlots[1]) / 180.0f * (float)M_PI;

        float real = mag * cosf(angle);
        float imag = mag * sinf(angle);

        this->SetValue(0, real);
        this->SetValue(1, imag);
    }
}

/**
 * @brief Sets the magnitude input for the le_Polar2Rect element.
 * @param e Pointer to the element providing the magnitude.
 * @param outputSlot The slot of the output in the providing element.
 */
void le_Polar2Rect::SetInput_Mag(le_Base<float>* e, uint8_t outputSlot)
{
    le_Element::Connect(e, outputSlot, this, 0);
}

/**
 * @brief Sets the angle input for the le_Polar2Rect element.
 * @param e Pointer to the element providing the angle.
 * @param outputSlot The slot of the output in the providing element.
 */
void le_Polar2Rect::SetInput_Angle(le_Base<float>* e, uint8_t outputSlot)
{
    le_Element::Connect(e, outputSlot, this, 1);
}
