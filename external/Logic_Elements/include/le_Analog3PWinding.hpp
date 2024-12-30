#pragma once

#include "le_Base.hpp"

#ifdef LE_ELEMENTS_ANALOG

#include "le_Analog1PWinding.hpp"
#include "le_Time.hpp"

#include <cmath>
#include <complex>

#define IO_A3P_INPUT_RAW_A 0
#define IO_A3P_INPUT_RAW_B 1
#define IO_A3P_INPUT_RAW_C 2
#ifdef LE_ELEMENTS_ANALOG_COMPLEX
#define IO_A3P_INPUT_REF 3
#define IO_A3P_OUTPUT_A 0
#define IO_A3P_OUTPUT_B 1
#define IO_A3P_OUTPUT_C 2
#define IO_A3P_OUTPUT_0 3
#define IO_A3P_OUTPUT_1 4
#define IO_A3P_OUTPUT_2 5
#else
#define IO_A3P_INPUT_REF_REAL 3
#define IO_A3P_INPUT_REF_IMAG 4
#define IO_A3P_OUTPUT_REAL_A 0
#define IO_A3P_OUTPUT_IMAG_A 1
#define IO_A3P_OUTPUT_REAL_B 2
#define IO_A3P_OUTPUT_IMAG_B 3
#define IO_A3P_OUTPUT_REAL_C 4
#define IO_A3P_OUTPUT_IMAG_C 5
#define IO_A3P_OUTPUT_REAL_0 6
#define IO_A3P_OUTPUT_IMAG_0 7
#define IO_A3P_OUTPUT_REAL_1 8
#define IO_A3P_OUTPUT_IMAG_1 9
#define IO_A3P_OUTPUT_REAL_2 10
#define IO_A3P_OUTPUT_IMAG_2 11
#endif

/**
 * @brief Class representing a three-phase winding in an analog system.
 *        Inherits from le_Base with float type.
 */
#ifdef LE_ELEMENTS_ANALOG_COMPLEX
class le_Analog3PWinding : protected le_Base<std::complex<float>>
#else
class le_Analog3PWinding : protected le_Base<float>
#endif
{
LE_ELEMENT_ACCESS_MOD:
    /**
     * @brief Constructor that initializes the le_Analog3PWinding with specified samples per cycle.
     * @param samplesPerCycle Number of samples per cycle.
     */
    le_Analog3PWinding(uint16_t samplesPerCycle);

    /**
     * @brief Destructor to clean up the le_Analog3PWinding.
     */
    ~le_Analog3PWinding();

    /**
     * @brief Updates the le_Analog3PWinding.
     * @param timeStamp The current timestamp.
     */
    void Update(const le_Time& timeStamp);

public:
    /**
     * @brief Sets the input for Winding A.
     * @param e The element providing the input value for Winding A.
     * @param outputSlot The output slot of the element providing the input.
     */
    void SetInput_WindingA(le_Base<float>* e, uint8_t outputSlot);

    /**
     * @brief Sets the input for Winding B.
     * @param e The element providing the input value for Winding B.
     * @param outputSlot The output slot of the element providing the input.
     */
    void SetInput_WindingB(le_Base<float>* e, uint8_t outputSlot);

    /**
     * @brief Sets the input for Winding C.
     * @param e The element providing the input value for Winding C.
     * @param outputSlot The output slot of the element providing the input.
     */
    void SetInput_WindingC(le_Base<float>* e, uint8_t outputSlot);

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
     * @brief Verify inputs to all Analog1PWindings.
     */
    void VerifyInputs();

    /**
     * @brief Calculate Sequence Components.
     */
    void CalculateSequenceComponents();

    static const std::complex<float> a;     ///< Constant phasor component.
    static const std::complex<float> a2;    ///< Constant phasor component squared.

    le_Analog1PWinding** _windings; ///< Array of single-phase windings.
    bool bInputsVerified; ///< Flag indicating if inputs are verified.

    // Allow le_Engine to access private members
    friend class le_Engine;
};
#endif