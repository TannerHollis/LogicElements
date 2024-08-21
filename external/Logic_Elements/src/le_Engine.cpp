#include "le_Engine.hpp"

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
    if (type == "LE_AND") return le_Element_Type::LE_AND;
    if (type == "LE_OR") return le_Element_Type::LE_OR;
    if (type == "LE_NOT") return le_Element_Type::LE_NOT;
    if (type == "LE_RTRIG") return le_Element_Type::LE_RTRIG;
    if (type == "LE_FTRIG") return le_Element_Type::LE_FTRIG;
    if (type == "LE_NODE_DIGITAL") return le_Element_Type::LE_NODE_DIGITAL;
    if (type == "LE_NODE_ANALOG") return le_Element_Type::LE_NODE_ANALOG;
    if (type == "LE_TIMER") return le_Element_Type::LE_TIMER;
    if (type == "LE_COUNTER") return le_Element_Type::LE_COUNTER;
    if (type == "LE_MUX_DIGITAL") return le_Element_Type::LE_MUX_DIGITAL;
    if (type == "LE_MUX_ANALOG") return le_Element_Type::LE_MUX_ANALOG;
    if (type == "LE_ANALOG_1P") return le_Element_Type::LE_ANALOG_1P;
    if (type == "LE_ANALOG_3P") return le_Element_Type::LE_ANALOG_3P;
    if (type == "LE_OVERCURRENT") return le_Element_Type::LE_OVERCURRENT;
    if (type == "LE_MATH") return le_Element_Type::LE_MATH;
    if (type == "LE_R2P") return le_Element_Type::LE_R2P;
    if (type == "LE_P2R") return le_Element_Type::LE_P2R;
    if (type == "LE_PHASOR_SHIFT") return le_Element_Type::LE_PHASOR_SHIFT;
    if (type == "LE_PID") return le_Element_Type::LE_PID;

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
    this->uExecTimerFreq = 1; // Default for non divide by zero error
    this->uUpdateTime = 1; // Default for non divide by zero error
    this->uUpdateTimeLast = 0;
    this->uUpdateTimePeriod = 0;
    this->_elementExecTime = std::vector<uint32_t>();
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
            new le_Node<bool>(
                this->uDefaultNodeBufferLength == 0 ? 
                comp->args[0].uArg :
                this->uDefaultNodeBufferLength),
            compName);

#ifdef LE_ELEMENTS_ANALOG
    case le_Element_Type::LE_NODE_ANALOG:
        return this->AddElement(
            new le_Node<float>(
                this->uDefaultNodeBufferLength == 0 ?
                comp->args[0].uArg : 
                this->uDefaultNodeBufferLength),
            compName);
#endif

    case le_Element_Type::LE_TIMER:
        return this->AddElement(new le_Timer(comp->args[0].fArg, comp->args[1].fArg), compName);

    case le_Element_Type::LE_COUNTER:
        return this->AddElement(new le_Counter(comp->args[0].uArg), compName);

    case le_Element_Type::LE_MUX_DIGITAL:
        return this->AddElement(new le_Mux<bool>((uint8_t)comp->args[0].uArg, (uint8_t)comp->args[1].uArg), compName);

#ifdef LE_ELEMENTS_ANALOG
    case le_Element_Type::LE_MUX_ANALOG:
        return this->AddElement(new le_Mux<float>((uint8_t)comp->args[0].uArg, (uint8_t)comp->args[1].uArg), compName);

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

    case le_Element_Type::LE_MATH:
        return this->AddElement(new le_Math((uint8_t)comp->args[0].uArg, comp->args[1].sArg), compName);

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
 * @param timeStep The current timestamp.
 */
void le_Engine::Update(float timeStep)
{
#ifdef LE_ENGINE_EXECUTION_DIAG
    // Get start time once
    uint32_t startUpdateTime = this->GetTime();

    // Calculate update period
    this->uUpdateTimePeriod = startUpdateTime - this->uUpdateTimeLast;
    this->uUpdateTimeLast = startUpdateTime;

    uint32_t t;
    uint16_t nElements = (uint16_t)this->_elements.size();

    // Update each element and measure execution time
    for (uint16_t i = 0; i < nElements; i++)
    {
        le_Element* e = this->_elements[i];
        t = this->GetTime();
        e->Update(timeStep);
        uint32_t* updateTime = &this->_elementExecTime[i];
        *updateTime = this->GetTime() - t;
    }

    // Calculate total update time once
    this->uUpdateTime = this->GetTime() - startUpdateTime;
#else
    // Call each element update individually by highest to lowest order
    for (le_Element* e : this->_elements)
    {
        e->Update(timeStep);
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
    uint32_t overhead = this->uUpdateTime;

    // Calculate CPU usage
    uint16_t updateUsageInteger;
    uint16_t updateUsageFraction;
    this->ConvertFloatingPoint(
        this->uUpdateTime * 100,
        this->uUpdateTimePeriod, 
        &updateUsageInteger, 
        &updateUsageFraction);

    // Calculate Update Frequency
    uint16_t freqInteger;
    uint16_t freqFraction;
    this->ConvertFloatingPoint(
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
        this->ConvertFloatingPoint(
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
    this->ConvertFloatingPoint(
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

#ifdef LE_ENGINE_EXECUTION_DIAG

/**
* @brief Gets timestamp of a running timer to calculate function execute time.
* @return Return the current timer CNT register
*/
WEAK_ATTR inline uint32_t le_Engine::GetTime()
{
    // Get the current time
    auto now = std::chrono::high_resolution_clock::now();

    // Get the time since epoch in microseconds
    auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();

    return microseconds;
}

/**
* @brief Configures the execution timer with a specified frequency.
* @param execTimerFreq Frequency to set for the execution timer.
*/
void le_Engine::ConfigureTimer(uint32_t execTimerFreq)
{
    this->uExecTimerFreq = execTimerFreq;
}

/**
 * @brief Converts a ratio of two integers into integer and fractional parts.
 *
 * This function calculates the ratio of two integers (numerator and denominator) and
 * splits the result into its integer and fractional parts. The fractional part is
 * represented as a value scaled by 1000 to maintain precision.
 *
 * @param numerator The numerator of the ratio.
 * @param denominator The denominator of the ratio.
 * @param integerPart Pointer to store the integer part of the resulting ratio.
 * @param fractionalPart Pointer to store the fractional part of the resulting ratio,
 *                       scaled by 1000.
 */
inline void le_Engine::ConvertFloatingPoint(uint32_t numerator, uint32_t denominator, uint16_t* integerPart, uint16_t* fractionalPart)
{
    float percentage = (float)numerator / (float)denominator;
    *integerPart = (uint16_t)percentage;
    *fractionalPart = (uint16_t)((percentage - (float)*integerPart) * 1000.0f);
}

#endif

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
