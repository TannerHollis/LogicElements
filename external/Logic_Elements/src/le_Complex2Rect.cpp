#include "le_Complex2Rect.hpp"

#ifdef LE_ELEMENTS_ANALOG_COMPLEX

/**
 * @brief Constructs a le_Complex2Rect object.
 */
le_Complex2Rect::le_Complex2Rect() : le_Base<float>(le_Element_Type::LE_C2R, 1, 2) {}

/**
 * @brief Updates the le_Complex2Rect element.
 * @param timeStamp The current timestamp.
 */
void le_Complex2Rect::Update(const le_Time& timeStamp)
{
    UNUSED(timeStamp);

    le_Base<std::complex<float>>* e = this->template GetInput<le_Base<std::complex<float>>>(0);

    // Check null reference
    if (e != nullptr)
    {
        std::complex<float> val = e->GetValue(this->GetOutputSlot(0));

        this->SetValue(0, val.real());
        this->SetValue(1, val.imag());
    }
}

/**
* @brief Sets the input for the le_Complex2Rect element.
* @param e Pointer to the element.
* @param outputSlot The slot of the output in the providing element.
*/
void le_Complex2Rect::SetInput(le_Base<std::complex<float>>* e, uint8_t outputSlot)
{
    le_Element::Connect(e, outputSlot, this, 0);
}
#endif