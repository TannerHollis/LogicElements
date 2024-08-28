#pragma once

#include "le_Base.hpp"
#include "le_AND.hpp"
#include "le_OR.hpp"
#include "le_NOT.hpp"
#include "le_RTrig.hpp"
#include "le_FTrig.hpp"
#include "le_Node.hpp"
#include "le_Timer.hpp"
#include "le_Counter.hpp"
#include "le_Mux.hpp"
#include "le_SER.hpp"

// Check if analog elements are enabled
#ifdef LE_ELEMENTS_ANALOG
#include "le_Math.hpp"
#include "le_Overcurrent.hpp"
#include "le_Analog1PWinding.hpp"
#include "le_Analog3PWinding.hpp"
#include "le_Rect2Polar.hpp"
#include "le_Polar2Rect.hpp"
#include "le_PhasorShift.hpp"

#ifdef LE_ELEMENTS_PID
#include "le_PID.hpp"
#endif // LE_ELEMENTS_PID
#endif // LE_ELEMNTS_ANALOG

#ifdef LE_DNP3
#include "le_DNP3Outstation.hpp"
#endif

// Include standard C++ libraries
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <cstdio>
#include <cstring>

#define LE_ELEMENT_NAME_LENGTH 8
#define LE_ENGINE_NAME_LENGTH 32
#define LE_ELEMENT_ARGUMENT_LENGTH 64

/**
 * @brief The engine class that manages elements and their connections.
 */
class le_Engine
{
public:
    /**
     * @brief Parses the element type from a string.
     * @param type The string representing the type.
     * @return The corresponding le_Element_Type.
     */
    static le_Element_Type ParseElementType(std::string& type);

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
    typedef union le_Element_Argument_TypeDef
    {
        char sArg[LE_ELEMENT_ARGUMENT_LENGTH];
        float fArg;
        uint16_t uArg;
        bool bArg;
    } le_Element_Argument_TypeDef;

    /**
     * @brief Struct to define an element type.
     */
    typedef struct le_Element_TypeDef
    {
        char name[LE_ELEMENT_NAME_LENGTH];
        le_Element_Type type;
        le_Element_Argument_TypeDef args[5];

        /**
         * @brief Initializes the element name and type.
         * @param name The name of the element.
         * @param type The type of the element.
         */
        le_Element_TypeDef(std::string name, le_Element_Type type);
    } le_Element_TypeDef;

    /**
     * @brief Struct to define a connection to an element.
     */
    typedef struct le_Element_Net_Connection_TypeDef
    {
        char name[LE_ELEMENT_NAME_LENGTH];
        uint16_t slot;
    } le_Element_Net_Connection_TypeDef;

    /**
     * @brief Struct to define a network of connections between elements.
     */
    typedef struct le_Element_Net_TypeDef
    {
        le_Element_Net_Connection_TypeDef output;
        std::vector<le_Element_Net_Connection_TypeDef> inputs;

        /**
         * @brief Initializes the output connection.
         * @param elementName The name of the element.
         * @param outputSlot The output slot.
         */
        le_Element_Net_TypeDef(std::string elementName, uint16_t outputSlot);

        /**
         * @brief Adds an input connection.
         * @param elementName The name of the element.
         * @param inputSlot The input slot.
         */
        void AddInput(std::string elementName, uint16_t inputSlot);
    } le_Element_Net_TypeDef;

    /**
     * @brief Constructor to create an engine with a specified name.
     * @param name The name of the engine.
     */
    le_Engine(std::string name);

    /**
     * @brief Destructor to clean up the engine.
     */
    ~le_Engine();

    /**
     * @brief Adds an element to the engine.
     * @param comp The element type definition.
     * @return The created element.
     */
    le_Element* AddElement(le_Element_TypeDef* comp);

    /**
     * @brief Adds a network of connections to the engine.
     * @param net The network definition.
     */
    void AddNet(le_Element_Net_TypeDef* net);

    /**
     * @brief Updates all elements in the engine.
     * @param timeStamp The current timestamp.
     */
    void Update(const le_Time& timeStamp);

    /**
     * @brief Gets an element by its name.
     * @param elementName The name of the element.
     * @return The element.
     */
    le_Element* GetElement(std::string elementName);

    /**
     * @brief Gets the name of an element.
     * @param e The element.
     * @return The name of the element.
     */
    std::string GetElementName(le_Element* e);

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
    le_Element* AddElement(le_Element* e, const std::string& name);

    /**
     * @brief Sorts the elements in the engine based on their update order.
     */
    void SortElements();

    char sName[LE_ENGINE_NAME_LENGTH];                  ///< The name of the engine.
    std::vector<le_Element*> _elements;                 ///< Vector of elements in the engine.
    std::map<std::string, le_Element*> _elementsByName; ///< Map of elements by their names.
    uint16_t uDefaultNodeBufferLength;

#ifdef LE_ENGINE_EXECUTION_DIAG
private:
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
    void ConvertFloatingPoint(uint32_t numerator, uint32_t denominator, uint16_t* integerPart, uint16_t* fractionalPart);

    uint64_t uExecTimerFreq;
    uint64_t uUpdateTime;
    uint64_t uUpdateTimeLast;
    uint64_t uUpdateTimePeriod;
    std::vector<uint64_t> _elementExecTime;
#endif
};
