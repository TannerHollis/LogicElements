#include "le_Engine.hpp"

#include "le_Utility.hpp"

// Define a macro to handle weak attribute based on the compiler
#if defined(_MSC_VER)
#define WEAK_ATTR 
#elif defined(__GNUC__) || defined(__clang__)
#define WEAK_ATTR __attribute__((weak))
#endif

#include <chrono>

/**
 * @brief Parses the element type from a string.
 * @param type The string representing the type.
 * @return The corresponding le_Element_Type.
 */
le_Element_Type le_Engine::ParseElementType(std::string& type)
{
    if (type == "LE_NODE_DIGITAL") return le_Element_Type::LE_NODE_DIGITAL;
    if (type == "LE_AND") return le_Element_Type::LE_AND;
    if (type == "LE_OR") return le_Element_Type::LE_OR;
    if (type == "LE_NOT") return le_Element_Type::LE_NOT;
    if (type == "LE_RTRIG") return le_Element_Type::LE_RTRIG;
    if (type == "LE_FTRIG") return le_Element_Type::LE_FTRIG;
    if (type == "LE_TIMER") return le_Element_Type::LE_TIMER;
    if (type == "LE_COUNTER") return le_Element_Type::LE_COUNTER;
    if (type == "LE_MUX_DIGITAL") return le_Element_Type::LE_MUX_DIGITAL;
    if (type == "LE_SER") return le_Element_Type::LE_SER;
#ifdef LE_ELEMENTS_ANALOG
    if (type == "LE_NODE_ANALOG") return le_Element_Type::LE_NODE_ANALOG;
    if (type == "LE_OVERCURRENT") return le_Element_Type::LE_OVERCURRENT;
    if (type == "LE_ANALOG_1P") return le_Element_Type::LE_ANALOG_1P;
    if (type == "LE_ANALOG_3P") return le_Element_Type::LE_ANALOG_3P;
    if (type == "LE_R2P") return le_Element_Type::LE_R2P;
    if (type == "LE_P2R") return le_Element_Type::LE_P2R;
    if (type == "LE_MUX_ANALOG") return le_Element_Type::LE_MUX_ANALOG;
    if (type == "LE_PHASOR_SHIFT") return le_Element_Type::LE_PHASOR_SHIFT;
#ifdef LE_ELEMENTS_ANALOG_COMPLEX
    if (type == "LE_NODE_ANALOG_COMPLEX") return le_Element_Type::LE_NODE_ANALOG_COMPLEX;
    if (type == "LE_C2R") return le_Element_Type::LE_C2R;
    if (type == "LE_C2P") return le_Element_Type::LE_C2P;
    if (type == "LE_R2C") return le_Element_Type::LE_R2C;
    if (type == "LE_P2C") return le_Element_Type::LE_P2C;
    if (type == "LE_MUX_ANALOG_COMPLEX") return le_Element_Type::LE_MUX_ANALOG_COMPLEX;
#endif // LE_ELEMENTS_ANALOG_COMPLEX
#ifdef LE_ELEMENTS_MATH
    if (type == "LE_MATH") return le_Element_Type::LE_MATH;
#endif // LE_MATH
#ifdef LE_ELEMENTS_PID
    if (type == "LE_PID") return le_Element_Type::LE_PID;
#endif // LE_PID
#endif // LE_ELEMENTS_ANALOG
    return le_Element_Type::LE_INVALID;
}

/**
 * @brief Copies a string to a destination buffer with clamping to the buffer length.
 * @param src The source string.
 * @param dst The destination buffer.
 * @param dstLength The length of the destination buffer.
 */
void le_Engine::CopyAndClampString(std::string src, char* dst, uint16_t dstLength)
{
    std::memset(dst, 0, dstLength);
    std::memcpy(dst, src.c_str(), std::min((int)src.size(), dstLength - 1));
    dst[dstLength - 1] = '\0';
}

/**
 * @brief Constructor to create an engine with a specified name.
 * @param name The name of the engine.
 */
