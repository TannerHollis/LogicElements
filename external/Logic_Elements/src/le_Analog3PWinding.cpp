#include "le_Analog3PWinding.hpp"

#ifdef LE_ELEMENTS_ANALOG
/**
 * @brief Constructor that initializes the le_Analog3PWinding with specified samples per cycle.
 * @param samplesPerCycle Number of samples per cycle.
 */
#ifdef LE_ELEMENTS_ANALOG_COMPLEX
le_Analog3PWinding::le_Analog3PWinding(uint16_t samplesPerCycle) : le_Base<std::complex<float>>(le_Element_Type::LE_ANALOG_3P, 4, 6)
#else
le_Analog3PWinding::le_Analog3PWinding(uint16_t samplesPerCycle) : le_Base<float>(le_Element_Type::LE_ANALOG_3P, 5, 12)
#endif
{
    // Allocate memory for all windings
    this->_windings = new le_Analog1PWinding*[3];
    for (uint8_t i = 0; i < 3; i++)
    {
        this->_windings[i] = new le_Analog1PWinding(samplesPerCycle);
    }

    this->bInputsVerified = false;
}

/**
 * @brief Destructor to clean up the le_Analog3PWinding.
 */
le_Analog3PWinding::~le_Analog3PWinding()
{
    // Deallocate arrays
    for (uint8_t i = 0; i < 3; i++)
    {
        delete this->_windings[i];
    }

    delete[] this->_windings;
}

/**
 * @brief Updates the le_Analog3PWinding.
 * @param timeStamp The current timestamp.
 */
void le_Analog3PWinding::Update(const le_Time& timeStamp)
{
    VerifyInputs();

    // Update each winding
    for (uint8_t i = 0; i < 3; i++)
    {
        this->_windings[i]->Update(timeStamp);

        // Set phase values
#ifdef LE_ELEMENTS_ANALOG_COMPLEX
        this->SetValue(i, this->_windings[i]->GetValue(IO_A1P_OUTPUT));
#else
        this->SetValue(i * 2, this->_windings[i]->GetValue(IO_A1P_OUTPUT_REAL));
        this->SetValue(i * 2 + 1, this->_windings[i]->GetValue(IO_A1P_OUTPUT_IMAG));
#endif
    }

    // Set sequence component values
    CalculateSequenceComponents();
}

/**
 * @brief Sets the input for Winding A.
 * @param e The element providing the input value for Winding A.
 * @param outputSlot The output slot of the element providing the input.
 */
void le_Analog3PWinding::SetInput_WindingA(le_Base<float>* e, uint8_t outputSlot)
{
    le_Element::Connect(e, outputSlot, this, IO_A3P_INPUT_RAW_A);
}

/**
 * @brief Sets the input for Winding B.
 * @param e The element providing the input value for Winding B.
 * @param outputSlot The output slot of the element providing the input.
 */
void le_Analog3PWinding::SetInput_WindingB(le_Base<float>* e, uint8_t outputSlot)
{
    le_Element::Connect(e, outputSlot, this, IO_A3P_INPUT_RAW_B);
}

/**
 * @brief Sets the input for Winding C.
 * @param e The element providing the input value for Winding C.
 * @param outputSlot The output slot of the element providing the input.
 */
void le_Analog3PWinding::SetInput_WindingC(le_Base<float>* e, uint8_t outputSlot)
{
    le_Element::Connect(e, outputSlot, this, IO_A3P_INPUT_RAW_C);
}

#ifdef LE_ELEMENTS_ANALOG_COMPLEX
/**
 * @brief Sets the reference input for the winding.
 * @param e The element providing the reference input value.
 * @param outputSlot The output slot of the element providing the reference input.
 */
void le_Analog3PWinding::SetInput_Ref(le_Base<std::complex<float>>* e, uint8_t outputSlot)
{
    le_Element::Connect(e, outputSlot, this, IO_A1P_INPUT_REF);
}
#else
/**
 * @brief Sets the real part of the reference input for the winding.
 * @param e The element providing the reference real input value.
 * @param outputSlot The output slot of the element providing the reference real input.
 */
void le_Analog3PWinding::SetInput_RefReal(le_Base<float>* e, uint8_t outputSlot)
{
    le_Element::Connect(e, outputSlot, this, IO_A1P_INPUT_REF_REAL);
}

/**
 * @brief Sets the imaginary part of the reference input for the winding.
 * @param e The element providing the reference imaginary input value.
 * @param outputSlot The output slot of the element providing the reference imaginary input.
 */
void le_Analog3PWinding::SetInput_RefImag(le_Base<float>* e, uint8_t outputSlot)
{
    le_Element::Connect(e, outputSlot, this, IO_A1P_INPUT_REF_IMAG);
}
#endif

