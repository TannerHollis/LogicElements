#include "le_Polar2Rect.hpp"

#ifdef LE_ELEMENTS_ANALOG

#include <cmath>

/**
 * @brief Constructs a le_Polar2Rect object.
 */
le_Polar2Rect::le_Polar2Rect() : le_Base<float>(le_Element_Type::LE_P2R, 2, 2) {}

/**
 * @brief Updates the le_Polar2Rect element.
 * @param timeStamp The current timestamp.
 */
void le_Polar2Rect::Update(const le_Time& timeStamp)
{
    UNUSED(timeStamp);

    le_Base<float>* eMag = this->template GetInput<le_Base<float>>(0);
    le_Base<float>* eAngle = this->template GetInput<le_Base<float>>(1);

    // Check null reference
    if (eMag != nullptr && eAngle != nullptr)
    {
        float mag = eMag->GetValue(this->GetOutputSlot(0));
        float angle = eAngle->GetValue(this->GetOutputSlot(1)) / 180.0f * (float)M_PI;

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
#endif