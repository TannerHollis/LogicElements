#include "Elements/Power/le_PhasorShift.hpp"
#ifdef LE_ELEMENTS_ANALOG
#include <cmath>

namespace LogicElements {

#ifdef LE_ELEMENTS_ANALOG_COMPLEX
PhasorShift::PhasorShift(float shiftMagnitude, float shiftAngleClockwise) : Element(ElementType::PhasorShift) {
    pInput = AddInputPort<std::complex<float>>("input");
    pOutput = AddOutputPort<std::complex<float>>(LE_PORT_OUTPUT_PREFIX);
    shiftMag = shiftMagnitude;
    shiftAngle = -shiftAngleClockwise / 180.0f * (float)M_PI;
    unitShiftReal = shiftMag * cosf(shiftAngle);
    unitShiftImag = shiftMag * sinf(shiftAngle);
}
void PhasorShift::Update(const Time& timeStamp) {
    UNUSED(timeStamp);
    if (pInput->IsConnected()) {
        std::complex<float> input = pInput->GetValue();
        float real = input.real() * unitShiftReal - input.imag() * unitShiftImag;
        float imag = input.real() * unitShiftImag + input.imag() * unitShiftReal;
        pOutput->SetValue(std::complex<float>(real, imag));
    }
}
void PhasorShift::SetInput(Element* sourceElement, const std::string& sourcePortName) {
    Element::Connect(sourceElement, sourcePortName, this, "input");
}
std::complex<float> PhasorShift::GetOutput() const { return pOutput->GetValue(); }
#else
PhasorShift::PhasorShift(float shiftMagnitude, float shiftAngleClockwise) : Element(ElementType::PhasorShift) {
    pReal = AddInputPort<float>("real");
    pImag = AddInputPort<float>("imaginary");
    pOutReal = AddOutputPort<float>("real");
    pOutImag = AddOutputPort<float>("imaginary");
    shiftMag = shiftMagnitude;
    shiftAngle = -shiftAngleClockwise / 180.0f * (float)M_PI;
    unitShiftReal = shiftMag * cosf(shiftAngle);
    unitShiftImag = shiftMag * sinf(shiftAngle);
}
void PhasorShift::Update(const Time& timeStamp) {
    UNUSED(timeStamp);
    if (pReal->IsConnected() && pImag->IsConnected()) {
        float real = pReal->GetValue();
        float imag = pImag->GetValue();
        pOutReal->SetValue(real * unitShiftReal - imag * unitShiftImag);
        pOutImag->SetValue(real * unitShiftImag + imag * unitShiftReal);
    }
}
void PhasorShift::SetInput_Real(Element* sourceElement, const std::string& sourcePortName) {
    Element::Connect(sourceElement, sourcePortName, this, "real");
}
void PhasorShift::SetInput_Imag(Element* sourceElement, const std::string& sourcePortName) {
    Element::Connect(sourceElement, sourcePortName, this, "imaginary");
}
float PhasorShift::GetReal() const { return pOutReal->GetValue(); }
float PhasorShift::GetImag() const { return pOutImag->GetValue(); }
#endif
#endif

} // namespace LogicElements