/**
 * @brief Verify inputs to all Analog1PWindings.
 */
void le_Analog3PWinding::VerifyInputs()
{
    if (this->bInputsVerified)
        return;

    // Check reference signal inputs
#ifdef LE_ELEMENTS_ANALOG_COMPLEX
    le_Base<std::complex<float>>* ref = this-> template GetInput<le_Base<std::complex<float>>>(IO_A3P_INPUT_REF);
#else
    le_Base<float>* refReal = this->template GetInput<le_Base<float>>(IO_A3P_INPUT_REF_REAL);
    le_Base<float>* refImag = this->template GetInput<le_Base<float>>(IO_A3P_INPUT_REF_IMAG);
#endif

    // Verify inputs for phases
    for (uint8_t phase = 0; phase < 3; phase++)
    {
        le_Base<float>* phaseInput = this->template GetInput<le_Base<float>>(phase);
        le_Base<float>* phaseWinding = (le_Base<float>*)this->_windings[phase];
        if (phaseInput != nullptr)
        {
            le_Element::Connect(phaseInput, this->_outputSlots[phase], phaseWinding, IO_A1P_INPUT_RAW);
        }
#ifdef LE_ELEMENTS_ANALOG_COMPLEX
        if (ref != nullptr) // If reference is detected, connect the it through as reference
        {
            le_Element::Connect(ref, this->_outputSlots[IO_A3P_INPUT_REF], phaseWinding, IO_A1P_INPUT_REF);
        }
#else
        if (refReal != nullptr && refImag != nullptr) // If reference is detected, connect the it through as reference
        {
            le_Element::Connect(refReal, this->_outputSlots[IO_A3P_INPUT_REF_REAL], phaseWinding, IO_A1P_INPUT_REF_REAL);
            le_Element::Connect(refImag, this->_outputSlots[IO_A3P_INPUT_REF_IMAG], phaseWinding, IO_A1P_INPUT_REF_IMAG);
        }
#endif
    }

    this->bInputsVerified = true;
}

/**
 * @brief Calculate Sequence Components.
 */
void le_Analog3PWinding::CalculateSequenceComponents()
{
    // Get all complex values
    std::complex<float> _a = this->a;
    std::complex<float> _a2 = this->a2;
#ifdef LE_ELEMENTS_ANALOG_COMPLEX
    std::complex<float> vA = this->GetValue(IO_A3P_OUTPUT_A);
    std::complex<float> vB = this->GetValue(IO_A3P_OUTPUT_B);
    std::complex<float> vC = this->GetValue(IO_A3P_OUTPUT_C);
#else
    std::complex<float> vA = std::complex<float>(this->GetValue(IO_A3P_OUTPUT_REAL_A), this->GetValue(IO_A3P_OUTPUT_IMAG_A));
    std::complex<float> vB = std::complex<float>(this->GetValue(IO_A3P_OUTPUT_REAL_B), this->GetValue(IO_A3P_OUTPUT_IMAG_B));
    std::complex<float> vC = std::complex<float>(this->GetValue(IO_A3P_OUTPUT_REAL_C), this->GetValue(IO_A3P_OUTPUT_IMAG_C));
#endif

    // Calculate sequence components
    std::complex<float> v0 = vA + vB + vC;
    std::complex<float> v1 = (vA + _a * vB + _a2 * vC) / 3.0f;
    std::complex<float> v2 = (vA + _a2 * vB + _a * vC) / 3.0f;

    // Save sequence components
#ifdef LE_ELEMENTS_ANALOG_COMPLEX
    this->SetValue(IO_A3P_OUTPUT_0, v0);
    this->SetValue(IO_A3P_OUTPUT_1, v1);
    this->SetValue(IO_A3P_OUTPUT_2, v2);
#else
    this->SetValue(IO_A3P_OUTPUT_REAL_0, v0.real());  // Zero-Sequence (Real)
    this->SetValue(IO_A3P_OUTPUT_IMAG_0, v0.imag());  // Zero-Sequence (Imag)
    this->SetValue(IO_A3P_OUTPUT_REAL_1, v1.real());  // Positive-Sequence (Real)
    this->SetValue(IO_A3P_OUTPUT_IMAG_1, v1.imag()); // Positive-Sequence (Imag)
    this->SetValue(IO_A3P_OUTPUT_REAL_2, v2.real()); // Negative-Sequence (Real)
    this->SetValue(IO_A3P_OUTPUT_IMAG_2, v2.imag()); // Negative-Sequence (Imag)
#endif
}

const std::complex<float> le_Analog3PWinding::a = std::polar(1.0f, (float)(2.0 * M_PI / 3.0)); ///< Constant phasor component.
const std::complex<float> le_Analog3PWinding::a2 = a * a;
#endif