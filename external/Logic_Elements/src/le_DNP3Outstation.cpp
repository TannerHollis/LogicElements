#include "le_DNP3Outstation.hpp"

#ifdef LE_DNP3

#include "le_Engine.hpp"
#include "opendnp3/ConsoleLogger.h"
#include "opendnp3/channel/PrintingChannelListener.h"

/**
 * @brief Constructs an instance of le_DNP3Outstation.
 */
le_DNP3Outstation::le_DNP3Outstation(le_DNP3Outstation_Config config) :
    outstationConfig(config)
{
    // Initialize engine pointer
    engine = nullptr;

    // Create a default outstation application
    app = DefaultOutstationApplication::Create();

    // Create a DNP3 manager with the number of sessions
    uint8_t nSessions = config._sessions.size();
    manager = new DNP3Manager(nSessions, ConsoleLogger::Create());

    // Define log levels
    const auto logLevels = levels::NORMAL | levels::ALL_COMMS;

    // Add a TCP server channel for the outstation
    channel = manager->AddTCPServer(
        config.name,
        logLevels,
        ServerAcceptMode::CloseExisting,
        IPEndpoint(config.outstation.ip, config.outstation.port),
        PrintingChannelListener::Create());

    // Initialize outstations based on session configurations
    for (uint8_t i = 0; i < nSessions; i++)
    {
        le_DNP3Outstation_Session_Config* session = &outstationConfig._sessions[i];

        // Create outstation stack configuration
        OutstationStackConfig sessionConfig(session->dbConfig);

        // Configure outstation properties
        sessionConfig.outstation.eventBufferConfig = EventBufferConfig::AllTypes(session->eventBufferLength);
        sessionConfig.outstation.params.allowUnsolicited = session->allowUnsolicited;

        // Configure DNP3 addresses
        sessionConfig.link.LocalAddr = outstationConfig.outstation.dnp;
        sessionConfig.link.RemoteAddr = session->client.dnp;
        sessionConfig.link.KeepAliveTimeout = TimeDuration::Max();

        // Add outstation to the channel
        auto outstation = channel->AddOutstation(
            config._sessions[i].name,
            le_DNP3CommandHandler::Create(session),
            app,
            sessionConfig);

        // Store the outstation in the list
        outstations.push_back(outstation);
    }
}

/**
 * @brief Destroys the le_DNP3Outstation instance.
 */
le_DNP3Outstation::~le_DNP3Outstation()
{
    // Clean up the DNP3 manager
    delete manager;
}

/**
 * @brief Validates the points associated with the outstation sessions.
 */
void le_DNP3Outstation::ValidatePoints(le_Engine* engine)
{
    this->engine = engine;

    for (le_DNP3Outstation_Session_Config& session : outstationConfig._sessions)
    {
        // Validate binary inputs
        for (auto& pair : session._binaryInputs)
        {
            le_DNP3_Point<BinaryConfig>* point = &pair.second;
            le_Element* element = engine->GetElement(point->elementName);

            // Set the element if found
            if (element != nullptr)
            {
                point->element = element;
            }
        }

        // Validate binary outputs
        for (auto& pair : session._binaryOutputs)
        {
            le_DNP3_Point<BOStatusConfig>* point = &pair.second;
            le_Element* element = engine->GetElement(point->elementName);

            // Set the element if found
            if (element != nullptr)
            {
                point->element = element;
            }
        }

        // Validate analog inputs
        for (auto& pair : session._analogInputs)
        {
            le_DNP3_Point<AnalogConfig>* point = &pair.second;
            le_Element* element = engine->GetElement(point->elementName);

            // Set the element if found
            if (element != nullptr)
            {
                point->element = element;
            }
        }

        // Validate analog outputs
        for (auto& pair : session._analogOutputs)
        {
            le_DNP3_Point<AOStatusConfig>* point = &pair.second;
            le_Element* element = engine->GetElement(point->elementName);

            // Set the element if found
            if (element != nullptr)
            {
                point->element = element;
            }
        }
    }
}

/**
 * @brief Updates the DNP3 outstation with the latest point values.
 */
void le_DNP3Outstation::Update()
{
    if (engine == nullptr)
        return;

    // Iterate over sessions and update their points
    for (uint8_t j = 0; j < outstationConfig._sessions.size(); j++)
    {
        le_DNP3Outstation_Session_Config& session = outstationConfig._sessions[j];
        UpdateBuilder builder;

        // Update binary inputs
        for (auto& pair : session._binaryInputs)
        {
            le_DNP3_Point<BinaryConfig>* point = &pair.second;

            if (point->element != nullptr)
            {
                le_Node<bool>* element = (le_Node<bool>*)point->element;
                builder.Update(Binary(element->GetValue(0), Flags(0x01), app->Now()), point->index);
            }
        }

        // Update binary outputs
        for (auto& pair : session._binaryOutputs)
        {
            le_DNP3_Point<BOStatusConfig>* point = &pair.second;

            if (point->element != nullptr)
            {
                le_Node<bool>* element = (le_Node<bool>*)point->element;
                builder.Update(BinaryOutputStatus(element->GetValue(0), Flags(0x01), app->Now()), point->index);
            }
        }

        // Update analog inputs
        for (auto& pair : session._analogInputs)
        {
            le_DNP3_Point<AnalogConfig>* point = &pair.second;

            if (point->element != nullptr)
            {
                le_Node<float>* element = (le_Node<float>*)point->element;
                builder.Update(Analog(element->GetValue(0), Flags(0x01), app->Now()), point->index);
            }
        }

        // Update analog outputs
        for (auto& pair : session._analogOutputs)
        {
            le_DNP3_Point<AOStatusConfig>* point = &pair.second;

            if (point->element != nullptr)
            {
                le_Node<float>* element = (le_Node<float>*)point->element;
                builder.Update(AnalogOutputStatus(element->GetValue(0), Flags(0x01), app->Now()), point->index);
            }
        }

        // Apply the built updates to the outstation
        outstations[j]->Apply(builder.Build());
    }
}

/**
 * @brief Enables the outstation for communication.
 */
void le_DNP3Outstation::Enable()
{
    for (auto& outstation : outstations)
    {
        outstation->Enable();
    }
}

/**
 * @brief Disables the outstation for communication.
 */
void le_DNP3Outstation::Disable()
{
    for (auto& outstation : outstations)
    {
        outstation->Disable();
    }
}

#endif