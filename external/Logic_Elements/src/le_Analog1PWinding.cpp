#include "le_Analog1PWinding.hpp"

#ifdef LE_ELEMENTS_ANALOG

/**
 * @brief Constructor that initializes the le_Analog1PWinding with specified samples per cycle.
 * @param samplesPerCycle Number of samples per cycle.
 */
#ifdef LE_ELEMENTS_ANALOG_COMPLEX
le_Analog1PWinding::le_Analog1PWinding(uint16_t samplesPerCycle) : le_Base<std::complex<float>>(le_Element_Type::LE_ANALOG_1P, 2, 1)
#else
le_Analog1PWinding::le_Analog1PWinding(uint16_t samplesPerCycle) : le_Base<float>(le_Element_Type::LE_ANALOG_1P, 3, 2)
#endif
{
    // Set extrinsic variables
    this->uSamplesPerCycle = samplesPerCycle;

    // Set intrinsic variables
    this->_rawValues = new float[samplesPerCycle];
    this->_rawFilteredValues = new float[samplesPerCycle];

    // Clear arrays
    for (uint16_t i = 0; i < samplesPerCycle; i++)
    {
        this->_rawValues[i] = 0.0f;
        this->_rawFilteredValues[i] = 0.0f;
    }

    this->uWrite = samplesPerCycle - 1;
    this->uQuarterCycle = (uint16_t)((float)samplesPerCycle / 4.0f) - 1; // Designate read head as: (2pi_samples / 4)

    // Initialize and calculate cosine filter coefficients
    this->sFilter.coefficients = new float[samplesPerCycle];
    this->sFilter.length = samplesPerCycle;
    for (uint16_t i = 0; i < samplesPerCycle; i++)
    {
        this->sFilter.coefficients[i] = (float)(2.0 / (double)samplesPerCycle * cos(2.0 * (double)M_PI / (double)samplesPerCycle * (double)i));
    }
}

/**
 * @brief Destructor to clean up the le_Analog1PWinding.
 */
le_Analog1PWinding::~le_Analog1PWinding()
{
    // Deallocate arrays
    delete[] this->_rawValues;
    delete[] this->_rawFilteredValues;
    delete[] this->sFilter.coefficients;
}

/**
 * @brief Updates the le_Analog1PWinding.
 * @param timeStamp The current timestamp.
 */
void le_Analog1PWinding::Update(const le_Time& timeStamp)
{
    UNUSED(timeStamp);

    ApplyCosineFilter();
    CalculatePhasor();
    AdjustOutputAngleWithReference();

    this->uWrite = (this->uWrite - 1 + this->uSamplesPerCycle) % this->uSamplesPerCycle;
    this->uQuarterCycle = (this->uQuarterCycle - 1 + this->uSamplesPerCycle) % this->uSamplesPerCycle;
}

/**
 * @brief Sets the input for Winding.
 * @param e The element providing the input value for Winding.
 * @param outputSlot The output slot of the element providing the input.
 */
void le_Analog1PWinding::SetInput_Winding(le_Base<float>* e, uint8_t outputSlot)
{
    le_Element::Connect(e, outputSlot, this, IO_A1P_INPUT_RAW);
}

#ifdef LE_ELEMENTS_ANALOG_COMPLEX
/**
 * @brief Sets the reference input for the winding.
 * @param e The element providing the reference real input value.
 * @param outputSlot The output slot of the element providing the reference input.
 */
void le_Analog1PWinding::SetInput_Ref(le_Base<std::complex<float>>* e, uint8_t outputSlot)
{
    le_Element::Connect(e, outputSlot, this, IO_A1P_INPUT_REF);
}
#else
/**
 * @brief Sets the real part of the reference input for the winding.
 * @param e The element providing the reference real input value.
 * @param outputSlot The output slot of the element providing the reference real input.
 */
void le_Analog1PWinding::SetInput_RefReal(le_Base<float>* e, uint8_t outputSlot)
{
    le_Element::Connect(e, outputSlot, this, IO_A1P_INPUT_REF_REAL);
}

/**
 * @brief Sets the imaginary part of the reference input for the winding.
 * @param e The element providing the reference imaginary input value.
 * @param outputSlot The output slot of the element providing the reference imaginary input.
 */
void le_Analog1PWinding::SetInput_RefImag(le_Base<float>* e, uint8_t outputSlot)
{
    le_Element::Connect(e, outputSlot, this, IO_A1P_INPUT_REF_IMAG);
}
#endif

/**
 * @brief Calculates the phasor for the winding.
 */
