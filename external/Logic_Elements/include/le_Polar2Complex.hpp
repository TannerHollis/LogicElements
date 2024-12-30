#pragma once

#include "le_Base.hpp"

#ifdef LE_ELEMENTS_ANALOG_COMPLEX

#include <complex>
/**
 * @brief Class representing a basic conversion from complex to phasor domain.
 *        Inherits from le_Base with float type.
 */
class le_Polar2Complex : protected le_Base<std::complex<float>>
{
LE_ELEMENT_ACCESS_MOD:
    /**
     * @brief Constructs a le_Polar2Complex object.
     */
    le_Polar2Complex();

    /**
     * @brief Updates the le_Polar2Complex element.
     * @param timeStamp The current timestamp.
     */
    void Update(const le_Time& timeStamp);

public:
    /**
     * @brief Sets the magnitude input for the le_Polar2Complex element.
     * @param e Pointer to the element.
     * @param outputSlot The slot of the output in the providing magnitude value element.
     */
    void SetInput_Mag(le_Base<float>* e, uint8_t outputSlot);

    /**
     * @brief Sets the angle input for the le_Polar2Complex element.
     * @param e Pointer to the element.
     * @param outputSlot The slot of the output in the providing angle value element.
     */
    void SetInput_Angle(le_Base<float>* e, uint8_t outputSlot);

private:
    friend class le_Engine;
};
#endif