#pragma once

#include "le_Base.hpp"

#ifdef LE_ELEMENTS_ANALOG

#include "le_Time.hpp"

#include <cmath>
#include <complex>

#define IO_A1P_INPUT_RAW 0
#ifdef LE_ELEMENTS_ANALOG_COMPLEX
#define IO_A1P_INPUT_REF 1
#define IO_A1P_OUTPUT 0
#else
#define IO_A1P_INPUT_REF_REAL 1
#define IO_A1P_INPUT_REF_IMAG 2
#define IO_A1P_OUTPUT_REAL 0
#define IO_A1P_OUTPUT_IMAG 1
#endif

/**
 * @brief Class representing a single-phase winding in an analog system.
 *        Inherits from le_Base with float type.
 */
#ifdef LE_ELEMENTS_ANALOG_COMPLEX
class le_Analog1PWinding : protected le_Base<std::complex<float>>
#else
class le_Analog1PWinding : protected le_Base<float>
#endif
{
LE_ELEMENT_ACCESS_MOD:
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
     * @param timeStamp The current timestamp.
     */
    void Update(const le_Time& timeStamp);

public:
    /**
     * @brief Sets the raw input for the winding.
     * @param e The element providing the raw input value.
     * @param outputSlot The output slot of the element providing the raw input.
     */
    void SetInput_Winding(le_Base<float>* e, uint8_t outputSlot);

#ifdef LE_ELEMENTS_ANALOG_COMPLEX
    /**
     * @brief Sets the real part of the reference input for the winding.
     * @param e The element providing the reference real input value.
     * @param outputSlot The output slot of the element providing the reference real input.
     */
    void SetInput_Ref(le_Base<std::complex<float>>* e, uint8_t outputSlot);
#else
    /**
     * @brief Sets the real part of the reference input for the winding.
     * @param e The element providing the reference real input value.
     * @param outputSlot The output slot of the element providing the reference real input.
     */
    void SetInput_RefReal(le_Base<float>* e, uint8_t outputSlot);

    /**
     * @brief Sets the imaginary part of the reference input for the winding.
     * @param e The element providing the reference imaginary input value.
     * @param outputSlot The output slot of the element providing the reference imaginary input.
     */
    void SetInput_RefImag(le_Base<float>* e, uint8_t outputSlot);
#endif

private:
    /**
     * @brief Calculates the phasor for the winding.
     */
    inline void CalculatePhasor();

    /**
     * @brief Applies a cosine filter to the raw values.
     */
    inline void ApplyCosineFilter();

    /**
     * @brief Applies an angular shift with reference to the reference input.
     */
    inline void AdjustOutputAngleWithReference();

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
#endif