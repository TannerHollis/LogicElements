#include "le_Polar2Complex.hpp"
#ifdef LE_ELEMENTS_ANALOG_COMPLEX
#include <cmath>

namespace LogicElements {

Polar2Complex::Polar2Complex() : Element(ElementType::Polar2Complex) {
    pMagnitude = AddInputPort<float>("magnitude");
    pAngle = AddInputPort<float>("angle");
    pComplex = AddOutputPort<std::complex<float>>("complex");
}
void Polar2Complex::Update(const Time& timeStamp) {
    UNUSED(timeStamp);
    if (pMagnitude->IsConnected() && pAngle->IsConnected()) {
        float mag = pMagnitude->GetValue();
        float angle = pAngle->GetValue() / 180.0f * (float)M_PI;
        pComplex->SetValue(std::polar(mag, angle));
    }
}
void Polar2Complex::SetInput_Mag(Element* sourceElement, const std::string& sourcePortName) {
    Element::Connect(sourceElement, sourcePortName, this, "magnitude");
}
void Polar2Complex::SetInput_Angle(Element* sourceElement, const std::string& sourcePortName) {
    Element::Connect(sourceElement, sourcePortName, this, "angle");
}
std::complex<float> Polar2Complex::GetComplex() const { return pComplex->GetValue(); }
#endif

} // namespace LogicElements
