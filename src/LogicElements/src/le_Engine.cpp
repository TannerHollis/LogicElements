#include "le_Engine.hpp"

#include "le_Utility.hpp"

// Define a macro to handle weak attribute based on the compiler
#if defined(_MSC_VER)
#define WEAK_ATTR 
#elif defined(__GNUC__) || defined(__clang__)
#define WEAK_ATTR __attribute__((weak))
#endif

#include <chrono>

namespace LogicElements {

/**
 * @brief Parses the element type from a string.
 * @param type The string representing the type.
 * @return The corresponding ElementType.
 */
ElementType Engine::ParseElementType(std::string& type)
{
    if (type == "ElementType::NodeDigital") return ElementType::NodeDigital;
    if (type == "ElementType::AND") return ElementType::AND;
    if (type == "ElementType::OR") return ElementType::OR;
    if (type == "ElementType::NOT") return ElementType::NOT;
    if (type == "ElementType::RTrig") return ElementType::RTrig;
    if (type == "ElementType::FTrig") return ElementType::FTrig;
    if (type == "ElementType::Timer") return ElementType::Timer;
    if (type == "ElementType::Counter") return ElementType::Counter;
    if (type == "ElementType::MuxDigital") return ElementType::MuxDigital;
    if (type == "ElementType::SER") return ElementType::SER;
#ifdef LE_ELEMENTS_ANALOG
    if (type == "ElementType::NodeAnalog") return ElementType::NodeAnalog;
    if (type == "ElementType::Overcurrent") return ElementType::Overcurrent;
    if (type == "ElementType::Analog1PWinding") return ElementType::Analog1PWinding;
    if (type == "ElementType::Analog3PWinding") return ElementType::Analog3PWinding;
    if (type == "ElementType::Rect2Polar") return ElementType::Rect2Polar;
    if (type == "ElementType::Polar2Rect") return ElementType::Polar2Rect;
    if (type == "ElementType::MuxAnalog") return ElementType::MuxAnalog;
    if (type == "ElementType::PhasorShift") return ElementType::PhasorShift;
#ifdef LE_ELEMENTS_ANALOG_COMPLEX
    if (type == "ElementType::NodeAnalogComplex") return ElementType::NodeAnalogComplex;
    if (type == "ElementType::Complex2Rect") return ElementType::Complex2Rect;
    if (type == "ElementType::Complex2Polar") return ElementType::Complex2Polar;
    if (type == "ElementType::Rect2Complex") return ElementType::Rect2Complex;
    if (type == "ElementType::Polar2Complex") return ElementType::Polar2Complex;
    if (type == "ElementType::MuxAnalogComplex") return ElementType::MuxAnalogComplex;
#endif // LE_ELEMENTS_ANALOG_COMPLEX
#ifdef LE_ELEMENTS_MATH
    if (type == "ElementType::Math") return ElementType::Math;
#endif // ElementType::Math
#ifdef LE_ELEMENTS_PID
    if (type == "ElementType::PID") return ElementType::PID;
#endif // ElementType::PID
#endif // LE_ELEMENTS_ANALOG
    return ElementType::Invalid;
}

/**
 * @brief Copies a string to a destination buffer with clamping to the buffer length.
 * @param src The source string.
 * @param dst The destination buffer.
 * @param dstLength The length of the destination buffer.
 */
void Engine::CopyAndClampString(std::string src, char* dst, uint16_t dstLength)
{
    std::memset(dst, 0, dstLength);
    std::memcpy(dst, src.c_str(), std::min((int)src.size(), dstLength - 1));
    dst[dstLength - 1] = '\0';
}

/**
 * @brief Constructor to create an engine with a specified name.
 * @param name The name of the engine.
 */
Engine::Engine(std::string name)
{
    // Set extrinsic variables
    CopyAndClampString(name, this->sName, LE_ENGINE_NAME_LENGTH);

    // Set intrinsic variables
    this->_elements = std::vector<Element*>();
    this->_elementsByName = std::map<std::string, Element*>();
    this->uDefaultNodeBufferLength = 0;

#ifdef LE_ENGINE_EXECUTION_DIAG
    this->uExecTimerFreq = 1000000000; // 1e9
    this->uUpdateTime = 1; // Default for non divide by zero error
    this->uUpdateTimeLast = 0;
    this->uUpdateTimePeriod = 0;
    this->_elementExecTime = std::vector<int64_t>();
#endif
}

/**
 * @brief Destructor to clean up the engine.
 */
Engine::~Engine()
{
    // Iterate through elements and call destructor
    uint16_t nElements = (uint16_t)this->_elements.size();
    for (uint16_t i = 0; i < nElements; i++)
    {
        Element* e = (Element*)this->_elements[i];
        if (e != nullptr)
            delete e;
    }

    this->_elements.clear();
}

/**
 * @brief Adds an element to the engine.
 * @param comp The element type definition.
 * @return The created element.
 */
Element* Engine::AddElement(ElementTypeDef* comp)
{
    // Convert to std::string
    std::string compName = std::string(comp->name);

    switch ((ElementType)comp->type)
    {
    case ElementType::AND:
        return this->AddElement(new AND((uint8_t)comp->args[0].uArg), compName);

    case ElementType::OR:
        return this->AddElement(new OR((uint8_t)comp->args[0].uArg), compName);

    case ElementType::NOT:
        return this->AddElement(new NOT(), compName);

    case ElementType::RTrig:
        return this->AddElement(new RTrig(), compName);

    case ElementType::FTrig:
        return this->AddElement(new FTrig(), compName);

    case ElementType::NodeDigital:
        return this->AddElement(
            new NodeDigital(
                this->uDefaultNodeBufferLength == 0 ? 
                comp->args[0].uArg :
                this->uDefaultNodeBufferLength),
            compName);

#ifdef LE_ELEMENTS_ANALOG
    case ElementType::NodeAnalog:
        return this->AddElement(
            new NodeAnalog(
                this->uDefaultNodeBufferLength == 0 ?
                comp->args[0].uArg : 
                this->uDefaultNodeBufferLength),
            compName);
#endif

    case ElementType::Timer:
        return this->AddElement(new Timer(comp->args[0].fArg, comp->args[1].fArg), compName);

    case ElementType::Counter:
        return this->AddElement(new Counter(comp->args[0].uArg), compName);

    case ElementType::SER:
        return this->AddElement(new SER(comp->args[0].uArg), compName);

    case ElementType::MuxDigital:
        return this->AddElement(new MuxDigital((uint8_t)comp->args[0].uArg, (uint8_t)comp->args[1].uArg), compName);

#ifdef LE_ELEMENTS_ANALOG
    case ElementType::MuxAnalog:
        return this->AddElement(new MuxAnalog((uint8_t)comp->args[0].uArg, (uint8_t)comp->args[1].uArg), compName);

    case ElementType::Analog1PWinding:
        return this->AddElement(new Analog1PWinding(comp->args[0].uArg), compName);

    case ElementType::Analog3PWinding:
        return this->AddElement(new Analog3PWinding(comp->args[0].uArg), compName);

    case ElementType::Rect2Polar:
        return this->AddElement(new Rect2Polar(), compName);

    case ElementType::Polar2Rect:
        return this->AddElement(new Polar2Rect(), compName);

    case ElementType::PhasorShift:
        return this->AddElement(new PhasorShift(comp->args[0].fArg, comp->args[1].fArg), compName);

    case ElementType::Overcurrent:
    {
        std::string curveName = std::string(comp->args[0].sArg);
        return this->AddElement(new Overcurrent(
            comp->args[0].sArg, // Curve type
            comp->args[1].fArg, // Pickup value
            comp->args[2].fArg, // Time Dial
            comp->args[3].fArg,  // Time Adder
            comp->args[4].bArg), compName); // Electromechanical reset
    }

#ifdef LE_ELEMENTS_ANALOG_COMPLEX
    case ElementType::NodeAnalogComplex:
        return this->AddElement(
            new NodeAnalogComplex(
                this->uDefaultNodeBufferLength == 0 ?
                comp->args[0].uArg :
                this->uDefaultNodeBufferLength),
            compName);

    case ElementType::Complex2Rect:
        return this->AddElement(new Complex2Rect(), compName);

    case ElementType::Complex2Polar:
        return this->AddElement(new Complex2Polar(), compName);

    case ElementType::Rect2Complex:
        return this->AddElement(new Rect2Complex(), compName);

    case ElementType::Polar2Complex:
        return this->AddElement(new Polar2Complex(), compName);

    case ElementType::MuxAnalogComplex:
        return this->AddElement(new MuxAnalogComplex((uint8_t)comp->args[0].uArg, (uint8_t)comp->args[1].uArg), compName);
#endif

#ifdef LE_ELEMENTS_MATH
    case ElementType::Math:
        return this->AddElement(new Math((uint8_t)comp->args[0].uArg, comp->args[1].sArg), compName);
#endif

#ifdef LE_ELEMENTS_PID
    case ElementType::PID:
        return this->AddElement(new PID(
            comp->args[0].fArg, // Proportional constant
            comp->args[1].fArg, // Integral constant
            comp->args[2].fArg, // Derivative constant
            comp->args[3].fArg, // Output minimum
            comp->args[4].fArg), compName); // Output maximum
#endif
#endif

    default:
        // TODO: implement printf() function
        // printf("Unknown component type\n");
        return nullptr;
    }
}

/**
 * @brief Adds a network of connections to the engine.
 * @param net The network definition.
 */
void Engine::AddNet(ElementNetTypeDef* net)
{
    // Get number of inputs
    uint16_t n = (uint16_t)net->inputs.size();

    // Get output element
    std::string outputName = std::string(net->output.name);
    Element* outputElement = this->GetElement(outputName);
    if (outputElement == nullptr)
        return;

    // Get output port name
    std::string outputPortName = std::string(net->output.port);

    // Iterate through inputs and connect
    for (uint16_t i = 0; i < n; i++)
    {
        // Get input element
        std::string inputName = std::string(net->inputs[i].name);
        Element* inputElement = this->GetElement(inputName);

        // If element doesn't exist
        if (inputElement == nullptr)
            continue;

        // Get input port name
        std::string inputPortName = std::string(net->inputs[i].port);

        // Connect elements using port names
        Element::Connect(outputElement, outputPortName, inputElement, inputPortName);
    }

    // Sort elements
    this->SortElements();
}

/**
 * @brief Updates all elements in the engine.
 * @param timeStamp The current timestamp.
 */
void Engine::Update(const Time& timeStamp)
{
#ifdef LE_ENGINE_EXECUTION_DIAG
    // Record the start time of the update
    int64_t startUpdateTime = timeStamp.ToNanosecondsSinceEpoch();

    // Calculate the time period since the last update
    this->uUpdateTimePeriod = startUpdateTime - this->uUpdateTimeLast;
    this->uUpdateTimeLast = startUpdateTime;

    uint16_t nElements = static_cast<uint16_t>(this->_elements.size());

    // Ensure _elementExecTime is properly sized
    if (this->_elementExecTime.size() != nElements) {
        this->_elementExecTime.resize(nElements, 0);
    }

    // Update each element and measure its execution time
    for (uint16_t i = 0; i < nElements; i++)
    {
        Element* element = this->_elements[i];
        Time elementStartTime = Time::GetTime();
        element->Update(timeStamp);
        this->_elementExecTime[i] = Time::GetTime().ToNanosecondsSinceEpoch() - elementStartTime.ToNanosecondsSinceEpoch();
    }

    // Calculate the total update time
    this->uUpdateTime = Time::GetTime().ToNanosecondsSinceEpoch() - startUpdateTime;
#else
    // Update each element in the order they are stored
    for (Element* element : this->_elements)
    {
        element->Update(timeStamp);
    }
#endif
}

/**
 * @brief Gets an element by its name.
 * @param elementName The name of the element.
 * @return The element.
 */
Element* Engine::GetElement(std::string elementName)
{
    // Find element by name
    auto it = this->_elementsByName.find(elementName);

    // If key doesn't exist, return nullptr
    if (it == this->_elementsByName.end())
        return nullptr;
    else
        return it->second;
}

/**
 * @brief Gets the name of an element.
 * @param e The element.
 * @return The name of the element.
 */
std::string Engine::GetElementName(Element* e)
{
    for (const auto& pair : this->_elementsByName)
    {
        if (pair.second == e)
        {
            return pair.first;
        }
    }
    return ""; // Return an empty string if the value is not found
}

/**
 * @brief Prints the current state of the engine.
 */
uint16_t Engine::GetInfo(char* buffer, uint16_t length)
{
    uint16_t offset = 0;

    // Print engine name
    offset += snprintf(buffer, length, "Engine Name: %s\r\n", sName);

#ifdef LE_ENGINE_EXECUTION_DIAG
    // Variable for overhead
    uint64_t overhead = this->uUpdateTime;

    // Calculate CPU usage
    uint16_t updateUsageInteger;
    uint16_t updateUsageFraction;
    Utility::ConvertFloatingPoint(
        this->uUpdateTime * 100,
        this->uUpdateTimePeriod, 
        &updateUsageInteger, 
        &updateUsageFraction);

    // Calculate Update Frequency
    uint16_t freqInteger;
    uint16_t freqFraction;
    Utility::ConvertFloatingPoint(
        this->uExecTimerFreq,
        this->uUpdateTimePeriod, 
        &freqInteger,
        &freqFraction);

    // Print to buffer
    offset += snprintf(
        buffer + offset,
        length - offset,
        "CPU_Total: %3u.%03u%%\tFreq: %5u.%03u Hz\r\n",
        updateUsageInteger,
        updateUsageFraction,
        freqInteger,
        freqFraction);
#endif
    uint16_t nElements = (uint16_t)this->_elements.size();
    for (uint16_t i = 0; i < nElements; i++)
    {
        Element* e = this->_elements[i];
        std::string elementName = this->GetElementName(e);
#ifdef LE_ENGINE_EXECUTION_DIAG
        // Calculate element CPU usage
        uint16_t usageInteger;
        uint16_t usageFraction;
        Utility::ConvertFloatingPoint(
            this->_elementExecTime[i] * 100, 
            this->uUpdateTime, 
            &usageInteger,
            &usageFraction);

        // Print to buffer
        offset += snprintf(
            buffer + offset, 
            length - offset, 
            "  Element: %-8s\tOrder: %-3u\tCPU_Update: %3u.%03u%%\r\n",
            elementName.c_str(), 
            e->GetOrder(),
            usageInteger,
            usageFraction);

        overhead -= this->_elementExecTime[i];
#else
        offset += snprintf(buffer + offset, length - offset, "%s  Element: %-8s \tOrder: %-3u\r\n", buffer, elementName.c_str(), e->GetOrder());
#endif
    }

#ifdef LE_ENGINE_EXECUTION_DIAG
    // Calculate overhead percentage
    uint16_t overheadInteger;
    uint16_t overheadFraction;
    Utility::ConvertFloatingPoint(
        overhead * 100,
        this->uUpdateTime,
        &overheadInteger,
        &overheadFraction);

    // Print to buffer
    offset += snprintf(
        buffer + offset, 
        length - offset, 
        "  Engine Overhead:\t\t\tCPU_Update: %3u.%03u%%\r\n",
        overheadInteger, 
        overheadFraction);
#endif
    return offset;
}

/*
* @brief Allows the user to define a default buffer length for nodes, used for event data logging.
* @param Size of data buffer to be allocated.
*/
void Engine::SetDefaultNodeBufferLength(uint16_t length)
{
    this->uDefaultNodeBufferLength = length;
}

/**
 * @brief Adds an element with a specified name to the engine.
 * @param name The name of the element.
 * @param e The element.
 * @return The added element.
 */
Element* Engine::AddElement(Element* e, const std::string& name)
{
    // Create new map member
    std::pair<std::string, Element*> newElement = std::make_pair(name, e);

    // Insert new member into map
    auto insertResult = this->_elementsByName.insert(newElement);

    // Add element to vector
    this->_elements.push_back(insertResult.first->second);

    // Sort elements
    this->SortElements();

#ifdef LE_ENGINE_EXECUTION_DIAG
    // Add element to execution time map
    this->_elementExecTime.push_back(0);
#endif

    return insertResult.first->second;
}

/**
 * @brief Sorts the elements in the engine based on their update order.
 */
void Engine::SortElements()
{
    std::sort(this->_elements.begin(), this->_elements.end(), Element::CompareElementOrders);
}

/**
 * @brief Sets the output connection.
 * @param elementName The name of the element.
 * @param outputPortName The name of the output port.
 */
Engine::ElementNetTypeDef::ElementNetTypeDef(std::string elementName, std::string outputPortName)
{
    // Create output
    Engine::CopyAndClampString(elementName, this->output.name, LE_ELEMENT_NAME_LENGTH);
    Engine::CopyAndClampString(outputPortName, this->output.port, LE_ELEMENT_NAME_LENGTH);
}

/**
 * @brief Adds an input connection.
 * @param elementName The name of the element.
 * @param inputPortName The name of the input port.
 */
void Engine::ElementNetTypeDef::AddInput(std::string elementName, std::string inputPortName)
{
    // Create connection
    ElementNetConnectionTypeDef net;
    Engine::CopyAndClampString(elementName, net.name, LE_ELEMENT_NAME_LENGTH);
    Engine::CopyAndClampString(inputPortName, net.port, LE_ELEMENT_NAME_LENGTH);

    // Add to inputs
    this->inputs.push_back(net);
}

/**
* @brief Initializes the element name and type.
* @param name The name of the element.
* @param type The type of the element.
*/

Engine::ElementTypeDef::ElementTypeDef(std::string name, ElementType type)
{
    CopyAndClampString(name.c_str(), this->name, LE_ELEMENT_NAME_LENGTH);
    this->type = type;
    this->args[0] = ElementArgumentTypeDef({ 0 });
    this->args[1] = ElementArgumentTypeDef({ 0 });
    this->args[2] = ElementArgumentTypeDef({ 0 });
    this->args[3] = ElementArgumentTypeDef({ 0 });
    this->args[4] = ElementArgumentTypeDef({ 0 });
}

} // namespace LogicElements
