#include "le_Analog3PWinding.hpp"

/**
 * @brief Constructor that initializes the le_Analog3PWinding with specified samples per cycle.
 * @param samplesPerCycle Number of samples per cycle.
 */
le_Analog3PWinding::le_Analog3PWinding(uint16_t samplesPerCycle) : le_Base<float>(5, 12)
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
 * @param timeStep The current timestamp.
 */
void le_Analog3PWinding::Update(float timeStep)
{
    VerifyInputs();

    // Update each winding
    for (uint8_t i = 0; i < 3; i++)
    {
        this->_windings[i]->Update(timeStep);

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
    le_Element::Connect(e, outputSlot, this, 0);
}

/**
 * @brief Sets the input for Winding B.
 * @param e The element providing the input value for Winding B.
 * @param outputSlot The output slot of the element providing the input.
 */
void le_Analog3PWinding::SetInput_WindingB(le_Base<float>* e, uint8_t outputSlot)
{
    le_Element::Connect(e, outputSlot, this, 1);
}

/**
 * @brief Sets the input for Winding C.
 * @param e The element providing the input value for Winding C.
 * @param outputSlot The output slot of the element providing the input.
 */
void le_Analog3PWinding::SetInput_WindingC(le_Base<float>* e, uint8_t outputSlot)
{
    le_Element::Connect(e, outputSlot, this, 2);
}

/**
 * @brief Sets the real part of the reference input for the winding.
 * @param e The element providing the reference real input value.
 * @param outputSlot The output slot of the element providing the reference real input.
 */
void le_Analog3PWinding::SetInput_RefReal(le_Base<float>* e, uint8_t outputSlot)
{
    le_Element::Connect(e, outputSlot, this, 3);
}

/**
 * @brief Sets the imaginary part of the reference input for the winding.
 * @param e The element providing the reference imaginary input value.
 * @param outputSlot The output slot of the element providing the reference imaginary input.
 */
void le_Analog3PWinding::SetInput_RefImag(le_Base<float>* e, uint8_t outputSlot)
{
    le_Element::Connect(e, outputSlot, this, 4);
}

/**
 * @brief Verify inputs to all Analog1PWindings.
 */
void le_Analog3PWinding::VerifyInputs()
{
    if (this->bInputsVerified)
        return;

    // Check reference signal inputs
    le_Base<float>* refReal = (le_Base<float>*)this->_inputs[3];
    le_Base<float>* refImag = (le_Base<float>*)this->_inputs[4];

    // Verify inputs for phases
    for (uint8_t phase = 0; phase < 3; phase++)
    {
        le_Base<float>* phaseInput = (le_Base<float>*)this->_inputs[phase];
        le_Base<float>* phaseWinding = (le_Base<float>*)this->_windings[phase];
        if (phaseInput != nullptr)
        {
            le_Element::Connect(phaseInput, this->_outputSlots[phase], phaseWinding, phase);
        }
        if (refReal != nullptr && refImag != nullptr)
        {
            le_Element::Connect(refReal, this->_outputSlots[3], phaseWinding, 1);
            le_Element::Connect(refImag, this->_outputSlots[4], phaseWinding, 2);
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
    std::complex<float> a = this->a;
    std::complex<float> a2 = this->a2;
    std::complex<float> vA = std::complex<float>(this->GetValue(0), this->GetValue(1));
    std::complex<float> vB = std::complex<float>(this->GetValue(2), this->GetValue(3));
    std::complex<float> vC = std::complex<float>(this->GetValue(4), this->GetValue(5));

    // Calculate sequence components
    std::complex<float> v0 = vA + vB + vC;
    std::complex<float> v1 = (vA + a * vB + a2 * vC) / 3.0f;
    std::complex<float> v2 = (vA + a2 * vB + a * vC) / 3.0f;

    // Save sequence components
    this->SetValue(6, v0.real());  // Zero-Sequence (Real)
    this->SetValue(7, v0.imag());  // Zero-Sequence (Imag)
    this->SetValue(8, v1.real());  // Positive-Sequence (Real)
    this->SetValue(9, v1.imag()); // Positive-Sequence (Imag)
    this->SetValue(10, v2.real()); // Negative-Sequence (Real)
    this->SetValue(11, v2.imag()); // Negative-Sequence (Imag)
}

const std::complex<float> le_Analog3PWinding::a = std::polar(1.0f, (float)(2.0 * M_PI / 3.0)); ///< Constant phasor component.
const std::complex<float> le_Analog3PWinding::a2 = a * a;
