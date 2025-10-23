#pragma once

#include "le_Element.hpp"

#ifdef LE_ELEMENTS_ANALOG
#include "le_Time.hpp"

// Include standard C++ libraries
#include <string>


namespace LogicElements {
/**
 * @brief Class representing an overcurrent protection element.
 *        HETEROGENEOUS: float current input → bool trip output!
 */
class Overcurrent : public Element
{
private:
    /**
     * @brief Enum representing different overcurrent curve types.
     */
    typedef enum OvercurrentCurve
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
    } OvercurrentCurve;

    /**
     * @brief Parses the curve type from a string.
     * @param curve The string representing the curve type.
     * @return The corresponding OvercurrentCurve type.
     */
    static OvercurrentCurve ParseCurveType(std::string& curve);

LE_ELEMENT_ACCESS_MOD:
    /**
     * @brief Constructor that initializes the overcurrent element.
     * @param curve The curve type as a string.
     * @param pickup The pickup current value.
     * @param timeDial The time dial setting.
     * @param timeAdder The time adder value.
     * @param emReset Flag indicating if electromechanical reset is enabled.
     */
    Overcurrent(std::string curve, float pickup, float timeDial, float timeAdder, bool emReset);

    /**
     * @brief Updates the overcurrent element.
     * @param timeStamp The current timestamp.
     */
    void Update(const Time& timeStamp) override;

public:
    /**
     * @brief Sets the input element for the overcurrent protection.
     * @param sourceElement The input element providing the current value.
     * @param sourcePortName The name of the output port on the source element.
     */
    void SetInput(Element* sourceElement, const std::string& sourcePortName);

    /**
     * @brief Gets the trip output (bool).
     * @return True if tripped, false otherwise.
     */
    bool GetTrip() const;

private:
    InputPort<float>* pCurrent;  ///< Current input port (HETEROGENEOUS: float input).
    OutputPort<bool>* pTrip;      ///< Trip output port (HETEROGENEOUS: bool output).

    /**
     * @brief Sets the curve parameters based on the curve type.
     * @param curve The curve type.
     * @param parameters The array to store the curve parameters.
     */
    static void SetCurveParameters(OvercurrentCurve curve, float* parameters);

    OvercurrentCurve curve; ///< The type of the overcurrent curve.
    float fPickup;              ///< The pickup current value.
    float fTimeDial;            ///< The time dial setting.
    float fTimeAdder;           ///< The time adder value.
    bool bEMReset;              ///< Flag indicating if electromechanical reset is enabled.

    float fPercent;             ///< The percentage of the current relative to the pickup value.
    float _fParameters[5];      ///< Array to store the curve parameters.

    Time lastTimeStamp;  ///< The previous timestamp for calculating the timestep.

    // Allow le_Engine to access private members
    friend class Engine;
};
#endif

} // namespace LogicElements
