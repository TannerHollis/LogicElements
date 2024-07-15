#pragma once

#include "le_Base.hpp"

#include <cmath>
#include <complex>

/**
 * @brief Class representing a single-phase winding in an analog system.
 *        Inherits from le_Base with float type.
 */
class le_Analog1PWinding : protected le_Base<float>
{
protected:
    /**
     * @brief Constructor that initializes the le_Analog1PWinding with specified samples per cycle.
     * @param samplesPerCycle Number of samples per cycle.
     */
    le_Analog1PWinding(uint16_t samplesPerCycle);

    /**
     * @brief Destructor to clean up the le_Analog1PWinding.
     */
    ~le_Analog1PWinding();

    /**
     * @brief Updates the le_Analog1PWinding.
     * @param timeStep The current timestamp.
     */
    void Update(float timeStep);

public:
    /**
     * @brief Sets the raw input for the winding.
     * @param e The element providing the raw input value.
     * @param outputSlot The output slot of the element providing the raw input.
     */
    void SetInput_Winding(le_Base<float>* e, uint16_t outputSlot);

    /**
     * @brief Sets the real part of the reference input for the winding.
     * @param e The element providing the reference real input value.
     * @param outputSlot The output slot of the element providing the reference real input.
     */
    void SetInput_RefReal(le_Base<float>* e, uint16_t outputSlot);

    /**
     * @brief Sets the imaginary part of the reference input for the winding.
     * @param e The element providing the reference imaginary input value.
     * @param outputSlot The output slot of the element providing the reference imaginary input.
     */
    void SetInput_RefImag(le_Base<float>* e, uint16_t outputSlot);

private:
    /**
     * @brief Calculates the phasor for the winding.
     */
    void CalculatePhasor();

    /**
     * @brief Applies a cosine filter to the raw values.
     */
    void ApplyCosineFilter();

    /**
     * @brief Applies an angular shift with reference to the reference input.
     */
    void AdjustOutputAngleWithReference();

    uint16_t uSamplesPerCycle; ///< Number of samples per cycle.

    float* _rawValues;          ///< Raw values buffer.
    float* _rawFilteredValues;  ///< Raw filtered values buffer.
    uint16_t uWrite;            ///< Write pointer.
    uint16_t uQuarterCycle;     ///< Quarter cycle read pointer.

    /**
     * @brief Structure to hold filter parameters.
     */
    struct {
        uint16_t length;       ///< Length of the filter.
        float* coefficients;   ///< Coefficients of the filter.
    } sFilter;

    // Allow le_Engine and le_Analog3PWinding to access private members
    friend class le_Engine;
    friend class le_Analog3PWinding;

};
