#pragma once

#include "Core/le_Config.hpp"
#include "Core/le_Element.hpp"
#include "Elements/Digital/le_AND.hpp"
#include "Elements/Digital/le_OR.hpp"
#include "Elements/Digital/le_NOT.hpp"
#include "Elements/Digital/le_RTrig.hpp"
#include "Elements/Digital/le_FTrig.hpp"
#include "Core/le_Node.hpp"
#include "Elements/Digital/le_Timer.hpp"
#include "Elements/Digital/le_Counter.hpp"
#include "Elements/Digital/le_Mux.hpp"
#include "Elements/Digital/le_SER.hpp"

// Check if analog elements are enabled
#ifdef LE_ELEMENTS_ANALOG
#include "Elements/Control/le_Overcurrent.hpp"
#include "Elements/Power/le_Analog1PWinding.hpp"
#include "Elements/Power/le_Analog3PWinding.hpp"
#include "Elements/Conversions/le_Rect2Polar.hpp"
#include "Elements/Conversions/le_Polar2Rect.hpp"
#include "Elements/Power/le_PhasorShift.hpp"

// Arithmetic elements
#include "Elements/Arithmetic/le_Add.hpp"
#include "Elements/Arithmetic/le_Subtract.hpp"
#include "Elements/Arithmetic/le_Multiply.hpp"
#include "Elements/Arithmetic/le_Divide.hpp"
#include "Elements/Arithmetic/le_Negate.hpp"
#include "Elements/Arithmetic/le_Abs.hpp"

#ifdef LE_ELEMENTS_MATH
#include "Elements/Control/le_Math.hpp"
#endif // ElementType::Math

#ifdef LE_ELEMENTS_PID
#include "Elements/Control/le_PID.hpp"
#endif // LE_ELEMENTS_PID

#ifdef LE_ELEMENTS_ANALOG_COMPLEX
#include "Elements/Conversions/le_Rect2Complex.hpp"
#include "Elements/Conversions/le_Polar2Complex.hpp"
#include "Elements/Conversions/le_Complex2Rect.hpp"
#include "Elements/Conversions/le_Complex2Polar.hpp"
#include "Elements/Arithmetic/le_AddComplex.hpp"
#include "Elements/Arithmetic/le_SubtractComplex.hpp"
#include "Elements/Arithmetic/le_MultiplyComplex.hpp"
#include "Elements/Arithmetic/le_DivideComplex.hpp"
#include "Elements/Arithmetic/le_NegateComplex.hpp"
#include "Elements/Arithmetic/le_Magnitude.hpp"
#endif // LE_ELEMENTS_ANALOG_COMPLEX

#endif // LE_ELEMNTS_ANALOG

#ifdef LE_DNP3
#include "Communication/le_DNP3Outstation.hpp"
#endif

// Include standard C++ libraries
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace LogicElements {

/**
 * @brief The engine class that manages elements and their connections.
 */
class Engine
{
public:
    /**
     * @brief Parses the element type from a string.
     * @param type The string representing the type.
     * @return The corresponding ElementType.
     */
    static ElementType ParseElementType(std::string& type);

    /**
     * @brief Copies a string to a destination buffer with clamping to the buffer length.
     * @param src The source string.
     * @param dst The destination buffer.
     * @param dstLength The length of the destination buffer.
     */
    static void CopyAndClampString(std::string src, char* dst, uint16_t dstLength);

public:
    /**
     * @brief Union to represent different types of element arguments.
     */
    typedef union ElementArgumentTypeDef
    {
        char sArg[LE_ELEMENT_ARGUMENT_LENGTH];
        float fArg;
        uint16_t uArg;
        bool bArg;
    } ElementArgumentTypeDef;

    /**
     * @brief Struct to define an element type.
     */
    typedef struct ElementTypeDef
    {
        char name[LE_ELEMENT_NAME_LENGTH];
        ElementType type;
        ElementArgumentTypeDef args[5];

        /**
         * @brief Initializes the element name and type.
         * @param name The name of the element.
         * @param type The type of the element.
         */
        ElementTypeDef(std::string name, ElementType type);
    } ElementTypeDef;

    /**
     * @brief Struct to define a connection to an element.
     */
    typedef struct ElementNetConnectionTypeDef
    {
        char name[LE_ELEMENT_NAME_LENGTH];
        char port[LE_ELEMENT_NAME_LENGTH]; // Port name instead of slot number
    } ElementNetConnectionTypeDef;

    /**
     * @brief Struct to define a network of connections between elements.
     */
    typedef struct ElementNetTypeDef
    {
        ElementNetConnectionTypeDef output;
        std::vector<ElementNetConnectionTypeDef> inputs;

        /**
         * @brief Initializes the output connection.
         * @param elementName The name of the element.
         * @param outputPortName The name of the output port.
         */
        ElementNetTypeDef(std::string elementName, std::string outputPortName);

        /**
         * @brief Adds an input connection.
         * @param elementName The name of the element.
         * @param inputPortName The name of the input port.
         */
        void AddInput(std::string elementName, std::string inputPortName);
    } ElementNetTypeDef;

    /**
     * @brief Constructor to create an engine with a specified name.
     * @param name The name of the engine.
     */
    Engine(std::string name);

    /**
     * @brief Destructor to clean up the engine.
     */
    ~Engine();

    /**
     * @brief Adds an element to the engine.
     * @param comp The element type definition.
     * @return The created element.
     */
    Element* AddElement(ElementTypeDef* comp);

    /**
     * @brief Adds a network of connections to the engine.
     * @param net The network definition.
     */
    void AddNet(ElementNetTypeDef* net);

    /**
     * @brief Updates all elements in the engine.
     * @param timeStamp The current timestamp.
     */
    void Update(const Time& timeStamp);

    /**
     * @brief Gets an element by its name.
     * @param elementName The name of the element.
     * @return The element.
     */
    Element* GetElement(std::string elementName);

    /**
     * @brief Gets the name of an element.
     * @param e The element.
     * @return The name of the element.
     */
    std::string GetElementName(Element* e);

    /**
     * @brief Prints the current state of the engine.
     * @param buffer Pointer to the character buffer
     * @param length Length of the character buffer
     * @return the character count written to buffer
     */
    uint16_t GetInfo(char* buffer, uint16_t length);

    /*
    * @brief Allows the user to define a default buffer length for nodes, used for event data logging.
    * @param Size of data buffer to be allocated.
    */
    void SetDefaultNodeBufferLength(uint16_t length);

private:
    /**
     * @brief Adds an element with a specified name to the engine.
     * @param name The name of the element.
     * @param e The element.
     * @return The added element.
     */
    Element* AddElement(Element* e, const std::string& name);

    /**
     * @brief Sorts the elements in the engine based on their update order.
     */
    void SortElements();

    char sName[LE_ENGINE_NAME_LENGTH];                  ///< The name of the engine.
    std::vector<Element*> _elements;                 ///< Vector of elements in the engine.
    std::map<std::string, Element*> _elementsByName; ///< Map of elements by their names.
    uint16_t uDefaultNodeBufferLength;

    int64_t uExecTimerFreq;
    int64_t uUpdateTime;
    int64_t uUpdateTimeLast;
    int64_t uUpdateTimePeriod;
    std::vector<int64_t> _elementExecTime;
};

} // namespace LogicElements
