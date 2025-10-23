#include "Elements/Power/le_Analog1PWinding.hpp"
#ifdef LE_ELEMENTS_ANALOG
#include <cmath>

namespace LogicElements {

#ifdef LE_ELEMENTS_ANALOG_COMPLEX
Analog1PWinding::Analog1PWinding(uint16_t samplesPerCycle) : Element(ElementType::Analog1PWinding) {
    pRaw = AddInputPort<float>("raw"); pRef = AddInputPort<std::complex<float>>("reference");
    pOutput = AddOutputPort<std::complex<float>>(LE_PORT_OUTPUT_PREFIX);
    uSamplesPerCycle = samplesPerCycle; _rawValues = new float[samplesPerCycle]; _rawFilteredValues = new float[samplesPerCycle];
    for (uint16_t i = 0; i < samplesPerCycle; i++) { _rawValues[i] = 0.0f; _rawFilteredValues[i] = 0.0f; }
    uWrite = samplesPerCycle - 1; uQuarterCycle = (uint16_t)((float)samplesPerCycle / 4.0f) - 1;
    sFilter.coefficients = new float[samplesPerCycle]; sFilter.length = samplesPerCycle;
    for (uint16_t i = 0; i < samplesPerCycle; i++)
        sFilter.coefficients[i] = (float)(2.0 / (double)samplesPerCycle * cos(2.0 * (double)M_PI / (double)samplesPerCycle * (double)i));
}
Analog1PWinding::~Analog1PWinding() { delete[] _rawValues; delete[] _rawFilteredValues; delete[] sFilter.coefficients; }
void Analog1PWinding::Update(const Time& timeStamp) {
    UNUSED(timeStamp); ApplyCosineFilter(); CalculatePhasor(); AdjustOutputAngleWithReference();
    uWrite = (uWrite - 1 + uSamplesPerCycle) % uSamplesPerCycle; uQuarterCycle = (uQuarterCycle - 1 + uSamplesPerCycle) % uSamplesPerCycle;
}
void Analog1PWinding::SetInput_Winding(Element* sourceElement, const std::string& sourcePortName) {
    Element::Connect(sourceElement, sourcePortName, this, "raw");
}
void Analog1PWinding::SetInput_Reference(Element* sourceElement, const std::string& sourcePortName) {
    Element::Connect(sourceElement, sourcePortName, this, "reference");
}
std::complex<float> Analog1PWinding::GetOutput() const { return pOutput->GetValue(); }
void Analog1PWinding::ApplyCosineFilter() {
    if (pRaw->IsConnected()) _rawValues[uWrite] = pRaw->GetValue();
    float sum = 0.0f;
    for (uint16_t i = 0; i < uSamplesPerCycle; i++)
        sum += _rawValues[(uWrite + i) % uSamplesPerCycle] * sFilter.coefficients[i];
    _rawFilteredValues[uWrite] = sum;
}
void Analog1PWinding::CalculatePhasor() {
    float real = _rawFilteredValues[uWrite]; float imag = -_rawFilteredValues[uQuarterCycle];
    pOutput->SetValue(std::complex<float>(real, imag));
}
void Analog1PWinding::AdjustOutputAngleWithReference() {
    if (pRef->IsConnected()) {
        std::complex<float> ref = pRef->GetValue(); std::complex<float> out = pOutput->GetValue();
        float refAngle = std::arg(ref); float outMag = std::abs(out); float outAngle = std::arg(out);
        pOutput->SetValue(std::polar(outMag, outAngle - refAngle));
    }
}
#else
Analog1PWinding::Analog1PWinding(uint16_t samplesPerCycle) : Element(ElementType::Analog1PWinding) {
    pRaw = AddInputPort<float>("raw"); pRefReal = AddInputPort<float>("reference_real"); pRefImag = AddInputPort<float>("reference_imag");
    pOutReal = AddOutputPort<float>("real"); pOutImag = AddOutputPort<float>("imaginary");
    uSamplesPerCycle = samplesPerCycle; _rawValues = new float[samplesPerCycle]; _rawFilteredValues = new float[samplesPerCycle];
    for (uint16_t i = 0; i < samplesPerCycle; i++) { _rawValues[i] = 0.0f; _rawFilteredValues[i] = 0.0f; }
    uWrite = samplesPerCycle - 1; uQuarterCycle = (uint16_t)((float)samplesPerCycle / 4.0f) - 1;
    sFilter.coefficients = new float[samplesPerCycle]; sFilter.length = samplesPerCycle;
    for (uint16_t i = 0; i < samplesPerCycle; i++)
        sFilter.coefficients[i] = (float)(2.0 / (double)samplesPerCycle * cos(2.0 * (double)M_PI / (double)samplesPerCycle * (double)i));
}
Analog1PWinding::~Analog1PWinding() { delete[] _rawValues; delete[] _rawFilteredValues; delete[] sFilter.coefficients; }
void Analog1PWinding::Update(const Time& timeStamp) {
    UNUSED(timeStamp); ApplyCosineFilter(); CalculatePhasor(); AdjustOutputAngleWithReference();
    uWrite = (uWrite - 1 + uSamplesPerCycle) % uSamplesPerCycle; uQuarterCycle = (uQuarterCycle - 1 + uSamplesPerCycle) % uSamplesPerCycle;
}
void Analog1PWinding::SetInput_Winding(Element* sourceElement, const std::string& sourcePortName) {
    Element::Connect(sourceElement, sourcePortName, this, "raw");
}
void Analog1PWinding::SetInput_ReferenceReal(Element* sourceElement, const std::string& sourcePortName) {
    Element::Connect(sourceElement, sourcePortName, this, "reference_real");
}
void Analog1PWinding::SetInput_ReferenceImag(Element* sourceElement, const std::string& sourcePortName) {
    Element::Connect(sourceElement, sourcePortName, this, "reference_imag");
}
float Analog1PWinding::GetReal() const { return pOutReal->GetValue(); }
float Analog1PWinding::GetImag() const { return pOutImag->GetValue(); }
void Analog1PWinding::ApplyCosineFilter() {
    if (pRaw->IsConnected()) _rawValues[uWrite] = pRaw->GetValue();
    float sum = 0.0f;
    for (uint16_t i = 0; i < uSamplesPerCycle; i++)
        sum += _rawValues[(uWrite + i) % uSamplesPerCycle] * sFilter.coefficients[i];
    _rawFilteredValues[uWrite] = sum;
}
void Analog1PWinding::CalculatePhasor() {
    float real = _rawFilteredValues[uWrite]; float imag = -_rawFilteredValues[uQuarterCycle];
    pOutReal->SetValue(real); pOutImag->SetValue(imag);
}
void Analog1PWinding::AdjustOutputAngleWithReference() {
    if (pRefReal->IsConnected() && pRefImag->IsConnected()) {
        float refReal = pRefReal->GetValue(); float refImag = pRefImag->GetValue();
        float outReal = pOutReal->GetValue(); float outImag = pOutImag->GetValue();
        float refAngle = atan2f(refImag, refReal); float outMag = sqrtf(outReal*outReal + outImag*outImag);
        float outAngle = atan2f(outImag, outReal);
        float adjustedAngle = outAngle - refAngle;
        pOutReal->SetValue(outMag * cosf(adjustedAngle)); pOutImag->SetValue(outMag * sinf(adjustedAngle));
    }
}
#endif
#endif

} // namespace LogicElements
