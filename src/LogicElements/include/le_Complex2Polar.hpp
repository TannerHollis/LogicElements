#pragma once
#include "le_Element.hpp"

namespace LogicElements {

#ifdef LE_ELEMENTS_ANALOG_COMPLEX
class Complex2Polar : public Element {
LE_ELEMENT_ACCESS_MOD:
    Complex2Polar();
    void Update(const Time& timeStamp) override;
public:
    void SetInput(Element* sourceElement, const std::string& sourcePortName);
    float GetMagnitude() const;
    float GetAngle() const;
private:
    InputPort<std::complex<float>>* pComplex;
    OutputPort<float>* pMagnitude;
    OutputPort<float>* pAngle;
    friend class Engine;
};
#endif

} // namespace LogicElements
