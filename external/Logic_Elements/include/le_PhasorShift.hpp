#pragma once

#include "le_Base.hpp"

/**
 * @brief Class representing a basic shift, using rectangular input.
 *        Inherits from le_Base with float type.
 */
class le_PhasorShift : protected le_Base<float>
{
LE_ELEMENT_ACCESS_MOD:
    /**
     * @brief Constructs a PhasorShift object with given parameters.
     * @param shiftMagnitude Magnitude of the shift.
     * @param shiftAngleClockwise Angle of the shift in degrees clockwise.
     */
    le_PhasorShift(float shiftMagnitude, float shiftAngleClockwise);

    /**
     * @brief Updates the PhasorShift element.
     * @param timeStep The current timestamp.
     */
    void Update(float timeStep);

public:
    /**
     * @brief Sets the real input for the PhasorShift element.
     * @param e Pointer to the element providing the real part.
     * @param outputSlot The slot of the output in the providing element.
     */
    void SetInput_Real(le_Base<float>* e, uint8_t outputSlot);

    /**
     * @brief Sets the imaginary input for the PhasorShift element.
     * @param e Pointer to the element providing the imaginary part.
     * @param outputSlot The slot of the output in the providing element.
     */
    void SetInput_Imag(le_Base<float>* e, uint8_t outputSlot);

private:
    float shiftMag;       ///< Magnitude of the shift.
    float shiftAngle;     ///< Angle of the shift in degrees clockwise.
    float unitShiftReal;  ///< Real part of the unit shift.
    float unitShiftImag;  ///< Imaginary part of the unit shift.

    friend class le_Engine;
};
