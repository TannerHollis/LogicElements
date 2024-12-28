#include "le_Analog3PWinding.hpp"
#include "le_Analog1PWinding.hpp"
/**
 * @brief Constructor that initializes the le_Analog3PWinding with specified samples per cycle.
 * @param samplesPerCycle Number of samples per cycle.
 */
le_Analog3PWinding::le_Analog3PWinding(uint16_t samplesPerCycle) : le_Base<float>(le_Element_Type::LE_ANALOG_3P, 5, 12)
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
        this->SetValue(i * 2, this->_windings[i]->GetValue(0));
        this->SetValue(i * 2 + 1, this->_windings[i]->GetValue(1));
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

/**
 * @brief Sets the real part of the reference input for the winding.
 * @param e The element providing the reference real input value.
 * @param outputSlot The output slot of the element providing the reference real input.
 */
void le_Analog3PWinding::SetInput_RefReal(le_Base<float>* e, uint8_t outputSlot)
{
    le_Element::Connect(e, outputSlot, this, IO_A3P_INPUT_REF_REAL);
}

/**
 * @brief Sets the imaginary part of the reference input for the winding.
 * @param e The element providing the reference imaginary input value.
 * @param outputSlot The output slot of the element providing the reference imaginary input.
 */
void le_Analog3PWinding::SetInput_RefImag(le_Base<float>* e, uint8_t outputSlot)
{
    le_Element::Connect(e, outputSlot, this, IO_A3P_INPUT_REF_IMAG);
}

/**
 * @brief Verify inputs to all Analog1PWindings.
 */
void le_Analog3PWinding::VerifyInputs()
{
    if (this->bInputsVerified)
        return;

    // Check reference signal inputs
    le_Base<float>* refReal = this->template GetInput<le_Base<float>>(IO_A3P_INPUT_REF_REAL);
    le_Base<float>* refImag = this->template GetInput<le_Base<float>>(IO_A3P_INPUT_REF_IMAG);

    // Verify inputs for phases
    for (uint8_t phase = 0; phase < 3; phase++)
    {
        le_Base<float>* phaseInput = this->template GetInput<le_Base<float>>(phase);
        le_Base<float>* phaseWinding = (le_Base<float>*)this->_windings[phase];
        if (phaseInput != nullptr)
        {
            le_Element::Connect(phaseInput, this->_outputSlots[phase], phaseWinding, phase);
        }
        if (refReal != nullptr && refImag != nullptr) // If no reference is detected, connect the A phase as reference
        {
            le_Element::Connect(refReal, this->_outputSlots[IO_A3P_INPUT_REF_REAL], phaseWinding, IO_A1P_INPUT_REF_REAL);
            le_Element::Connect(refImag, this->_outputSlots[IO_A3P_INPUT_REF_IMAG], phaseWinding, IO_A1P_INPUT_REF_IMAG);
        }
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
    std::complex<float> vA = std::complex<float>(this->GetValue(IO_A3P_OUTPUT_REAL_A), this->GetValue(IO_A3P_OUTPUT_IMAG_A));
    std::complex<float> vB = std::complex<float>(this->GetValue(IO_A3P_OUTPUT_REAL_B), this->GetValue(IO_A3P_OUTPUT_IMAG_B));
    std::complex<float> vC = std::complex<float>(this->GetValue(IO_A3P_OUTPUT_REAL_C), this->GetValue(IO_A3P_OUTPUT_IMAG_C));

    // Calculate sequence components
    std::complex<float> v0 = vA + vB + vC;
    std::complex<float> v1 = (vA + _a * vB + _a2 * vC) / 3.0f;
    std::complex<float> v2 = (vA + _a2 * vB + _a * vC) / 3.0f;

    // Save sequence components
    this->SetValue(IO_A3P_OUTPUT_REAL_0, v0.real());  // Zero-Sequence (Real)
    this->SetValue(IO_A3P_OUTPUT_IMAG_0, v0.imag());  // Zero-Sequence (Imag)
    this->SetValue(IO_A3P_OUTPUT_REAL_1, v1.real());  // Positive-Sequence (Real)
    this->SetValue(IO_A3P_OUTPUT_IMAG_1, v1.imag()); // Positive-Sequence (Imag)
    this->SetValue(IO_A3P_OUTPUT_REAL_2, v2.real()); // Negative-Sequence (Real)
    this->SetValue(IO_A3P_OUTPUT_IMAG_2, v2.imag()); // Negative-Sequence (Imag)
}

const std::complex<float> le_Analog3PWinding::a = std::polar(1.0f, (float)(2.0 * M_PI / 3.0)); ///< Constant phasor component.
const std::complex<float> le_Analog3PWinding::a2 = a * a;
