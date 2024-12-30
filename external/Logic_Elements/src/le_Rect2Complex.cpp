#include "le_Rect2Complex.hpp"

#ifdef LE_ELEMENTS_ANALOG_COMPLEX

/**
 * @brief Constructs a le_Rect2Complex object.
 */
le_Rect2Complex::le_Rect2Complex() : le_Base<std::complex<float>>(le_Element_Type::LE_R2C, 2, 1) {}

/**
 * @brief Updates the le_Rect2Complex element.
 * @param timeStamp The current timestamp.
 */
void le_Rect2Complex::Update(const le_Time& timeStamp)
{
    UNUSED(timeStamp);

    le_Base<float>* eReal = this->template GetInput<le_Base<float>>(0);
    le_Base<float>* eImag = this->template GetInput<le_Base<float>>(1);

    // Check null reference
    if (eReal != nullptr && eImag != nullptr)
    {
        float real = eReal->GetValue(this->GetOutputSlot(0));
        float imag = eImag->GetValue(this->GetOutputSlot(1));

        this->SetValue(0, std::complex<float>(real, imag));
    }
}

/**
* @brief Sets the real value input for the le_Rect2Complex element.
* @param e Pointer to the element.
* @param outputSlot The slot of the output in the providing real value element.
*/
void le_Rect2Complex::SetInput_Real(le_Base<float>* e, uint8_t outputSlot)
{
    le_Element::Connect(e, outputSlot, this, 0);
}

/**
* @brief Sets the imag value input for the le_Rect2Complex element.
* @param e Pointer to the element.
* @param outputSlot The slot of the output in the providing imag value element.
*/
void le_Rect2Complex::SetInput_Imag(le_Base<float>* e, uint8_t outputSlot)
{
    le_Element::Connect(e, outputSlot, this, 1);
}
#endif