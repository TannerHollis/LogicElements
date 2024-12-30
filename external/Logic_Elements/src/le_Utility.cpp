#include "le_Utility.hpp"

void le_Utility::ConvertFloatingPoint(uint64_t numerator, uint64_t denominator, uint16_t* integerPart, uint16_t* fractionalPart)
{
    if (denominator == 0) {
        // Handle division by zero if necessary
        *integerPart = 0;
        *fractionalPart = 0;
        return;
    }

    *integerPart = static_cast<uint16_t>(numerator / denominator);
    uint64_t remainder = numerator % denominator;
    *fractionalPart = static_cast<uint16_t>((remainder * 1000) / denominator);
}
