#include "le_Element.hpp"

namespace LogicElements {

/**
 * @brief Constructor that initializes the element.
 * @param type The type of the element.
 */
Element::Element(ElementType type) : type(type), iOrder(0)
{
    // Port vectors are initialized empty
}

/**
 * @brief Destructor to clean up allocated memory.
 */
Element::~Element()
{
    // Deallocate all input ports
    for (Port* port : _inputPorts)
    {
        delete port;
    }
    _inputPorts.clear();
    _inputPortsByName.clear();

    // Deallocate all output ports
    for (Port* port : _outputPorts)
    {
        delete port;
    }
    _outputPorts.clear();
    _outputPortsByName.clear();
}

/**
 * @brief Gets the update order of the element.
 * @return The update order.
 */
uint16_t Element::GetOrder()
{
    // Reset order to zero and recalculate it
    this->iOrder = 0;
    this->FindOrder(this, &(this->iOrder));

    return this->iOrder;
}

/**
* @brief Comparison operator for ordering elements by their update order.
* @param left The left-hand element to compare with.
* @param right The right-hand element to compare with.
* @return True if this element has a smaller update order than the other element.
*/
bool Element::CompareElementOrders(Element* left, Element* right)
{
    return left->GetOrder() < right->GetOrder();
}

/**
 * @brief Static function to connect the output of one element to the input of another element using port names.
 * @param outputElement The output element.
 * @param outputPortName The name of the output port.
 * @param inputElement The input element.
 * @param inputPortName The name of the input port.
 * @return True if connection was successful, false if type mismatch or port not found.
 */
bool Element::Connect(Element* outputElement, const std::string& outputPortName,
                         Element* inputElement, const std::string& inputPortName)
{
    // Get the output port
    Port* outputPort = outputElement->GetOutputPort(outputPortName);
    if (outputPort == nullptr)
        return false;

    // Get the input port
    Port* inputPort = inputElement->GetInputPort(inputPortName);
    if (inputPort == nullptr)
        return false;

    // Check type compatibility
    if (outputPort->GetType() != inputPort->GetType())
        return false;

    // Connect based on type
    switch (outputPort->GetType())
    {
    case PortType::DIGITAL:
    {
        OutputPort<bool>* outP = dynamic_cast<OutputPort<bool>*>(outputPort);
        InputPort<bool>* inP = dynamic_cast<InputPort<bool>*>(inputPort);
        if (outP && inP)
        {
            inP->Connect(outP);
            return true;
        }
        break;
    }
    case PortType::ANALOG:
    {
        OutputPort<float>* outP = dynamic_cast<OutputPort<float>*>(outputPort);
        InputPort<float>* inP = dynamic_cast<InputPort<float>*>(inputPort);
        if (outP && inP)
        {
            inP->Connect(outP);
            return true;
        }
        break;
    }
    case PortType::COMPLEX:
    {
        OutputPort<std::complex<float>>* outP = dynamic_cast<OutputPort<std::complex<float>>*>(outputPort);
        InputPort<std::complex<float>>* inP = dynamic_cast<InputPort<std::complex<float>>*>(inputPort);
        if (outP && inP)
        {
            inP->Connect(outP);
            return true;
        }
        break;
    }
    }

    return false;
}

/**
 * @brief Gets an input port by name.
 * @param name The name of the port.
 * @return Pointer to the port, or nullptr if not found.
 */
Port* Element::GetInputPort(const std::string& name)
{
    auto it = _inputPortsByName.find(name);
    if (it != _inputPortsByName.end())
        return it->second;
    return nullptr;
}

/**
 * @brief Gets an output port by name.
 * @param name The name of the port.
 * @return Pointer to the port, or nullptr if not found.
 */
Port* Element::GetOutputPort(const std::string& name)
{
    auto it = _outputPortsByName.find(name);
    if (it != _outputPortsByName.end())
        return it->second;
    return nullptr;
}

/**
 * @brief Finds the order of the element recursively.
 * @param original The original element to start from.
 * @param order Pointer to store the order.
 */
void Element::FindOrder(Element* original, uint16_t* order)
{
    // Increment the order
    (*order)++;

    size_t numInputs = _inputPorts.size();
    if (numInputs == 0)
        return;

    // Create array of orders for each input
    uint16_t* orders = new uint16_t[numInputs];

    // Iterate through inputs to calculate the order
    for (size_t i = 0; i < numInputs; i++)
    {
        orders[i] = *order;
        Port* port = _inputPorts[i];
        
        // Get the source element from the connected port
        Element* sourceElement = nullptr;
        
        // Try each port type
        if (port->GetType() == PortType::DIGITAL)
        {
            InputPort<bool>* inputPort = dynamic_cast<InputPort<bool>*>(port);
            if (inputPort && inputPort->IsConnected())
            {
                sourceElement = inputPort->GetSource()->GetOwner();
            }
        }
        else if (port->GetType() == PortType::ANALOG)
        {
            InputPort<float>* inputPort = dynamic_cast<InputPort<float>*>(port);
            if (inputPort && inputPort->IsConnected())
            {
                sourceElement = inputPort->GetSource()->GetOwner();
            }
        }
        else if (port->GetType() == PortType::COMPLEX)
        {
            InputPort<std::complex<float>>* inputPort = dynamic_cast<InputPort<std::complex<float>>*>(port);
            if (inputPort && inputPort->IsConnected())
            {
                sourceElement = inputPort->GetSource()->GetOwner();
            }
        }

        // Avoid circular dependency and null pointers
        if (sourceElement != nullptr && sourceElement != original)
        {
            sourceElement->FindOrder(this, &orders[i]);
        }
    }

    // Take highest order
    for (size_t i = 0; i < numInputs; i++)
    {
        if (orders[i] > *order)
        {
            *order = orders[i];
        }
    }

    // Deallocate orders
    delete[] orders;
}

/**
 * @brief Gets the type of the specified element.
 * @param e The element to get the type of.
 * @return The type of the element.
 */
ElementType Element::GetType(Element* e)
{
    return e->type;
}

} // namespace LogicElements