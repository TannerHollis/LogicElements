#include "Elements/Digital/le_Counter.hpp"

namespace LogicElements {

/**
 * @brief Constructor implementation that initializes the counter element with a specified final count.
 * @param countFinal The final count value for the counter.
 */
Counter::Counter(uint16_t countFinal) : Element(ElementType::Counter)
{
    // Create ports
    pCountUp = AddInputPort<bool>("count_up");
    pReset = AddInputPort<bool>("reset");
    pOutput = AddOutputPort<bool>("output");

    // Set extrinsic variables
    this->uCountFinal = countFinal;

    // Set intrinsic variables
    this->uCount = 0;
    this->_inputStates[0] = false;
    this->_inputStates[1] = false;
}

/**
 * @brief Updates the counter element.
 * @param timeStamp The current timestamp.
 */
void Counter::Update(const Time& timeStamp)
{
    UNUSED(timeStamp);

    // Check if inputs are connected
    if (!pCountUp->IsConnected() || !pReset->IsConnected())
        return;

    // Update input states for rising trigger detection
    this->_inputStates[1] = this->_inputStates[0];
    this->_inputStates[0] = pCountUp->GetValue();

    // Detect rising edge trigger
    bool rtrig = this->_inputStates[0] && !this->_inputStates[1];

    // Reset dominant logic
    if (pReset->GetValue())
        this->uCount = 0;
    else if (rtrig)
        this->uCount += 1;

    // Set the output based on the count value
    pOutput->SetValue(this->uCount >= this->uCountFinal);
}

/**
 * @brief Sets the input for counting up.
 * @param sourceElement The element to connect from.
 * @param sourcePortName The name of the output port on the source element.
 */
void Counter::SetInput_CountUp(Element* sourceElement, const std::string& sourcePortName)
{
    Element::Connect(sourceElement, sourcePortName, this, "count_up");
}

/**
 * @brief Sets the input for resetting the counter.
 * @param sourceElement The element to connect from.
 * @param sourcePortName The name of the output port on the source element.
 */
void Counter::SetInput_Reset(Element* sourceElement, const std::string& sourcePortName)
{
    Element::Connect(sourceElement, sourcePortName, this, "reset");
}

/**
 * @brief Gets the output value.
 * @return The output value.
 */
bool Counter::GetOutput() const
{
    return pOutput->GetValue();
}

} // namespace LogicElements
