#pragma once

#include "le_Base.hpp"

#ifdef LE_ELEMENTS_ANALOG_COMPLEX

/**
 * @brief Class representing a basic conversion from complex to rectangular domain.
 *        Inherits from le_Base with float type.
 */
class le_Rect2Complex : protected le_Base<std::complex<float>>
{
LE_ELEMENT_ACCESS_MOD:
    /**
     * @brief Constructs a le_Rect2Complex object.
     */
    le_Rect2Complex();

    /**
     * @brief Updates the le_Rect2Complex element.
     * @param timeStamp The current timestamp.
     */
    void Update(const le_Time& timeStamp);

public:
    /**
     * @brief Sets the real input for the le_Rect2Complex element.
     * @param e Pointer to the element.
     * @param outputSlot The slot of the output in the providing real value element.
     */
    void SetInput_Real(le_Base<float>* e, uint8_t outputSlot);

    /**
     * @brief Sets the imag input for the le_Rect2Complex element.
     * @param e Pointer to the element.
     * @param outputSlot The slot of the output in the providing imag value element.
     */
    void SetInput_Imag(le_Base<float>* e, uint8_t outputSlot);

private:
    friend class le_Engine;
};
#endif