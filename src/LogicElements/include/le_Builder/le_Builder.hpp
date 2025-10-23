#pragma once

#include "le_Board.hpp"
#include "nlohmann/json.hpp"
#include <fstream>
#include <string>
#include <unordered_map>

#define MAX_POOL_SIZE 1024
#define MAX_ERROR_LENGTH 512

using json = nlohmann::json;

namespace LogicElements {

/**
 * @brief Class responsible for loading and parsing configurations for Engine and DNP3OutstationConfig.
 */
class Builder
{
public:
    /**
     * @brief Major error types that can occur during parsing.
     */
    enum class MajorError : int8_t
    {
        NONE = 0,              ///< No error.
        INV_FILE,              ///< Invalid file path or file cannot be opened.
        INV_JSON_FILE,         ///< Invalid JSON format.
        INV_ENGINE_NAME,       ///< Missing or invalid engine name in JSON.
        INV_ENGINE_COMPONENTS, ///< Invalid or missing engine components in JSON.
        INV_ENGINE_NETS,       ///< Invalid or missing engine nets in JSON.
        INV_SER,              ///< Invalid or missing SER configuration in JSON.
#ifdef LE_DNP3
        INV_DNP3_CONFIG        ///< Invalid or missing DNP3 configuration in JSON.
#endif
    };

    /**
     * @brief Converts MajorError enum to string.
     * @param error The MajorError enum value.
     * @return The corresponding string representation.
     */
    static const char* MajorErrorToString(MajorError error);

    /**
     * @brief Minor error types that can occur during parsing.
     */
    enum class MinorError : int8_t
    {
        NONE = 0,              ///< No error.
        INV_COMPONENTS_OUTPUT, ///< Invalid component output in JSON.
        INV_ENGINE_NETS,       ///< Invalid engine nets in JSON.
        INV_SER_POINT,         ///< Invalid or missing SER point in JSON.
#ifdef LE_DNP3
        INV_DNP3_SESSION,      ///< Invalid or missing DNP3 session in JSON.
        INV_DNP3_POINT         ///< Invalid or missing DNP3 point in JSON.
#endif
    };

    /**
     * @brief Converts MinorError enum to string.
     * @param error The MinorError enum value.
     * @return The corresponding string representation.
     */
    static const char* MinorErrorToString(MinorError error);

    /**
     * @brief Loads an engine configuration and optional DNP3 configuration from a file.
     * @param filePath Path to the JSON file containing the configuration.
     * @param board Reference to a pointer that will point to the loaded Board.
     * @return True if the configuration was loaded successfully, otherwise false.
     */
    static bool LoadFromFile(const std::string& filePath, Board*& board);

    /**
     * @brief Loads an engine configuration and optional DNP3 configuration from a JSON string.
     * @param jsonString JSON string containing the configuration.
     * @param board Reference to a pointer that will point to the loaded Board.
     * @return True if the configuration was loaded successfully, otherwise false.
     */
    static bool LoadConfig(const char* jsonString, Board*& board);

    /**
     * @brief Combines all error information into a single string.
     * @param buffer Pointer to the buffer where the error string will be stored.
     * @param length The maximum length of the buffer.
     * @return The length of the copied characters.
     */
    static uint16_t GetErrorString(char* buffer, uint16_t length);

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

private:
    static MajorError majorError;                        ///< Last major error encountered.
    static MinorError minorError;                        ///< Last minor error encountered.
    static char sErrorMessage[MAX_ERROR_LENGTH];         ///< Buffer for storing the last error message.
    static char sErroneousJson[MAX_ERROR_LENGTH];        ///< Buffer for storing the erroneous JSON.

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
    static void SetError(MajorError major, MinorError minor, const char* erroneousJson);

    /**
     * @brief Reads the contents of a file into a string.
     * @param filePath Path to the file to read.
     * @return The contents of the file as a string, or an empty string on error.
     */
    static std::string readFile(const std::string& filePath);

    /**
     * @brief Parses the elements section of the configuration.
     * @param board Pointer to the Board to populate.
     * @param elementsField JSON field containing the elements.
     * @return True if the elements were parsed successfully, otherwise false.
     */
    static bool ParseElements(Board* board, const json& elementsField);

    /**
     * @brief Parses the arguments for an element.
     * @param comp Pointer to the element to populate.
     * @param args JSON field containing the arguments.
     */
    static void ParseElementArguments(Engine::ElementTypeDef* comp, const json& args);

    /**
     * @brief Parses the nets section of the configuration.
     * @param board Pointer to the Board to populate.
     * @param netsField JSON field containing the nets.
     * @return True if the nets were parsed successfully, otherwise false.
     */
    static bool ParseNets(Board* board, const json& netsField);

    /**
     * @brief Parses a single net connection.
     * @param connection Pointer to the net connection to populate.
     * @param j JSON field containing the connection.
     * @return True if the connection was parsed successfully, otherwise false.
     */
    static bool ParseNetConnection(Engine::ElementNetConnectionTypeDef* connection, const json& j);

