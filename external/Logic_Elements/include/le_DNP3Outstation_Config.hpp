#pragma once

#ifdef LE_DNP3

// Include DNP3 libraries
#include <opendnp3/outstation/MeasurementConfig.h>
#include <opendnp3/outstation/DatabaseConfig.h>

// Include STL libraries
#include <vector>
#include <string>
#include <map>

class le_Element;
class le_Engine;

using namespace opendnp3;

/**
 * @brief Template struct representing a DNP3 point configuration.
 *
 * @tparam T The type of the point (e.g., BinaryConfig, AnalogConfig).
 */
template <typename T>
struct le_DNP3_Point
{
    std::string elementName; ///< The name of the element associated with this point.
    T* point = nullptr;      ///< Pointer to the DNP3 point configuration.
    uint16_t index;          ///< The index of the point in the session.
    le_Element* element;     ///< Pointer to the element in the system.
};

/**
 * @brief Struct representing a DNP3 address configuration.
 */
typedef struct le_DNP3_Address
{
    std::string ip;  ///< IP address of the device.
    uint16_t dnp;    ///< DNP3 address.
    uint16_t port;   ///< Port number for DNP3 communication.
} le_DNP3_Address;

/**
 * @brief Struct representing the configuration for a DNP3 outstation session.
 */
typedef struct le_DNP3Outstation_Session_Config
{
    std::string name;                          ///< Name of the session.
    le_DNP3_Address client;                    ///< Client address configuration.
    uint16_t eventBufferLength;                ///< Length of the event buffer.
    bool allowUnsolicited;                     ///< Flag to allow unsolicited responses.

    DatabaseConfig dbConfig;                   ///< DNP3 database configuration.
    std::map<uint16_t, le_DNP3_Point<BinaryConfig>> _binaryInputs;   ///< Map of binary inputs.
    std::map<uint16_t, le_DNP3_Point<BOStatusConfig>> _binaryOutputs; ///< Map of binary outputs.
    std::map<uint16_t, le_DNP3_Point<AnalogConfig>> _analogInputs;   ///< Map of analog inputs.
    std::map<uint16_t, le_DNP3_Point<AOStatusConfig>> _analogOutputs; ///< Map of analog outputs.

    /**
     * @brief Adds a binary input to the session configuration.
     *
     * @param index The index of the binary input.
     * @param elementName The name of the associated element.
     * @param pointClass The DNP3 point class.
     * @param sVar The static variation for the binary input.
     * @param eVar The event variation for the binary input.
     * @return Pointer to the newly added binary input point configuration.
     */
    le_DNP3_Point<BinaryConfig>* AddBinaryInput(
        uint16_t index,
        std::string elementName,
        PointClass pointClass = PointClass::Class2,
        StaticBinaryVariation sVar = StaticBinaryVariation::Group1Var1,
        EventBinaryVariation eVar = EventBinaryVariation::Group2Var1
    );

    /**
     * @brief Adds a binary output to the session configuration.
     *
     * @param index The index of the binary output.
     * @param elementName The name of the associated element.
     * @param pointClass The DNP3 point class.
     * @param sVar The static variation for the binary output status.
     * @param eVar The event variation for the binary output status.
     * @return Pointer to the newly added binary output point configuration.
     */
    le_DNP3_Point<BOStatusConfig>* AddBinaryOutput(
        uint16_t index,
        std::string elementName,
        PointClass pointClass = PointClass::Class2,
        StaticBinaryOutputStatusVariation sVar = StaticBinaryOutputStatusVariation::Group10Var2,
        EventBinaryOutputStatusVariation eVar = EventBinaryOutputStatusVariation::Group11Var1
    );

    /**
     * @brief Adds an analog input to the session configuration.
     *
     * @param index The index of the analog input.
     * @param elementName The name of the associated element.
     * @param pointClass The DNP3 point class.
     * @param sVar The static variation for the analog input.
     * @param eVar The event variation for the analog input.
     * @return Pointer to the newly added analog input point configuration.
     */
    le_DNP3_Point<AnalogConfig>* AddAnalogInput(
        uint16_t index,
        std::string elementName,
        PointClass pointClass = PointClass::Class2,
        float deadband = 0.0f,
        StaticAnalogVariation sVar = StaticAnalogVariation::Group30Var1,
        EventAnalogVariation eVar = EventAnalogVariation::Group32Var1
    );

    /**
     * @brief Adds an analog output to the session configuration.
     *
     * @param index The index of the analog output.
     * @param elementName The name of the associated element.
     * @param pointClass The DNP3 point class.
     * @param sVar The static variation for the analog output status.
     * @param eVar The event variation for the analog output status.
     * @return Pointer to the newly added analog output point configuration.
     */
    le_DNP3_Point<AOStatusConfig>* AddAnalogOutput(
        uint16_t index,
        std::string elementName,
        PointClass pointClass = PointClass::Class2,
        float deadband = 0.0f,
        StaticAnalogOutputStatusVariation sVar = StaticAnalogOutputStatusVariation::Group40Var1,
        EventAnalogOutputStatusVariation eVar = EventAnalogOutputStatusVariation::Group42Var1
    );
} le_DNP3Outstation_Session_Config;

/**
 * @brief Struct representing the configuration for a DNP3 outstation.
 */
typedef struct le_DNP3Outstation_Config
{
    std::string name;                        ///< Name of the outstation.
    le_DNP3_Address outstation;              ///< Outstation address configuration.

    std::vector<le_DNP3Outstation_Session_Config> _sessions; ///< List of session configurations.

    /**
     * @brief Adds a session to the outstation configuration.
     *
     * @param session The session configuration to be added.
     * @return Pointer to the newly added session configuration.
     */
    le_DNP3Outstation_Session_Config* AddSession(le_DNP3Outstation_Session_Config session);
} le_DNP3Outstation_Config;

#endif