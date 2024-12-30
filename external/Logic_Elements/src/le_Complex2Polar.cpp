#include "le_Complex2Polar.hpp"

#ifdef LE_ELEMENTS_ANALOG_COMPLEX

/**
 * @brief Constructs a le_Complex2Polar object.
 */
le_Complex2Polar::le_Complex2Polar() : le_Base<float>(le_Element_Type::LE_C2P, 1, 2) {}

/**
 * @brief Updates the le_Complex2Polar element.
 * @param timeStamp The current timestamp.
 */
void le_Complex2Polar::Update(const le_Time& timeStamp)
{
    UNUSED(timeStamp);

    le_Base<std::complex<float>>* e = this->template GetInput<le_Base<std::complex<float>>>(0);

    // Check null reference
    if (e != nullptr)
    {
        std::complex<float> val = e->GetValue(this->GetOutputSlot(0));

        float mag = std::abs(val);
        float angle = std::arg(val) * 180.0f / (float)M_PI;

        this->SetValue(0, mag);
        this->SetValue(1, angle);
    }
}

/**
* @brief Sets the input for the le_Complex2Polar element.
* @param e Pointer to the element.
* @param outputSlot The slot of the output in the providing element.
*/
void le_Complex2Polar::SetInput(le_Base<std::complex<float>>* e, uint8_t outputSlot)
{
    le_Element::Connect(e, outputSlot, this, 0);
}
#endif