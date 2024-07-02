#pragma once

#include "..\le_Engine.hpp"

#include "tiny-json.h"

#include <fstream>

class le_Builder
{
public:
    static le_Engine* LoadFromFile(const std::string& filePath)
    {
        // Load string from file
        char* fileStr = readFileToCharPointer(filePath);

        // Load engine with json string
        le_Engine* result = LoadFromString(fileStr);

        // Deallocate string
        delete[] fileStr;

        return result;
    }

    static le_Engine* LoadFromString(char* string)
    {
        // Specify pool size
        json_t* pool = (json_t*)malloc(sizeof(json_t) * 512);

        // Specify root
        json_t const* root = json_create(string, pool, 512);

        if (root == nullptr)
            return nullptr;

        // Get name
        json_t const* nameField = json_getProperty(root, "name");

        if (nameField == nullptr)
            return nullptr;
        
        // Create le_Engine
        le_Engine* engine = new le_Engine(json_getValue(nameField));

        // Get elements
        json_t const* elementsField = json_getProperty(root, "elements");

        if (elementsField == nullptr || JSON_ARRAY != json_getType( elementsField ))
        {
            delete engine;
            return nullptr;
        }

        // Parse elements
        ParseElements(engine, elementsField);

        // Get nets
        json_t const* netsField = json_getProperty(root, "nets");

        if (netsField == nullptr || JSON_ARRAY != json_getType(netsField))
        {
            delete engine;
            return nullptr;
        }

        // Parse nets
        ParseNets(engine, netsField);

        return engine;
    }

private:
    static char* readFileToCharPointer(const std::string& filePath) {
        std::ifstream file(filePath, std::ios::binary);
        if (!file) {
            std::cerr << "Failed to open the file." << std::endl;
            return nullptr;
        }

        // Determine the file size
        file.seekg(0, std::ios::end);
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        // Allocate memory for the file content
        char* buffer = new char[size + 1]; // +1 for the null terminator

        // Read the file content into the buffer
        if (!file.read(buffer, size)) {
            std::cerr << "Failed to read the file." << std::endl;
            delete[] buffer;
            return nullptr;
        }

        // Null-terminate the buffer
        buffer[size] = '\0';

        return buffer;
    }

    static void ParseElements(le_Engine* e, json_t const* j)
    {
        json_t const* element;
        for (element = json_getChild(j); element != 0; element = json_getSibling(element))
        {
            // Get name
            json_t const* nameField = json_getProperty(element, "name");

            if (nameField == nullptr || JSON_TEXT != json_getType(nameField))
                continue;

            // Get type
            json_t const* typeField = json_getProperty(element, "type");

            if (typeField == nullptr || JSON_INTEGER != json_getType(typeField))
                continue;

            // Create structure
            le_Engine::le_Element_Def comp;
            GetString(nameField, comp.name, LE_ELEMENT_NAME_LENGTH);
            comp.type = json_getInteger(typeField);

            // Get Arguments property
            json_t const* args = json_getProperty(element, "args");

            if (args == nullptr || JSON_ARRAY != json_getType(args))
                continue;

            // Get arguments
            ParseElementArguments(&comp, args);

            // Add Element
            e->AddElement(&comp);
        }
    }

    static void ParseElementArguments(le_Engine::le_Element_Def* comp, json_t const* j)
    {
        // Iterator
        uint8_t i = 0;

        // Define argument
        json_t const* arg;
        for (arg = json_getChild(j); arg != 0; arg = json_getSibling(arg))
        {
            // No more than 5 arguments per element
            if (i == 5)
                break;

            // Get type
            jsonType_t type = json_getType(arg);
            switch (type)
            {
            case JSON_INTEGER:
            {
                comp->args[i].uArg = (uint16_t)json_getInteger(arg);
                break;
            }

            case JSON_REAL:
            {
                comp->args[i].fArg = (float)json_getReal(arg);
                break;
            }

            case JSON_BOOLEAN:
            {
                comp->args[i].bArg = json_getBoolean(arg);
                break;
            }

            case JSON_TEXT:
            {
                GetString(arg, comp->args[i].sArg, LE_ELEMENT_ARGUMENT_LENGTH);
                break;
            }

            default:
                continue;
            }

            // Increment iterator
            i++;
        }
    }

    static void ParseNets(le_Engine* e, json_t const* j)
    {
        json_t const* net;
        for (net = json_getChild(j); net != 0; net = json_getSibling(net))
        {
            // Get output field
            json_t const* outputElementField = json_getProperty(net, "output");

            if (outputElementField == nullptr || JSON_OBJ != json_getType(outputElementField))
                continue;

            // Parse output fields
            le_Engine::le_Element_Net n;
            if (!ParseNetConnection(&n.output, outputElementField))
                continue;

            /// Get inputs field
            json_t const* inputElementsField = json_getProperty(net, "inputs");

            if (inputElementsField == nullptr || JSON_ARRAY != json_getType(inputElementsField))
                continue;

            // Parse input fields
            json_t const* inputElementField;
            for (inputElementField = json_getChild(inputElementsField); inputElementField != 0; inputElementField = json_getSibling(inputElementField))
            {
                // Create new connection
                le_Engine::le_Element_Net_Connection c;

                // If could not parse
                if(!ParseNetConnection(&c, inputElementField))
                    continue;

                // Add connection to output
                n.inputs.push_back(c);
            }

            // Add net to engine
            e->AddNet(&n);
        }
    }

    static bool ParseNetConnection(le_Engine::le_Element_Net_Connection* c, json_t const* j)
    {
        // Get output
        json_t const* nameField = json_getProperty(j, "name");

        if (nameField == nullptr || JSON_TEXT != json_getType(nameField))
            return false;

        // Get outputSlot
        json_t const* slotField = json_getProperty(j, "slot");

        if (slotField == nullptr || JSON_INTEGER != json_getType(slotField))
            return false;

        // Assign to net connection struct
        GetString(nameField, c->name, LE_ELEMENT_NAME_LENGTH);
        c->slot = (uint16_t)json_getInteger(slotField);

        return true;
    }

    static bool GetString(json_t const* j, char* buffer, uint16_t length)
    {
        // Check for string
        if (j == nullptr || JSON_TEXT != json_getType(j))
            return false;
        
        // Get json text
        std::string text = std::string(json_getValue(j));

        // Copy string and add null termination
        le_Engine::CopyAndClampString(text, buffer, LE_ELEMENT_NAME_LENGTH);
        
        return true;
    }
};