void le_Analog1PWinding::CalculatePhasor()
{
    // Calculate phasor using 90 degree offset value
#ifdef LE_ELEMENTS_ANALOG_COMPLEX
    this->SetValue(IO_A1P_OUTPUT, std::complex<float>(
        _rawFilteredValues[this->uWrite],
        _rawFilteredValues[this->uQuarterCycle]));        // Write complex component
#else
    this->SetValue(IO_A1P_OUTPUT_REAL, _rawFilteredValues[this->uWrite]);        // Write real component
    this->SetValue(IO_A1P_OUTPUT_IMAG, _rawFilteredValues[this->uQuarterCycle]); // Write imaginary component
#endif
}

/**
 * @brief Applies a cosine filter to the raw values.
 */
void le_Analog1PWinding::ApplyCosineFilter()
{
    // Get the input element
    le_Base<float>* e = this->template GetInput<le_Base<float>>(IO_A1P_INPUT_RAW);
    if (e != nullptr)
    {
        float sum = 0.0f;
        uint16_t pos = this->uWrite;

        // Store rawInput to raw values buffer
        this->_rawValues[this->uWrite] = e->GetValue(this->GetOutputSlot(IO_A1P_INPUT_RAW));

        // Calculate Cosine Filter sum
        // Manual wrap-around to avoid modulo operation in every iteration
        uint16_t samplesPerCycle = this->uSamplesPerCycle;
        float* rawValues = this->_rawValues;
        float* coefficients = this->sFilter.coefficients;

        for (uint16_t i = 0; i < samplesPerCycle; ++i)
        {
            sum += rawValues[pos] * coefficients[i];
            pos++;
            if (pos >= samplesPerCycle)
                pos = 0; // Wrap around
        }

        // Scale sum and store in filtered values buffer
        this->_rawFilteredValues[this->uWrite] = sum;
    }
}

/**
* @brief Applies an angular shift with reference to the reference input.
*/
void le_Analog1PWinding::AdjustOutputAngleWithReference()
{
    // Get reference signal elements
#ifdef LE_ELEMENTS_ANALOG_COMPLEX
    le_Base<std::complex<float>>* e = this->template GetInput<le_Base<std::complex<float>>>(IO_A1P_INPUT_REF);
    if(e != nullptr)
    {
        float refReal = e->GetValue(this->GetOutputSlot(IO_A1P_INPUT_REF)).real();
        float refImag = e->GetValue(this->GetOutputSlot(IO_A1P_INPUT_REF)).imag();
#else
    le_Base<float>* eReal = this->template GetInput<le_Base<float>>(IO_A1P_INPUT_REF_REAL);
    le_Base<float>* eImag = this->template GetInput<le_Base<float>>(IO_A1P_INPUT_REF_IMAG);
    if (eReal != nullptr && eImag != nullptr)
    {
        // Get reference signal components
        float refReal = eReal->GetValue(this->GetOutputSlot(IO_A1P_INPUT_REF_REAL));
        float refImag = eImag->GetValue(this->GetOutputSlot(IO_A1P_INPUT_REF_IMAG));
#endif
        // Check domain error
        if (refReal == 0.0f && refImag == 0.0f)
            return;

        // Normalize the reference vector to a unit vector
        float refMag = std::sqrt(refReal * refReal + refImag * refImag);
        float refUnitReal = refReal / refMag;
        float refUnitImag = refImag / refMag;
#ifdef LE_ELEMENTS_ANALOG_COMPLEX
        float real = this->GetValue(IO_A1P_INPUT_REF).real();
        float imag = this->GetValue(IO_A1P_INPUT_REF).imag();
#else
        // Get current real and imaginary parts
        float real = this->GetValue(IO_A1P_OUTPUT_REAL);
        float imag = this->GetValue(IO_A1P_OUTPUT_IMAG);
#endif
        // Check domain error
        if (real == 0.0f && imag == 0.0f)
            return;

        // Rotate the vector by the negative of the reference angle
        float newReal = real * refUnitReal + imag * refUnitImag;
        float newImag = imag * refUnitReal - real * refUnitImag;

        // Set the new values
#ifdef LE_ELEMENTS_ANALOG_COMPLEX
        this->SetValue(IO_A1P_OUTPUT, std::complex<float>(newReal, newImag));
#else
        this->SetValue(IO_A1P_OUTPUT_REAL, newReal);
        this->SetValue(IO_A1P_OUTPUT_IMAG, newImag);
#endif
    }
}
#endif