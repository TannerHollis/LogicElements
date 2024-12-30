#include "le_Polar2Complex.hpp"

#ifdef LE_ELEMENTS_ANALOG_COMPLEX

/**
 * @brief Constructs a le_Polar2Complex object.
 */
le_Polar2Complex::le_Polar2Complex() : le_Base<std::complex<float>>(le_Element_Type::LE_P2C, 2, 1) {}

/**
 * @brief Updates the le_Polar2Complex element.
 * @param timeStamp The current timestamp.
 */
void le_Polar2Complex::Update(const le_Time& timeStamp)
{
    UNUSED(timeStamp);

    le_Base<float>* eMag = this->template GetInput<le_Base<float>>(0);
    le_Base<float>* eAngle = this->template GetInput<le_Base<float>>(1);

    // Check null reference
    if (eMag != nullptr && eAngle != nullptr)
    {
        float mag = eMag->GetValue(this->GetOutputSlot(0));
        float angle = eAngle->GetValue(this->GetOutputSlot(1));

        float real = mag * cosf(angle / 180.0f * (float)M_PI);
        float imag = mag * sinf(angle / 180.0f * (float)M_PI);

        this->SetValue(0, std::complex<float>(real, imag));
    }
}

/**
* @brief Sets the magnitude value input for the le_Polar2Complex element.
* @param e Pointer to the element.
* @param outputSlot The slot of the output in the providing magnitude value element.
*/
void le_Polar2Complex::SetInput_Mag(le_Base<float>* e, uint8_t outputSlot)
{
    le_Element::Connect(e, outputSlot, this, 0);
}

/**
* @brief Sets the angle value input for the le_Polar2Complex element.
* @param e Pointer to the element.
* @param outputSlot The slot of the output in the providing angle value element.
*/
void le_Polar2Complex::SetInput_Angle(le_Base<float>* e, uint8_t outputSlot)
{
    le_Element::Connect(e, outputSlot, this, 1);
}
#endif