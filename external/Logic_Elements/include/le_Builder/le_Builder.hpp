#pragma once

#include "..\le_Engine.hpp"
#include "tiny-json.h"
#include <fstream>
#include <string>
#include <unordered_map>

#define MAX_POOL_SIZE 1024
#define MAX_ERROR_LENGTH 512

/**
 * @brief Class responsible for loading and parsing configurations for le_Engine and le_DNP3Outstation_Config.
 */
class le_Builder
{
public:
    /**
     * @brief Major error types that can occur during parsing.
     */
    enum class MajorError : int8_t
    {
        NONE = 0,
        INV_FILE,              ///< Invalid file path or file cannot be opened.
        INV_JSON_FILE,         ///< Invalid JSON format.
        INV_ENGINE_NAME,       ///< Missing or invalid engine name in JSON.
        INV_ENGINE_COMPONENTS, ///< Invalid or missing engine components in JSON.
        INV_ENGINE_NETS,       ///< Invalid or missing engine nets in JSON.
        INV_DNP3_CONFIG        ///< Invalid or missing DNP3 configuration in JSON.
    };

    /**
     * @brief Minor error types that can occur during parsing.
     */
    enum class MinorError : int8_t
    {
        NONE = 0,
        INV_COMPONENTS_OUTPUT, ///< Invalid component output in JSON.
        INV_ENGINE_NETS,       ///< Invalid engine nets in JSON.
        INV_DNP3_SESSION,      ///< Invalid or missing DNP3 session in JSON.
        INV_DNP3_POINT         ///< Invalid or missing DNP3 point in JSON.
    };

    /**
     * @brief Loads an engine configuration and optional DNP3 configuration from a file.
     * @param filePath Path to the JSON file containing the configuration.
     * @param engine Reference to a pointer that will point to the loaded le_Engine.
     * @param dnp3Config Reference to a pointer that will point to the loaded le_DNP3Outstation_Config, or nullptr if not present.
     * @return True if the configuration was loaded successfully, otherwise false.
     */
    static bool LoadFromFile(const std::string& filePath, le_Engine*& engine, le_DNP3Outstation_Config*& dnp3Config);

    /**
     * @brief Loads an engine configuration and optional DNP3 configuration from a JSON string.
     * @param jsonString JSON string containing the configuration.
     * @param engine Reference to a pointer that will point to the loaded le_Engine.
     * @param dnp3Config Reference to a pointer that will point to the loaded le_DNP3Outstation_Config, or nullptr if not present.
     * @return True if the configuration was loaded successfully, otherwise false.
     */
    static bool LoadConfig(const char* jsonString, le_Engine*& engine, le_DNP3Outstation_Config*& dnp3Config);

    /**
     * @brief Combines all error information into a single string.
     * @param buffer Pointer to the buffer where the error string will be stored.
     * @param length The maximum length of the buffer.
     */
        static void GetErrorString(char* buffer, uint16_t length);

private:
    static MajorError majorError;                        ///< Last major error encountered.
    static MinorError minorError;                        ///< Last minor error encountered.
    static char sErrorMessage[MAX_ERROR_LENGTH];         ///< Buffer for storing the last error message.
    static char sErroneousJson[MAX_ERROR_LENGTH];        ///< Buffer for storing the erroneous JSON.

    /**
     * @brief Retrieves the last major error that occurred during parsing.
     * @return The last major error.
     */
    static MajorError GetMajorError();

    /**
     * @brief Retrieves the last minor error that occurred during parsing.
     * @return The last minor error.
     */
    static MinorError GetMinorError();

    /**
     * @brief Retrieves the last error message generated during parsing.
     * @return The last error message.
     */
    static const char* GetErrorMessage();

    /**
     * @brief Retrieves the JSON string where the error occurred.
     * @return The erroneous JSON string.
     */
    static const char* GetErroneousJson();

    /**
     * @brief Clears the error state.
     */
    static void ClearErrors();

    /**
     * @brief Sets the error state and stores the erroneous JSON.
     * @param major The major error to set.
     * @param minor The minor error to set.
     * @param erroneousJson The JSON string where the error occurred.
     * @return Always returns nullptr.
     */
    static le_Engine* SetError(MajorError major, MinorError minor, const char* erroneousJson);

    /**
     * @brief Reads the contents of a file into a string.
     * @param filePath Path to the file to read.
     * @return The contents of the file as a string, or an empty string on error.
     */
    static std::string readFile(const std::string& filePath);

    /**
     * @brief Parses the elements section of the configuration.
     * @param engine Pointer to the le_Engine to populate.
     * @param elementsField JSON field containing the elements.
     * @return True if the elements were parsed successfully, otherwise false.
     */
    static bool ParseElements(le_Engine* engine, json_t const* elementsField);

    /**
     * @brief Parses the arguments for an element.
     * @param comp Pointer to the element to populate.
     * @param args JSON field containing the arguments.
     */
    static void ParseElementArguments(le_Engine::le_Element_TypeDef* comp, json_t const* args);

    /**
     * @brief Parses the nets section of the configuration.
     * @param engine Pointer to the le_Engine to populate.
     * @param netsField JSON field containing the nets.
     * @return True if the nets were parsed successfully, otherwise false.
     */
    static bool ParseNets(le_Engine* engine, json_t const* netsField);

    /**
     * @brief Parses a single net connection.
     * @param connection Pointer to the net connection to populate.
     * @param j JSON field containing the connection.
     * @return True if the connection was parsed successfully, otherwise false.
     */
    static bool ParseNetConnection(le_Engine::le_Element_Net_Connection_TypeDef* connection, json_t const* j);

    /**
     * @brief Parses the outstation configuration for DNP3.
     * @param config Pointer to the le_DNP3Outstation_Config to populate.
     * @param outstationField JSON field containing the outstation configuration.
     * @return True if the outstation configuration was parsed successfully, otherwise false.
     */
    static bool ParseOutstationConfig(le_DNP3Outstation_Config* config, json_t const* outstationField);

    /**
     * @brief Parses a DNP3 address configuration.
     * @param address Reference to the le_DNP3_Address to populate.
     * @param addressField JSON field containing the address.
     * @return True if the address was parsed successfully, otherwise false.
     */
    static bool ParseDNP3Address(le_DNP3_Address& address, json_t const* addressField);

    /**
     * @brief Parses the points configuration for a DNP3 session.
     * @param session Reference to the le_DNP3Outstation_Session_Config to populate.
     * @param pointsField JSON field containing the points configuration.
     * @return True if the points were parsed successfully, otherwise false.
     */
    static bool ParsePoints(le_DNP3Outstation_Session_Config& session, json_t const* pointsField);

    // String-to-enum conversion functions for various DNP3 variations
    static StaticBinaryVariation StringToStaticBinaryVariation(const char* str);
    static EventBinaryVariation StringToEventBinaryVariation(const char* str);
    static StaticBinaryOutputStatusVariation StringToStaticBinaryOutputStatusVariation(const char* str);
    static EventBinaryOutputStatusVariation StringToEventBinaryOutputStatusVariation(const char* str);
    static StaticAnalogVariation StringToStaticAnalogVariation(const char* str);
    static EventAnalogVariation StringToEventAnalogVariation(const char* str);
    static StaticAnalogOutputStatusVariation StringToStaticAnalogOutputStatusVariation(const char* str);
    static EventAnalogOutputStatusVariation StringToEventAnalogOutputStatusVariation(const char* str);
};
