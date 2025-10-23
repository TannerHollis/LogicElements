#include "le_Analog3PWinding.hpp"
#ifdef LE_ELEMENTS_ANALOG
#include <cmath>

namespace LogicElements {

#ifdef LE_ELEMENTS_ANALOG_COMPLEX
Analog3PWinding::Analog3PWinding(uint16_t samplesPerCycle) : Element(ElementType::Analog3PWinding), bInputsVerified(false) {
    pRawA = AddInputPort<float>("raw_a"); pRawB = AddInputPort<float>("raw_b"); pRawC = AddInputPort<float>("raw_c");
    pRef = AddInputPort<std::complex<float>>("reference");
    pPhaseA = AddOutputPort<std::complex<float>>("phase_a"); pPhaseB = AddOutputPort<std::complex<float>>("phase_b"); pPhaseC = AddOutputPort<std::complex<float>>("phase_c");
    pSeq0 = AddOutputPort<std::complex<float>>("seq_0"); pSeq1 = AddOutputPort<std::complex<float>>("seq_1"); pSeq2 = AddOutputPort<std::complex<float>>("seq_2");
    for (uint8_t i = 0; i < 3; i++) _windings[i] = new Analog1PWinding(samplesPerCycle);
}
Analog3PWinding::~Analog3PWinding() { for (uint8_t i = 0; i < 3; i++) delete _windings[i]; }
void Analog3PWinding::Update(const Time& timeStamp) {
    VerifyInputs();
    for (uint8_t i = 0; i < 3; i++) _windings[i]->Update(timeStamp);
    pPhaseA->SetValue(_windings[0]->GetOutput()); pPhaseB->SetValue(_windings[1]->GetOutput()); pPhaseC->SetValue(_windings[2]->GetOutput());
    CalculateSequenceComponents();
}
void Analog3PWinding::SetInput_WindingA(Element* sourceElement, const std::string& sourcePortName) {
    Element::Connect(sourceElement, sourcePortName, this, "raw_a");
}
void Analog3PWinding::SetInput_WindingB(Element* sourceElement, const std::string& sourcePortName) {
    Element::Connect(sourceElement, sourcePortName, this, "raw_b");
}
void Analog3PWinding::SetInput_WindingC(Element* sourceElement, const std::string& sourcePortName) {
    Element::Connect(sourceElement, sourcePortName, this, "raw_c");
}
void Analog3PWinding::SetInput_Ref(Element* sourceElement, const std::string& sourcePortName) {
    Element::Connect(sourceElement, sourcePortName, this, "reference");
}
std::complex<float> Analog3PWinding::GetPhaseA() const { return pPhaseA->GetValue(); }
std::complex<float> Analog3PWinding::GetPhaseB() const { return pPhaseB->GetValue(); }
std::complex<float> Analog3PWinding::GetPhaseC() const { return pPhaseC->GetValue(); }
std::complex<float> Analog3PWinding::GetSeq0() const { return pSeq0->GetValue(); }
std::complex<float> Analog3PWinding::GetSeq1() const { return pSeq1->GetValue(); }
std::complex<float> Analog3PWinding::GetSeq2() const { return pSeq2->GetValue(); }
void Analog3PWinding::VerifyInputs() {
    if (!bInputsVerified && pRawA->IsConnected() && pRawB->IsConnected() && pRawC->IsConnected()) {
        _windings[0]->SetInput_Winding(pRawA->GetSource()->GetOwner(), "raw");
        _windings[1]->SetInput_Winding(pRawB->GetSource()->GetOwner(), "raw");
        _windings[2]->SetInput_Winding(pRawC->GetSource()->GetOwner(), "raw");
        if (pRef->IsConnected()) {
            for (uint8_t i = 0; i < 3; i++)
                _windings[i]->SetInput_Reference(pRef->GetSource()->GetOwner(), "reference");
        }
        bInputsVerified = true;
    }
}
void Analog3PWinding::CalculateSequenceComponents() {
    std::complex<float> a = _windings[0]->GetOutput(); std::complex<float> b = _windings[1]->GetOutput(); std::complex<float> c = _windings[2]->GetOutput();
    std::complex<float> alpha(cosf(2.0f * (float)M_PI / 3.0f), sinf(2.0f * (float)M_PI / 3.0f));
    std::complex<float> alpha2 = alpha * alpha;
    pSeq0->SetValue((a + b + c) / 3.0f);
    pSeq1->SetValue((a + alpha * b + alpha2 * c) / 3.0f);
    pSeq2->SetValue((a + alpha2 * b + alpha * c) / 3.0f);
}
#else
Analog3PWinding::Analog3PWinding(uint16_t samplesPerCycle) : Element(ElementType::Analog3PWinding), bInputsVerified(false) {
    pRawA = AddInputPort<float>("raw_a"); pRawB = AddInputPort<float>("raw_b"); pRawC = AddInputPort<float>("raw_c");
    pRefReal = AddInputPort<float>("reference_real"); pRefImag = AddInputPort<float>("reference_imag");
    pPhaseA_Real = AddOutputPort<float>("phase_a_real"); pPhaseA_Imag = AddOutputPort<float>("phase_a_imag");
    pPhaseB_Real = AddOutputPort<float>("phase_b_real"); pPhaseB_Imag = AddOutputPort<float>("phase_b_imag");
    pPhaseC_Real = AddOutputPort<float>("phase_c_real"); pPhaseC_Imag = AddOutputPort<float>("phase_c_imag");
    pSeq0_Real = AddOutputPort<float>("seq_0_real"); pSeq0_Imag = AddOutputPort<float>("seq_0_imag");
    pSeq1_Real = AddOutputPort<float>("seq_1_real"); pSeq1_Imag = AddOutputPort<float>("seq_1_imag");
    pSeq2_Real = AddOutputPort<float>("seq_2_real"); pSeq2_Imag = AddOutputPort<float>("seq_2_imag");
    for (uint8_t i = 0; i < 3; i++) _windings[i] = new Analog1PWinding(samplesPerCycle);
}
Analog3PWinding::~Analog3PWinding() { for (uint8_t i = 0; i < 3; i++) delete _windings[i]; }
void Analog3PWinding::Update(const Time& timeStamp) {
    VerifyInputs();
    for (uint8_t i = 0; i < 3; i++) _windings[i]->Update(timeStamp);
    pPhaseA_Real->SetValue(_windings[0]->GetReal()); pPhaseA_Imag->SetValue(_windings[0]->GetImag());
    pPhaseB_Real->SetValue(_windings[1]->GetReal()); pPhaseB_Imag->SetValue(_windings[1]->GetImag());
    pPhaseC_Real->SetValue(_windings[2]->GetReal()); pPhaseC_Imag->SetValue(_windings[2]->GetImag());
    CalculateSequenceComponents();
}
void Analog3PWinding::SetInput_WindingA(Element* sourceElement, const std::string& sourcePortName) {
    Element::Connect(sourceElement, sourcePortName, this, "raw_a");
}
void Analog3PWinding::SetInput_WindingB(Element* sourceElement, const std::string& sourcePortName) {
    Element::Connect(sourceElement, sourcePortName, this, "raw_b");
}
void Analog3PWinding::SetInput_WindingC(Element* sourceElement, const std::string& sourcePortName) {
    Element::Connect(sourceElement, sourcePortName, this, "raw_c");
}
void Analog3PWinding::SetInput_ReferenceReal(Element* sourceElement, const std::string& sourcePortName) {
    Element::Connect(sourceElement, sourcePortName, this, "reference_real");
}
void Analog3PWinding::SetInput_ReferenceImag(Element* sourceElement, const std::string& sourcePortName) {
    Element::Connect(sourceElement, sourcePortName, this, "reference_imag");
}
float Analog3PWinding::GetPhaseA_Real() const { return pPhaseA_Real->GetValue(); }
float Analog3PWinding::GetPhaseA_Imag() const { return pPhaseA_Imag->GetValue(); }
float Analog3PWinding::GetPhaseB_Real() const { return pPhaseB_Real->GetValue(); }
float Analog3PWinding::GetPhaseB_Imag() const { return pPhaseB_Imag->GetValue(); }
float Analog3PWinding::GetPhaseC_Real() const { return pPhaseC_Real->GetValue(); }
float Analog3PWinding::GetPhaseC_Imag() const { return pPhaseC_Imag->GetValue(); }
float Analog3PWinding::GetSeq0_Real() const { return pSeq0_Real->GetValue(); }
float Analog3PWinding::GetSeq0_Imag() const { return pSeq0_Imag->GetValue(); }
float Analog3PWinding::GetSeq1_Real() const { return pSeq1_Real->GetValue(); }
float Analog3PWinding::GetSeq1_Imag() const { return pSeq1_Imag->GetValue(); }
float Analog3PWinding::GetSeq2_Real() const { return pSeq2_Real->GetValue(); }
float Analog3PWinding::GetSeq2_Imag() const { return pSeq2_Imag->GetValue(); }
void Analog3PWinding::VerifyInputs() {
    if (!bInputsVerified && pRawA->IsConnected() && pRawB->IsConnected() && pRawC->IsConnected()) {
        _windings[0]->SetInput_Winding(pRawA->GetSource()->GetOwner(), pRawA->GetSource()->GetName());
        _windings[1]->SetInput_Winding(pRawB->GetSource()->GetOwner(), pRawB->GetSource()->GetName());
        _windings[2]->SetInput_Winding(pRawC->GetSource()->GetOwner(), pRawC->GetSource()->GetName());
        if (pRefReal->IsConnected() && pRefImag->IsConnected()) {
            for (uint8_t i = 0; i < 3; i++) {
                _windings[i]->SetInput_ReferenceReal(pRefReal->GetSource()->GetOwner(), pRefReal->GetSource()->GetName());
                _windings[i]->SetInput_ReferenceImag(pRefImag->GetSource()->GetOwner(), pRefImag->GetSource()->GetName());
            }
        }
        bInputsVerified = true;
    }
}
void Analog3PWinding::CalculateSequenceComponents() {
    float aReal = _windings[0]->GetReal(); float aImag = _windings[0]->GetImag();
    float bReal = _windings[1]->GetReal(); float bImag = _windings[1]->GetImag();
    float cReal = _windings[2]->GetReal(); float cImag = _windings[2]->GetImag();
    float alpha_r = cosf(2.0f * (float)M_PI / 3.0f); float alpha_i = sinf(2.0f * (float)M_PI / 3.0f);
    float alpha2_r = cosf(4.0f * (float)M_PI / 3.0f); float alpha2_i = sinf(4.0f * (float)M_PI / 3.0f);
    pSeq0_Real->SetValue((aReal + bReal + cReal) / 3.0f); pSeq0_Imag->SetValue((aImag + bImag + cImag) / 3.0f);
    pSeq1_Real->SetValue((aReal + (alpha_r*bReal - alpha_i*bImag) + (alpha2_r*cReal - alpha2_i*cImag)) / 3.0f);
    pSeq1_Imag->SetValue((aImag + (alpha_r*bImag + alpha_i*bReal) + (alpha2_r*cImag + alpha2_i*cReal)) / 3.0f);
    pSeq2_Real->SetValue((aReal + (alpha2_r*bReal - alpha2_i*bImag) + (alpha_r*cReal - alpha_i*cImag)) / 3.0f);
    pSeq2_Imag->SetValue((aImag + (alpha2_r*bImag + alpha2_i*bReal) + (alpha_r*cImag + alpha_i*cReal)) / 3.0f);
}
#endif
#endif

} // namespace LogicElements
