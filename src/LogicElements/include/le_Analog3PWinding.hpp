#pragma once
#include "le_Element.hpp"
#include "le_Analog1PWinding.hpp"

namespace LogicElements {

#ifdef LE_ELEMENTS_ANALOG

#ifdef LE_ELEMENTS_ANALOG_COMPLEX
class Analog3PWinding : public Element {
private:
    Analog1PWinding* _windings[3]; bool bInputsVerified;
    void VerifyInputs(); void CalculateSequenceComponents();
LE_ELEMENT_ACCESS_MOD:
    Analog3PWinding(uint16_t samplesPerCycle);
    ~Analog3PWinding();
    void Update(const Time& timeStamp) override;
public:
    void SetInput_WindingA(Element* sourceElement, const std::string& sourcePortName);
    void SetInput_WindingB(Element* sourceElement, const std::string& sourcePortName);
    void SetInput_WindingC(Element* sourceElement, const std::string& sourcePortName);
    void SetInput_Ref(Element* sourceElement, const std::string& sourcePortName);
    std::complex<float> GetPhaseA() const; std::complex<float> GetPhaseB() const; std::complex<float> GetPhaseC() const;
    std::complex<float> GetSeq0() const; std::complex<float> GetSeq1() const; std::complex<float> GetSeq2() const;
private:
    InputPort<float>* pRawA; InputPort<float>* pRawB; InputPort<float>* pRawC;
    InputPort<std::complex<float>>* pRef;
    OutputPort<std::complex<float>>* pPhaseA; OutputPort<std::complex<float>>* pPhaseB; OutputPort<std::complex<float>>* pPhaseC;
    OutputPort<std::complex<float>>* pSeq0; OutputPort<std::complex<float>>* pSeq1; OutputPort<std::complex<float>>* pSeq2;
    friend class Engine;
};
#else
class Analog3PWinding : public Element {
private:
    Analog1PWinding* _windings[3]; bool bInputsVerified;
    void VerifyInputs(); void CalculateSequenceComponents();
LE_ELEMENT_ACCESS_MOD:
    Analog3PWinding(uint16_t samplesPerCycle);
    ~Analog3PWinding();
    void Update(const Time& timeStamp) override;
public:
    void SetInput_WindingA(Element* sourceElement, const std::string& sourcePortName);
    void SetInput_WindingB(Element* sourceElement, const std::string& sourcePortName);
    void SetInput_WindingC(Element* sourceElement, const std::string& sourcePortName);
    void SetInput_ReferenceReal(Element* sourceElement, const std::string& sourcePortName);
    void SetInput_ReferenceImag(Element* sourceElement, const std::string& sourcePortName);
    float GetPhaseA_Real() const; float GetPhaseA_Imag() const; float GetPhaseB_Real() const; 
    float GetPhaseB_Imag() const; float GetPhaseC_Real() const; float GetPhaseC_Imag() const;
    float GetSeq0_Real() const; float GetSeq0_Imag() const; float GetSeq1_Real() const;
    float GetSeq1_Imag() const; float GetSeq2_Real() const; float GetSeq2_Imag() const;
private:
    InputPort<float>* pRawA; InputPort<float>* pRawB; InputPort<float>* pRawC;
    InputPort<float>* pRefReal; InputPort<float>* pRefImag;
    OutputPort<float>* pPhaseA_Real; OutputPort<float>* pPhaseA_Imag;
    OutputPort<float>* pPhaseB_Real; OutputPort<float>* pPhaseB_Imag;
    OutputPort<float>* pPhaseC_Real; OutputPort<float>* pPhaseC_Imag;
    OutputPort<float>* pSeq0_Real; OutputPort<float>* pSeq0_Imag;
    OutputPort<float>* pSeq1_Real; OutputPort<float>* pSeq1_Imag;
    OutputPort<float>* pSeq2_Real; OutputPort<float>* pSeq2_Imag;
    friend class Engine;
};
#endif
#endif

} // namespace LogicElements
