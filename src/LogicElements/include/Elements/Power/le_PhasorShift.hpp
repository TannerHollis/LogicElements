#pragma once
#include "Core/le_Element.hpp"

namespace LogicElements {

#ifdef LE_ELEMENTS_ANALOG

#ifdef LE_ELEMENTS_ANALOG_COMPLEX
class PhasorShift : public Element {
LE_ELEMENT_ACCESS_MOD:
    PhasorShift(float shiftMagnitude, float shiftAngleClockwise);
    void Update(const Time& timeStamp) override;
public:
    void SetInput(Element* sourceElement, const std::string& sourcePortName);
    std::complex<float> GetOutput() const;
private:
    InputPort<std::complex<float>>* pInput;
    OutputPort<std::complex<float>>* pOutput;
    float shiftMag, shiftAngle, unitShiftReal, unitShiftImag;
    friend class Engine;
};
#else
class PhasorShift : public Element {
LE_ELEMENT_ACCESS_MOD:
    PhasorShift(float shiftMagnitude, float shiftAngleClockwise);
    void Update(const Time& timeStamp) override;
public:
    void SetInput_Real(Element* sourceElement, const std::string& sourcePortName);
    void SetInput_Imag(Element* sourceElement, const std::string& sourcePortName);
    float GetReal() const;
    float GetImag() const;
private:
    InputPort<float>* pReal;
    InputPort<float>* pImag;
    OutputPort<float>* pOutReal;
    OutputPort<float>* pOutImag;
    float shiftMag, shiftAngle, unitShiftReal, unitShiftImag;
    friend class Engine;
};
#endif
#endif

} // namespace LogicElements
