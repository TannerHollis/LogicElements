#include "Elements/Conversions/le_Complex2Polar.hpp"
#ifdef LE_ELEMENTS_ANALOG_COMPLEX

namespace LogicElements {

Complex2Polar::Complex2Polar() : Element(ElementType::Complex2Polar) {
    pComplex = AddInputPort<std::complex<float>>("complex");
    pMagnitude = AddOutputPort<float>("magnitude");
    pAngle = AddOutputPort<float>("angle");
}
void Complex2Polar::Update(const Time& timeStamp) {
    UNUSED(timeStamp);
    if (pComplex->IsConnected()) {
        std::complex<float> c = pComplex->GetValue();
        pMagnitude->SetValue(std::abs(c));
        pAngle->SetValue(std::arg(c) * 180.0f / (float)M_PI);
    }
}
void Complex2Polar::SetInput(Element* sourceElement, const std::string& sourcePortName) {
    Element::Connect(sourceElement, sourcePortName, this, "complex");
}
float Complex2Polar::GetMagnitude() const { return pMagnitude->GetValue(); }
float Complex2Polar::GetAngle() const { return pAngle->GetValue(); }
#endif

} // namespace LogicElements
