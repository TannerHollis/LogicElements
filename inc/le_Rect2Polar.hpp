#pragma once

#include "le_Base.hpp"

#include <cmath>

/**
 * @brief Class representing a basic conversion from rectangular to polar phasor domain.
 *        Inherits from le_Base with float type.
 */
class le_Phasor : protected le_Base<float>
{
LE_ELEMENT_ACCESS_MOD:
    le_Phasor