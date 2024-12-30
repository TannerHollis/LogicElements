#pragma once

#include "le_Base.hpp"

#ifdef LE_ELEMENTS_ANALOG

/**
 * @brief Class representing a basic shift, using rectangular input.
 *        Inherits from le_Base with float type.
 */
#ifdef LE_ELEMENTS_ANALOG_COMPLEX
class le_PhasorShift : protected le_Base<std::complex<float>>
#else
class le_PhasorShift : protected le_Base<float>
#endif
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
     * @param timeStamp The current timestamp.
     */
    void Update(const le_Time& timeStamp);

public:
#ifdef LE_ELEMENTS_ANALOG_COMPLEX
    /**
     * @brief Sets the input for the PhasorShift element.
     * @param e Pointer to the element.
     * @param outputSlot The slot of the output in the providing element.
     */
    void SetInput(le_Base<std::complex<float>>* e, uint8_t outputSlot);
#else
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
#endif

private:
    float shiftMag;       ///< Magnitude of the shift.
    float shiftAngle;     ///< Angle of the shift in degrees clockwise.
    float unitShiftReal;  ///< Real part of the unit shift.
    float unitShiftImag;  ///< Imaginary part of the unit shift.

    friend class le_Engine;
};
#endif // LE_ELEMENTS_ANALOG