#pragma once

#include "le_Base.hpp"

#ifdef LE_ELEMENTS_ANALOG

/**
 * @brief Class representing a basic conversion from rectangular to polar phasor domain.
 *        Inherits from le_Base with float type.
 */
class le_Rect2Polar : protected le_Base<float>
{
LE_ELEMENT_ACCESS_MOD:
    /**
     * @brief Constructs a le_Rect2Polar object.
     */
    le_Rect2Polar();

    /**
     * @brief Updates the le_Rect2Polar element.
     * @param timeStamp The current timestamp.
     */
    void Update(const le_Time& timeStamp);

public:
    /**
     * @brief Sets the real input for the le_Rect2Polar element.
     * @param e Pointer to the element providing the real part.
     * @param outputSlot The slot of the output in the providing element.
     */
    void SetInput_Real(le_Base<float>* e, uint8_t outputSlot);

    /**
     * @brief Sets the imaginary input for the le_Rect2Polar element.
     * @param e Pointer to the element providing the imaginary part.
     * @param outputSlot The slot of the output in the providing element.
     */
    void SetInput_Imag(le_Base<float>* e, uint8_t outputSlot);

private:
    friend class le_Engine;
};
#endif