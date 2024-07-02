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
#include "le_Overcurrent.hpp"

// Include standard C++ libraries
#include <string>
#include <map>
#include <algorithm>
#include <cstdio>
#include <cstring>

#define LE_ELEMENT_NAME_LENGTH 8
#define LE_ELEMENT_ARGUMENT_LENGTH 4

/**
 * @brief Enum class to define the types of elements.
 */
enum class le_Element_Type : int8_t {
    LE_AND = 0,
    LE_OR = 1,
    LE_NOT = 2,
    LE_RTRIG = 3,
    LE_FTRIG = 4,
    LE_NODE_DIGITAL = 10,
    LE_NODE_ANALOG = 11,
    LE_TIMER = 20,
    LE_COUNTER = 21,
    LE_OVERCURRENT = 30,
    LE_INVALID = -1
};

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
        int8_t type;
        le_Element_Argument_TypeDef args[5];
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
         * @brief Sets the output connection.
         * @param elementName The name of the element.
         * @param outputSlot The output slot.
         */
        void SetOutput(const char(&elementName)[LE_ELEMENT_NAME_LENGTH], uint16_t outputSlot);
        void SetOutput(std::string elementName, uint16_t outputSlot);

        /**
         * @brief Adds an input connection.
         * @param elementName The name of the element.
         * @param inputSlot The input slot.
         */
        void AddInput(const char(&elementName)[LE_ELEMENT_NAME_LENGTH], uint16_t inputSlot);
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
     * @param timeStep The current timestamp.
     */
    void Update(float timeStep);

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
     */
    void Print();

private:
    /**
     * @brief Adds an element with a specified name to the engine.
     * @param name The name of the element.
     * @param e The element.
     * @return The added element.
     */
    le_Element* AddElement(const std::string& name, le_Element* e);

    /**
     * @brief Sorts the elements in the engine based on their update order.
     */
    void SortElements();

    std::string sName;                        ///< The name of the engine.
    std::vector<le_Element*> _elements;       ///< Vector of elements in the engine.
    std::map<std::string, le_Element*> _elementsByName; ///< Map of elements by their names.
};
