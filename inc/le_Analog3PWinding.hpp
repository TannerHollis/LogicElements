#pragma once

#include "le_Base.hpp"
#include "le_Analog1PWinding.hpp"

#include <cmath>
#include <complex>

/**
 * @brief Class representing a three-phase winding in an analog system.
 *        Inherits from le_Base with float type.
 */
class le_Analog3PWinding : protected le_Base<float>
{
private:
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
     * @param timeStep The current timestamp.
     */
    void Update(float timeStep);

public:
    /**
     * @brief Sets the input for Winding A.
     * @param e The element providing the input value for Winding A.
     * @param outputSlot The output slot of the element providing the input.
     */
    void SetInput_WindingA(le_Base<float>* e, uint16_t outputSlot);

    /**
     * @brief Sets the input for Winding B.
     * @param e The element providing the input value for Winding B.
     * @param outputSlot The output slot of the element providing the input.
     */
    void SetInput_WindingB(le_Base<float>* e, uint16_t outputSlot);

    /**
     * @brief Sets the input for Winding C.
     * @param e The element providing the input value for Winding C.
     * @param outputSlot The output slot of the element providing the input.
     */
    void SetInput_WindingC(le_Base<float>* e, uint16_t outputSlot);

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
     * @brief Verify inputs to all Analog1PWindings.
     */
    void VerifyInputs();

    /**
     * @brief Calculate Sequence Components.
     */
    void CalculateSequenceComponents();

    static const std::complex<float> a;     ///< Constant phasor component.
    static const std::complex<float> a2;    ///< Constant phasor component squared.

    le_Analog1PWinding* _windings; ///< Array of single-phase windings.
    bool bInputsVerified; ///< Flag indicating if inputs are verified.

    // Allow le_Engine to access private members
    friend class le_Engine;
};
