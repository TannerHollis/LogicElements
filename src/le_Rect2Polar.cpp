#include "le_Rect2Polar.hpp"
#include <cmath>

/**
 * @brief Constructs a le_Rect2Polar object.
 */
le_Rect2Polar::le_Rect2Polar() : le_Base<float>(2, 2) {}

/**
 * @brief Updates the le_Rect2Polar element.
 * @param timeStep The current timestamp.
 */
void le_Rect2Polar::Update(float timeStep)
{
    UNUSED(timeStep);

    le_Base<float>* eReal = (le_Base<float>*)this->_inputs[0];
    le_Base<float>* eImag = (le_Base<float>*)this->_inputs[1];

    // Check null reference
    if (eReal != nullptr && eImag != nullptr)
    {
        float real = eReal->GetValue(this->_outputSlots[0]);
        float imag = eImag->GetValue(this->_outputSlots[1]);

        float mag = sqrtf(real * real + imag * imag);
        float angle = atan2f(imag, real) * 180.0f / (float)M_PI;

        this->SetValue(0, mag);
        this->SetValue(1, angle);
    }
}

/**
 * @brief Sets the real input for the le_Rect2Polar element.
 * @param e Pointer to the element providing the real part.
 * @param outputSlot The slot of the output in the providing element.
 */
void le_Rect2Polar::SetInput_Real(le_Base<float>* e, uint8_t outputSlot)
{
    le_Element::Connect(e, outputSlot, this, 0);
}

/**
 * @brief Sets the imaginary input for the le_Rect2Polar element.
 * @param e Pointer to the element providing the imaginary part.
 * @param outputSlot The slot of the output in the providing element.
 */
void le_Rect2Polar::SetInput_Imag(le_Base<float>* e, uint8_t outputSlot)
{
    le_Element::Connect(e, outputSlot, this, 1);
}
