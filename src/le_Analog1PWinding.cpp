#include "le_Analog1PWinding.hpp"

#define IO_INPUT_RAW 0
#define IO_INPUT_REF_REAL 1
#define IO_INPUT_REF_IMAG 2
#define IO_OUTPUT_REAL 0
#define IO_OUTPUT_IMAG 1

/**
 * @brief Constructor that initializes the le_Analog1PWinding with specified samples per cycle.
 * @param samplesPerCycle Number of samples per cycle.
 */
le_Analog1PWinding::le_Analog1PWinding(uint16_t samplesPerCycle) : le_Base<float>(3, 2)
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
void le_Analog1PWinding::Update(float timeStep)
{
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
    le_Element::Connect(e, outputSlot, this, IO_INPUT_RAW);
}

/**
 * @brief Sets the real part of the reference input for the winding.
 * @param e The element providing the reference real input value.
 * @param outputSlot The output slot of the element providing the reference real input.
 */
void le_Analog1PWinding::SetInput_RefReal(le_Base<float>* e, uint8_t outputSlot)
{
    le_Element::Connect(e, outputSlot, this, IO_INPUT_REF_REAL);
}

/**
 * @brief Sets the imaginary part of the reference input for the winding.
 * @param e The element providing the reference imaginary input value.
 * @param outputSlot The output slot of the element providing the reference imaginary input.
 */
void le_Analog1PWinding::SetInput_RefImag(le_Base<float>* e, uint8_t outputSlot)
{
    le_Element::Connect(e, outputSlot, this, IO_INPUT_REF_IMAG);
}

/**
 * @brief Calculates the phasor for the winding.
 */
void le_Analog1PWinding::CalculatePhasor()
{
    // Calculate phasor using 90 degree offset value
    this->SetValue(0, _rawFilteredValues[this->uWrite]);        // Write real component
    this->SetValue(1, _rawFilteredValues[this->uQuarterCycle]); // Write imaginary component
}

/**
 * @brief Applies a cosine filter to the raw values.
 */
void le_Analog1PWinding::ApplyCosineFilter()
{
    // Get the input element
    le_Base<float>* e = (le_Base<float>*)this->_inputs[IO_INPUT_RAW];
    if (e != nullptr)
    {
        float sum = 0.0f;
        uint16_t pos = this->uWrite;

        // Store rawInput to raw values buffer
        this->_rawValues[this->uWrite] = e->GetValue(this->_outputSlots[IO_INPUT_RAW]);

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
    le_Base<float>* eReal = (le_Base<float>*)this->_inputs[IO_INPUT_REF_REAL];
    le_Base<float>* eImag = (le_Base<float>*)this->_inputs[IO_INPUT_REF_IMAG];
    if (eReal != nullptr && eImag != nullptr)
    {
        // Get reference signal components
        float refReal = eReal->GetValue(this->_outputSlots[IO_INPUT_REF_REAL]);
        float refImag = eImag->GetValue(this->_outputSlots[IO_INPUT_REF_IMAG]);

        // Check domain error
        if (refReal == 0.0f && refImag == 0.0f)
            return;

        // Normalize the reference vector to a unit vector
        float refMag = std::sqrt(refReal * refReal + refImag * refImag);
        float refUnitReal = refReal / refMag;
        float refUnitImag = refImag / refMag;

        // Get current real and imaginary parts
        float real = this->GetValue(IO_OUTPUT_REAL);
        float imag = this->GetValue(IO_OUTPUT_IMAG);

        // Check domain error
        if (real == 0.0f && imag == 0.0f)
            return;

        // Rotate the vector by the negative of the reference angle
        float newReal = real * refUnitReal + imag * refUnitImag;
        float newImag = imag * refUnitReal - real * refUnitImag;

        // Set the new values
        this->SetValue(IO_OUTPUT_REAL, newReal);
        this->SetValue(IO_OUTPUT_IMAG, newImag);
    }
}

