#pragma once
#include "le_Element.hpp"

namespace LogicElements {

#ifdef LE_ELEMENTS_ANALOG

#ifdef LE_ELEMENTS_ANALOG_COMPLEX
class Analog1PWinding : public Element {
private:
    struct CosineFilter { float* coefficients; uint16_t length; } sFilter;
    float* _rawValues; float* _rawFilteredValues;
    uint16_t uSamplesPerCycle, uWrite, uQuarterCycle;
    void ApplyCosineFilter(); void CalculatePhasor(); void AdjustOutputAngleWithReference();
LE_ELEMENT_ACCESS_MOD:
    Analog1PWinding(uint16_t samplesPerCycle);
    ~Analog1PWinding();
    void Update(const Time& timeStamp) override;
public:
    void SetInput_Winding(Element* sourceElement, const std::string& sourcePortName);
    void SetInput_Reference(Element* sourceElement, const std::string& sourcePortName);
    std::complex<float> GetOutput() const;
private:
    InputPort<float>* pRaw; InputPort<std::complex<float>>* pRef; OutputPort<std::complex<float>>* pOutput;
    friend class Engine;
};
#else
class Analog1PWinding : public Element {
private:
    struct CosineFilter { float* coefficients; uint16_t length; } sFilter;
    float* _rawValues; float* _rawFilteredValues;
    uint16_t uSamplesPerCycle, uWrite, uQuarterCycle;
    void ApplyCosineFilter(); void CalculatePhasor(); void AdjustOutputAngleWithReference();
LE_ELEMENT_ACCESS_MOD:
    Analog1PWinding(uint16_t samplesPerCycle);
    ~Analog1PWinding();
    void Update(const Time& timeStamp) override;
public:
    void SetInput_Winding(Element* sourceElement, const std::string& sourcePortName);
    void SetInput_ReferenceReal(Element* sourceElement, const std::string& sourcePortName);
    void SetInput_ReferenceImag(Element* sourceElement, const std::string& sourcePortName);
    float GetReal() const; float GetImag() const;
private:
    InputPort<float>* pRaw; InputPort<float>* pRefReal; InputPort<float>* pRefImag;
    OutputPort<float>* pOutReal; OutputPort<float>* pOutImag;
    friend class Engine;
};
#endif
#endif

} // namespace LogicElements
