#include "le_PhasorShift.hpp"
#include <cmath>

/**
 * @brief Constructs a PhasorShift object with given parameters.
 * @param shiftMagnitude Magnitude of the shift.
 * @param shiftAngleClockwise Angle of the shift in degrees clockwise.
 */
le_PhasorShift::le_PhasorShift(float shiftMagnitude, float shiftAngleClockwise)
    : le_Base<float>(2, 2), shiftMag(shiftMagnitude), shiftAngle(shiftAngleClockwise)
{
    // Set implicit variables
    this->unitShiftReal = shiftMagnitude * cosf(shiftAngleClockwise / 180.0f * (float)M_PI);
    this->unitShiftImag = shiftMagnitude * -sinf(shiftAngleClockwise / 180.0f * (float)M_PI);
}

/**
 * @brief Updates the PhasorShift element.
 * @param timeStep The current timestamp.
 */
void le_PhasorShift::Update(float timeStep)
{
    UNUSED(timeStep);

    le_Base<float>* eReal = (le_Base<float>*)this->_inputs[0];
    le_Base<float>* eImag = (le_Base<float>*)this->_inputs[1];

    // Check null reference
    if (eReal != nullptr && eImag != nullptr)
    {
        float real = eReal->GetValue(this->_outputSlots[0]);
        float imag = eImag->GetValue(this->_outputSlots[1]);

        float newReal = real * unitShiftReal + imag * unitShiftImag;
        float newImag = imag * unitShiftReal - real * unitShiftImag;

        this->SetValue(0, newReal);
        this->SetValue(1, newImag);
    }
}

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
