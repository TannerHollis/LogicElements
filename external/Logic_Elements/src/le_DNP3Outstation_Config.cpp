#include "le_DNP3Outstation_Config.hpp"
#include "le_Engine.hpp"

/**
 * @brief Adds a binary input to the session configuration.
 */
le_DNP3_Point<BinaryConfig>* le_DNP3Outstation_Session_Config::AddBinaryInput(
    uint16_t index,
    std::string elementName,
    PointClass pointClass,
    StaticBinaryVariation sVar,
    EventBinaryVariation eVar
)
{
    // Set point properties
    le_DNP3_Point<BinaryConfig> point;
    point.elementName = elementName;
    point.element = nullptr;
    point.index = index;

    // Insert the binary input configuration into the database
    auto result = dbConfig.binary_input.insert(std::make_pair(index, BinaryConfig()));

    // Return nullptr if insertion failed
    if (!result.second)
        return nullptr;

    // Assign the configuration to the point and set its properties
    point.point = &result.first->second;
    point.point->clazz = pointClass;
    point.point->svariation = sVar;
    point.point->evariation = eVar;

    // Add the point to the map of binary inputs
    auto result2 = _binaryInputs.insert(std::make_pair(index, point));

    // Return a pointer to the newly added binary input point
    return &result2.first->second;
}

/**
 * @brief Adds a binary output to the session configuration.
 */
le_DNP3_Point<BOStatusConfig>* le_DNP3Outstation_Session_Config::AddBinaryOutput(
    uint16_t index,
    std::string elementName,
    PointClass pointClass,
    StaticBinaryOutputStatusVariation sVar,
    EventBinaryOutputStatusVariation eVar
)
{
    // Set point properties
    le_DNP3_Point<BOStatusConfig> point;
    point.elementName = elementName;
    point.element = nullptr;
    point.index = index;

    // Insert the binary output configuration into the database
    auto result = dbConfig.binary_output_status.insert(std::make_pair(index, BOStatusConfig()));

    // Return nullptr if insertion failed
    if (!result.second)
        return nullptr;

    // Assign the configuration to the point and set its properties
    point.point = &result.first->second;
    point.point->clazz = pointClass;
    point.point->svariation = sVar;
    point.point->evariation = eVar;

    // Add the point to the map of binary outputs
    auto result2 = _binaryOutputs.insert(std::make_pair(index, point));

    // Return a pointer to the newly added binary output point
    return &result2.first->second;
}

/**
 * @brief Adds an analog input to the session configuration.
 */
le_DNP3_Point<AnalogConfig>* le_DNP3Outstation_Session_Config::AddAnalogInput(
    uint16_t index,
    std::string elementName,
    PointClass pointClass,
    float deadband,
    StaticAnalogVariation sVar,
    EventAnalogVariation eVar
)
{
    // Set point properties
    le_DNP3_Point<AnalogConfig> point;
    point.elementName = elementName;
    point.element = nullptr;
    point.index = index;

    // Insert the analog input configuration into the database
    auto result = dbConfig.analog_input.insert(std::make_pair(index, AnalogConfig()));

    // Return nullptr if insertion failed
    if (!result.second)
        return nullptr;

    // Assign the configuration to the point and set its properties
    point.point = &result.first->second;
    point.point->clazz = pointClass;
    point.point->svariation = sVar;
    point.point->evariation = eVar;
    point.point->deadband = deadband;

    // Add the point to the map of analog inputs
    auto result2 = _analogInputs.insert(std::make_pair(index, point));

    // Return a pointer to the newly added analog input point
    return &result2.first->second;
}

/**
 * @brief Adds an analog output to the session configuration.
 */
le_DNP3_Point<AOStatusConfig>* le_DNP3Outstation_Session_Config::AddAnalogOutput(
    uint16_t index,
    std::string elementName,
    PointClass pointClass,
    float deadband,
    StaticAnalogOutputStatusVariation sVar,
    EventAnalogOutputStatusVariation eVar
)
{
    // Set point properties
    le_DNP3_Point<AOStatusConfig> point;
    point.elementName = elementName;
    point.element = nullptr;
    point.index = index;

    // Insert the analog output configuration into the database
    auto result = dbConfig.analog_output_status.insert(std::make_pair(index, AOStatusConfig()));

    // Return nullptr if insertion failed
    if (!result.second)
        return nullptr;

    // Assign the configuration to the point and set its properties
    point.point = &result.first->second;
    point.point->clazz = pointClass;
    point.point->svariation = sVar;
    point.point->evariation = eVar;
    point.point->deadband = deadband;

    // Add the point to the map of analog outputs
    auto result2 = _analogOutputs.insert(std::make_pair(index, point));

    // Return a pointer to the newly added analog output point
    return &result2.first->second;
}

/**
 * @brief Adds a session to the outstation configuration.
 */
le_DNP3Outstation_Session_Config* le_DNP3Outstation_Config::AddSession(le_DNP3Outstation_Session_Config session)
{
    // Add the session to the vector of sessions
    _sessions.push_back(session);

    // Return a pointer to the newly added session configuration
    return &_sessions.back();
}
