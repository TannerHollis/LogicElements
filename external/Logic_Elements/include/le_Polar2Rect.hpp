#pragma once

#include "le_Base.hpp"

/**
 * @brief Class representing a basic conversion from polar to rectangular phasor domain.
 *        Inherits from le_Base with float type.
 */
class le_Polar2Rect : protected le_Base<float>
{
LE_ELEMENT_ACCESS_MOD:
    /**
     * @brief Constructs a le_Polar2Rect object.
     */
    le_Polar2Rect();

    /**
     * @brief Updates the le_Polar2Rect element.
     * @param timeStep The current timestamp.
     */
    void Update(float timeStep);

public:
    /**
     * @brief Sets the magnitude input for the le_Polar2Rect element.
     * @param e Pointer to the element providing the magnitude.
     * @param outputSlot The slot of the output in the providing element.
     */
    void SetInput_Mag(le_Base<float>* e, uint8_t outputSlot);

    /**
     * @brief Sets the angle input for the le_Polar2Rect element.
     * @param e Pointer to the element providing the angle.
     * @param outputSlot The slot of the output in the providing element.
     */
    void SetInput_Angle(le_Base<float>* e, uint8_t outputSlot);

private:
    friend class le_Engine;
};
