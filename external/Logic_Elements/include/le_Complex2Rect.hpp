#pragma once

#include "le_Base.hpp"

#ifdef LE_ELEMENTS_ANALOG_COMPLEX

/**
 * @brief Class representing a basic conversion from complex to rectangular domain.
 *        Inherits from le_Base with float type.
 */
class le_Complex2Rect : protected le_Base<float>
{
LE_ELEMENT_ACCESS_MOD:
    /**
     * @brief Constructs a le_Complex2Rect object.
     */
    le_Complex2Rect();

    /**
     * @brief Updates the le_Complex2Rect element.
     * @param timeStamp The current timestamp.
     */
    void Update(const le_Time& timeStamp);

public:
    /**
     * @brief Sets the input for the le_Complex2Rect element.
     * @param e Pointer to the element.
     * @param outputSlot The slot of the output in the providing element.
     */
    void SetInput(le_Base<std::complex<float>>* e, uint8_t outputSlot);

private:
    friend class le_Engine;
};
#endif