#pragma once
#include "le_Element.hpp"

namespace LogicElements {

#ifdef LE_ELEMENTS_ANALOG_COMPLEX
class Polar2Complex : public Element {
LE_ELEMENT_ACCESS_MOD:
    Polar2Complex();
    void Update(const Time& timeStamp) override;
public:
    void SetInput_Mag(Element* sourceElement, const std::string& sourcePortName);
    void SetInput_Angle(Element* sourceElement, const std::string& sourcePortName);
    std::complex<float> GetComplex() const;
private:
    InputPort<float>* pMagnitude;
    InputPort<float>* pAngle;
    OutputPort<std::complex<float>>* pComplex;
    friend class Engine;
};
#endif

} // namespace LogicElements
