#pragma once

#include "le_Base.hpp"

// Include standard C++ libraries
#include <cmath>
#include <string>

/**
 * @brief Class representing an overcurrent protection element.
 *        Inherits from le_Base with boolean type.
 */
class le_Overcurrent : protected le_Base<bool>
{
private:
    /**
     * @brief Enum representing different overcurrent curve types.
     */
    typedef enum le_Overcurrent_Curve
    {
        LE_C1 = 0,
        LE_C2 = 1,
        LE_C3 = 2,
        LE_C4 = 3,
        LE_C5 = 4,
        LE_U1 = 10,
        LE_U2 = 11,
        LE_U3 = 12,
        LE_U4 = 13,
        LE_U5 = 14,
        LE_DT = 20,
        LE_INVALID = -1
    } le_Overcurrent_Curve;

    /**
     * @brief Parses the curve type from a string.
     * @param curve The string representing the curve type.
     * @return The corresponding le_Overcurrent_Curve type.
     */
    le_Overcurrent_Curve ParseCurveType(std::string& curve);

    /**
     * @brief Sets the curve parameters based on the curve type.
     * @param curve The curve type.
     * @param parameters The array to store the curve parameters.
     */
    void SetCurveParameters(le_Overcurrent_Curve curve, float* parameters);

    /**
     * @brief Constructor that initializes the overcurrent element.
     * @param curve The curve type as a string.
     * @param pickup The pickup current value.
     * @param timeDial The time dial setting.
     * @param timeAdder The time adder value.
     * @param emReset Flag indicating if electromechanical reset is enabled.
     */
    le_Overcurrent(std::string curve, float pickup, float timeDial, float timeAdder, bool emReset);

    /**
     * @brief Updates the overcurrent element.
     * @param timeStep The current timestamp.
     */
    void Update(float timeStep);

public:
    /**
     * @brief Sets the input element for the overcurrent protection.
     * @param e The input element providing the current value.
     */
    void SetInput(le_Base<float>* e);

private:
    le_Overcurrent_Curve curve; ///< The type of the overcurrent curve.
    float fPickup;              ///< The pickup current value.
    float fTimeDial;            ///< The time dial setting.
    float fTimeAdder;           ///< The time adder value.
    bool bEMReset;              ///< Flag indicating if electromechanical reset is enabled.

    float fPercent;             ///< The percentage of the current relative to the pickup value.
    float _fParameters[5];      ///< Array to store the curve parameters.

    // Allow le_Engine to access private members
    friend class le_Engine;
};
