#pragma once

#include <cstdint>

namespace LogicElements {

class Utility
{
public:
    /**
     * @brief Converts a ratio of two integers into integer and fractional parts.
     *
     * This function calculates the ratio of two integers (numerator and denominator) and
     * splits the result into its integer and fractional parts. The fractional part is
     * represented as a value scaled by 1000 to maintain precision.
     *
     * @param numerator The numerator of the ratio.
     * @param denominator The denominator of the ratio.
     * @param integerPart Pointer to store the integer part of the resulting ratio.
     * @param fractionalPart Pointer to store the fractional part of the resulting ratio,
     *                       scaled by 1000.
     */
    static void ConvertFloatingPoint(uint64_t numerator, uint64_t denominator, uint16_t* integerPart, uint16_t* fractionalPart);
};

} // namespace LogicElements