le_Engine::le_Engine(std::string name)
{
    // Set extrinsic variables
    CopyAndClampString(name, this->sName, LE_ENGINE_NAME_LENGTH);

    // Set intrinsic variables
    this->_elements = std::vector<le_Element*>();
    this->_elementsByName = std::map<std::string, le_Element*>();
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
le_Engine::~le_Engine()
{
    // Iterate through elements and call destructor
    uint16_t nElements = (uint16_t)this->_elements.size();
    for (uint16_t i = 0; i < nElements; i++)
    {
        le_Element* e = (le_Element*)this->_elements[i];
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
le_Element* le_Engine::AddElement(le_Element_TypeDef* comp)
{
    // Convert to std::string
    std::string compName = std::string(comp->name);

    switch ((le_Element_Type)comp->type)
    {
    case le_Element_Type::LE_AND:
        return this->AddElement(new le_AND((uint8_t)comp->args[0].uArg), compName);

    case le_Element_Type::LE_OR:
        return this->AddElement(new le_OR((uint8_t)comp->args[0].uArg), compName);

    case le_Element_Type::LE_NOT:
        return this->AddElement(new le_NOT(), compName);

    case le_Element_Type::LE_RTRIG:
        return this->AddElement(new le_RTrig(), compName);

    case le_Element_Type::LE_FTRIG:
        return this->AddElement(new le_FTrig(), compName);

    case le_Element_Type::LE_NODE_DIGITAL:
        return this->AddElement(
            new le_Node_Digital(
                this->uDefaultNodeBufferLength == 0 ? 
                comp->args[0].uArg :
                this->uDefaultNodeBufferLength),
            compName);

#ifdef LE_ELEMENTS_ANALOG
    case le_Element_Type::LE_NODE_ANALOG:
        return this->AddElement(
            new le_Node_Analog(
                this->uDefaultNodeBufferLength == 0 ?
                comp->args[0].uArg : 
                this->uDefaultNodeBufferLength),
            compName);
#endif

    case le_Element_Type::LE_TIMER:
        return this->AddElement(new le_Timer(comp->args[0].fArg, comp->args[1].fArg), compName);

    case le_Element_Type::LE_COUNTER:
        return this->AddElement(new le_Counter(comp->args[0].uArg), compName);

    case le_Element_Type::LE_SER:
        return this->AddElement(new le_SER(comp->args[0].uArg), compName);

    case le_Element_Type::LE_MUX_DIGITAL:
        return this->AddElement(new le_Mux_Digital((uint8_t)comp->args[0].uArg, (uint8_t)comp->args[1].uArg), compName);

#ifdef LE_ELEMENTS_ANALOG
    case le_Element_Type::LE_MUX_ANALOG:
        return this->AddElement(new le_Mux_Analog((uint8_t)comp->args[0].uArg, (uint8_t)comp->args[1].uArg), compName);

    case le_Element_Type::LE_ANALOG_1P:
        return this->AddElement(new le_Analog1PWinding(comp->args[0].uArg), compName);

    case le_Element_Type::LE_ANALOG_3P:
        return this->AddElement(new le_Analog3PWinding(comp->args[0].uArg), compName);

    case le_Element_Type::LE_R2P:
        return this->AddElement(new le_Rect2Polar(), compName);

    case le_Element_Type::LE_P2R:
        return this->AddElement(new le_Polar2Rect(), compName);

    case le_Element_Type::LE_PHASOR_SHIFT:
        return this->AddElement(new le_PhasorShift(comp->args[0].fArg, comp->args[1].fArg), compName);

    case le_Element_Type::LE_OVERCURRENT:
    {
        std::string curveName = std::string(comp->args[0].sArg);
        return this->AddElement(new le_Overcurrent(
            comp->args[0].sArg, // Curve type
            comp->args[1].fArg, // Pickup value
            comp->args[2].fArg, // Time Dial
            comp->args[3].fArg,  // Time Adder
            comp->args[4].bArg), compName); // Electromechanical reset
    }

#ifdef LE_ELEMENTS_ANALOG_COMPLEX
    case le_Element_Type::LE_NODE_ANALOG_COMPLEX:
        return this->AddElement(
            new le_Node_AnalogComplex(
                this->uDefaultNodeBufferLength == 0 ?
                comp->args[0].uArg :
                this->uDefaultNodeBufferLength),
            compName);

    case le_Element_Type::LE_C2R:
        return this->AddElement(new le_Complex2Rect(), compName);

    case le_Element_Type::LE_C2P:
        return this->AddElement(new le_Complex2Polar(), compName);

    case le_Element_Type::LE_R2C:
        return this->AddElement(new le_Rect2Complex(), compName);

    case le_Element_Type::LE_P2C:
        return this->AddElement(new le_Polar2Complex(), compName);

    case le_Element_Type::LE_MUX_ANALOG_COMPLEX:
        return this->AddElement(new le_Mux_AnalogComplex((uint8_t)comp->args[0].uArg, (uint8_t)comp->args[1].uArg), compName);
#endif

#ifdef LE_ELEMENTS_MATH
    case le_Element_Type::LE_MATH:
        return this->AddElement(new le_Math((uint8_t)comp->args[0].uArg, comp->args[1].sArg), compName);
#endif

#ifdef LE_ELEMENTS_PID
    case le_Element_Type::LE_PID:
        return this->AddElement(new le_PID(
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
void le_Engine::AddNet(le_Element_Net_TypeDef* net)
{
    // Get number of inputs
    uint16_t n = (uint16_t)net->inputs.size();

    // Get output element
    std::string outputName = std::string(net->output.name);
    le_Element* outputElement = this->GetElement(outputName);
    if (outputElement == nullptr)
        return;

    // Iterate through inputs and connect
    for (uint16_t i = 0; i < n; i++)
    {
        // Get input element
        std::string inputName = std::string(net->inputs[i].name);
        le_Element* inputElement = this->GetElement(inputName);

        // If element doesn't exist
        if (inputElement == nullptr)
            continue;

        // Connect elements
        le_Element::Connect(outputElement, (uint8_t)net->output.slot, inputElement, (uint8_t)net->inputs[i].slot);
    }

    // Sort elements
    this->SortElements();
}

/**
 * @brief Updates all elements in the engine.
 * @param timeStamp The current timestamp.
 */
void le_Engine::Update(const le_Time& timeStamp)
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
        le_Element* element = this->_elements[i];
        le_Time elementStartTime = le_Time::GetTime();
        element->Update(timeStamp);
        this->_elementExecTime[i] = le_Time::GetTime().ToNanosecondsSinceEpoch() - elementStartTime.ToNanosecondsSinceEpoch();
    }

    // Calculate the total update time
    this->uUpdateTime = le_Time::GetTime().ToNanosecondsSinceEpoch() - startUpdateTime;
#else
    // Update each element in the order they are stored
    for (le_Element* element : this->_elements)
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
le_Element* le_Engine::GetElement(std::string elementName)
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
std::string le_Engine::GetElementName(le_Element* e)
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
uint16_t le_Engine::GetInfo(char* buffer, uint16_t length)
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
    le_Utility::ConvertFloatingPoint(
        this->uUpdateTime * 100,
        this->uUpdateTimePeriod, 
        &updateUsageInteger, 
        &updateUsageFraction);

    // Calculate Update Frequency
    uint16_t freqInteger;
    uint16_t freqFraction;
    le_Utility::ConvertFloatingPoint(
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
        le_Element* e = this->_elements[i];
        std::string elementName = this->GetElementName(e);
#ifdef LE_ENGINE_EXECUTION_DIAG
        // Calculate element CPU usage
        uint16_t usageInteger;
        uint16_t usageFraction;
        le_Utility::ConvertFloatingPoint(
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
    le_Utility::ConvertFloatingPoint(
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
void le_Engine::SetDefaultNodeBufferLength(uint16_t length)
{
    this->uDefaultNodeBufferLength = length;
}

/**
 * @brief Adds an element with a specified name to the engine.
 * @param name The name of the element.
 * @param e The element.
 * @return The added element.
 */
le_Element* le_Engine::AddElement(le_Element* e, const std::string& name)
{
    // Create new map member
    std::pair<std::string, le_Element*> newElement = std::make_pair(name, e);

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
void le_Engine::SortElements()
{
    std::sort(this->_elements.begin(), this->_elements.end(), le_Element::CompareElementOrders);
}

/**
 * @brief Sets the output connection.
 * @param elementName The name of the element.
 * @param outputSlot The output slot.
 */
le_Engine::le_Element_Net_TypeDef::le_Element_Net_TypeDef(std::string elementName, uint16_t outputSlot)
{
    // Create output
    le_Engine::CopyAndClampString(elementName, this->output.name, LE_ELEMENT_NAME_LENGTH);
    this->output.slot = outputSlot;
}

/**
 * @brief Adds an input connection.
 * @param elementName The name of the element.
 * @param inputSlot The input slot.
 */
void le_Engine::le_Element_Net_TypeDef::AddInput(std::string elementName, uint16_t inputSlot)
{
    // Create connection
    le_Element_Net_Connection_TypeDef net;
    net.slot = 0;
    le_Engine::CopyAndClampString(elementName, net.name, LE_ELEMENT_NAME_LENGTH);
    net.slot = inputSlot; // Set slot

    // Add to inputs
    this->inputs.push_back(net);
}

/**
* @brief Initializes the element name and type.
* @param name The name of the element.
* @param type The type of the element.
*/

le_Engine::le_Element_TypeDef::le_Element_TypeDef(std::string name, le_Element_Type type)
{
    CopyAndClampString(name.c_str(), this->name, LE_ELEMENT_NAME_LENGTH);
    this->type = type;
    this->args[0] = le_Element_Argument_TypeDef({ 0 });
    this->args[1] = le_Element_Argument_TypeDef({ 0 });
    this->args[2] = le_Element_Argument_TypeDef({ 0 });
    this->args[3] = le_Element_Argument_TypeDef({ 0 });
    this->args[4] = le_Element_Argument_TypeDef({ 0 });
}
