#include "le_PhasorShift.hpp"

#ifdef LE_ELEMENTS_ANALOG

/**
 * @brief Constructs a PhasorShift object with given parameters.
 * @param shiftMagnitude Magnitude of the shift.
 * @param shiftAngleClockwise Angle of the shift in degrees clockwise.
 */
#ifdef LE_ELEMENTS_ANALOG_COMPLEX
le_PhasorShift::le_PhasorShift(float shiftMagnitude, float shiftAngleClockwise)
    : le_Base<std::complex<float>>(le_Element_Type::LE_PHASOR_SHIFT, 1, 2), shiftMag(shiftMagnitude), shiftAngle(shiftAngleClockwise)
#else
le_PhasorShift::le_PhasorShift(float shiftMagnitude, float shiftAngleClockwise)
    : le_Base<float>(le_Element_Type::LE_PHASOR_SHIFT, 2, 2), shiftMag(shiftMagnitude), shiftAngle(shiftAngleClockwise)
#endif
{
    // Set implicit variables
    this->unitShiftReal = shiftMagnitude * cosf(shiftAngleClockwise / 180.0f * (float)M_PI);
    this->unitShiftImag = shiftMagnitude * -sinf(shiftAngleClockwise / 180.0f * (float)M_PI);
}

/**
 * @brief Updates the PhasorShift element.
 * @param timeStamp The current timestamp.
 */
void le_PhasorShift::Update(const le_Time& timeStamp)
{
    UNUSED(timeStamp);
#ifdef LE_ELEMENTS_ANALOG_COMPLEX
    le_Base<std::complex<float>>* e = this->template GetInput<le_Base<std::complex<float>>>(0);

    if(e != nullptr)
    {
        float real = e->GetValue(this->GetOutputSlot(0)).real();
        float imag = e->GetValue(this->GetOutputSlot(0)).imag();
#else
    le_Base<float>* eReal = this->template GetInput<le_Base<float>>(0);
    le_Base<float>* eImag = this->template GetInput<le_Base<float>>(1);

    // Check null reference
    if (eReal != nullptr && eImag != nullptr)
    {
        float real = eReal->GetValue(this->GetOutputSlot(0));
        float imag = eImag->GetValue(this->GetOutputSlot(1));
#endif

        float newReal = real * unitShiftReal + imag * unitShiftImag;
        float newImag = imag * unitShiftReal - real * unitShiftImag;

        this->SetValue(0, newReal);
        this->SetValue(1, newImag);
    }
}

#ifdef LE_ELEMENTS_ANALOG_COMPLEX
/**
 * @brief Sets the input.
 * @param e The element providing the input value.
 * @param outputSlot The output slot of the element providing the input.
 */
void le_PhasorShift::SetInput(le_Base<std::complex<float>>* e, uint8_t outputSlot)
{
    le_Element::Connect(e, outputSlot, this, 0);
}
#else
/**
 * @brief Sets the real input for the PhasorShift element.
 * @param e Pointer to the element providing the real part.
 * @param outputSlot The slot of the output in the providing element.
 */
void le_PhasorShift::SetInput_Real(le_Base<float>* e, uint8_t outputSlot)
{
    le_Element::Connect(e, outputSlot, this, 0);
}

/**
 * @brief Sets the imaginary input for the PhasorShift element.
 * @param e Pointer to the element providing the imaginary part.
 * @param outputSlot The slot of the output in the providing element.
 */
void le_PhasorShift::SetInput_Imag(le_Base<float>* e, uint8_t outputSlot)
{
    le_Element::Connect(e, outputSlot, this, 1);
}
#endif // LE_ELEMENTS_ANALOG_COMPLEX
#endif // LE_ELEMENTS_ANALOG