    /**
     * @brief Parses the SER section of the configuration.
     * @param board Pointer to the Board to populate.
     * @param serField JSON field containing the SER elements.
     * @return True if the SER elements were parsed successfully, otherwise false.
     */
    static bool ParseSER(Board* board, const json& serField);

#ifdef LE_DNP3
    /**
     * @brief Parses the outstation configuration for DNP3.
     * @param config Pointer to the DNP3OutstationConfig to populate.
     * @param outstationField JSON field containing the outstation configuration.
     * @return True if the outstation configuration was parsed successfully, otherwise false.
     */
    static bool ParseOutstationConfig(DNP3OutstationConfig* config, const json& outstationField);

    /**
     * @brief Parses a DNP3 address configuration.
     * @param address Reference to the DNP3Address to populate.
     * @param addressField JSON field containing the address.
     * @return True if the address was parsed successfully, otherwise false.
     */
    static bool ParseDNP3Address(DNP3Address& address, const json& addressField);

    /**
     * @brief Parses the points configuration for a DNP3 session.
     * @param session Reference to the DNP3OutstationSessionConfig to populate.
     * @param pointsField JSON field containing the points configuration.
     * @return True if the points were parsed successfully, otherwise false.
     */
    static bool ParsePoints(DNP3OutstationSessionConfig& session, const json& pointsField);
    /**
     * @brief Parses a binary input point configuration.
     * @param session Reference to the DNP3OutstationSessionConfig to populate.
     * @param pointField JSON field containing the point configuration.
     * @return True if the point was parsed successfully, otherwise false.
     */
    static bool ParseBinaryInput(DNP3OutstationSessionConfig& session, const json& pointField);

    /**
     * @brief Parses a binary output point configuration.
     * @param session Reference to the DNP3OutstationSessionConfig to populate.
     * @param pointField JSON field containing the point configuration.
     * @return True if the point was parsed successfully, otherwise false.
     */
    static bool ParseBinaryOutput(DNP3OutstationSessionConfig& session, const json& pointField);

    /**
     * @brief Parses an analog input point configuration.
     * @param session Reference to the DNP3OutstationSessionConfig to populate.
     * @param pointField JSON field containing the point configuration.
     * @return True if the point was parsed successfully, otherwise false.
     */
    static bool ParseAnalogInput(DNP3OutstationSessionConfig& session, const json& pointField);

    /**
     * @brief Parses an analog output point configuration.
     * @param session Reference to the DNP3OutstationSessionConfig to populate.
     * @param pointField JSON field containing the point configuration.
     * @return True if the point was parsed successfully, otherwise false.
     */
    static bool ParseAnalogOutput(DNP3OutstationSessionConfig& session, const json& pointField);

    /**
     * @brief Parses a string to determine the corresponding PointClass.
     * 
     * @param str The string representation of the PointClass.
     * @return The corresponding PointClass enum value.
     */
    static PointClass ParsePointClass(const std::string& str);

    /**
     * @brief Converts a string to StaticBinaryVariation enum.
     * @param str The string to convert.
     * @return The corresponding StaticBinaryVariation enum value.
     */
    static StaticBinaryVariation StringToStaticBinaryVariation(const std::string& str);

    /**
     * @brief Converts a string to EventBinaryVariation enum.
     * @param str The string to convert.
     * @return The corresponding EventBinaryVariation enum value.
     */
    static EventBinaryVariation StringToEventBinaryVariation(const std::string& str);

    /**
     * @brief Converts a string to StaticBinaryOutputStatusVariation enum.
     * @param str The string to convert.
     * @return The corresponding StaticBinaryOutputStatusVariation enum value.
     */
    static StaticBinaryOutputStatusVariation StringToStaticBinaryOutputStatusVariation(const std::string& str);

    /**
     * @brief Converts a string to EventBinaryOutputStatusVariation enum.
     * @param str The string to convert.
     * @return The corresponding EventBinaryOutputStatusVariation enum value.
     */
    static EventBinaryOutputStatusVariation StringToEventBinaryOutputStatusVariation(const std::string& str);

    /**
     * @brief Converts a string to StaticAnalogVariation enum.
     * @param str The string to convert.
     * @return The corresponding StaticAnalogVariation enum value.
     */
    static StaticAnalogVariation StringToStaticAnalogVariation(const std::string& str);

    /**
     * @brief Converts a string to EventAnalogVariation enum.
     * @param str The string to convert.
     * @return The corresponding EventAnalogVariation enum value.
     */
    static EventAnalogVariation StringToEventAnalogVariation(const std::string& str);

    /**
     * @brief Converts a string to StaticAnalogOutputStatusVariation enum.
     * @param str The string to convert.
     * @return The corresponding StaticAnalogOutputStatusVariation enum value.
     */
    static StaticAnalogOutputStatusVariation StringToStaticAnalogOutputStatusVariation(const std::string& str);

    /**
     * @brief Converts a string to EventAnalogOutputStatusVariation enum.
     * @param str The string to convert.
     * @return The corresponding EventAnalogOutputStatusVariation enum value.
     */
    static EventAnalogOutputStatusVariation StringToEventAnalogOutputStatusVariation(const std::string& str);
#endif
};

} // namespace LogicElements
