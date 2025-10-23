#include "le_DNP3Outstation.hpp"

#ifdef LE_DNP3

#include "le_Engine.hpp"
#include "opendnp3/ConsoleLogger.h"
#include "opendnp3/channel/PrintingChannelListener.h"

namespace LogicElements {

/**
 * @brief Constructs an instance of DNP3Outstation.
 */
DNP3Outstation::DNP3Outstation(DNP3OutstationConfig config) :
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
        DNP3OutstationSessionConfig* session = &outstationConfig._sessions[i];

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
            DNP3CommandHandler::Create(session),
            app,
            sessionConfig);

        // Store the outstation in the list
        outstations.push_back(outstation);
    }
}

/**
 * @brief Destroys the DNP3Outstation instance.
 */
DNP3Outstation::~DNP3Outstation()
{
    // Clean up the DNP3 manager
    delete manager;
}

/**
 * @brief Validates the points associated with the outstation sessions.
 */
void DNP3Outstation::ValidatePoints(Engine* engine)
{
    this->engine = engine;

    for (DNP3OutstationSessionConfig& session : outstationConfig._sessions)
    {
        // Validate binary inputs
        for (auto& pair : session._binaryInputs)
        {
            DNP3Point<BinaryConfig>* point = &pair.second;
            Element* element = engine->GetElement(point->elementName);

            // Set the element if found
            if (element != nullptr)
            {
                point->element = element;
            }
        }

        // Validate binary outputs
        for (auto& pair : session._binaryOutputs)
        {
            DNP3Point<BOStatusConfig>* point = &pair.second;
            Element* element = engine->GetElement(point->elementName);

            // Set the element if found
            if (element != nullptr)
            {
                point->element = element;
            }
        }

        // Validate analog inputs
        for (auto& pair : session._analogInputs)
        {
            DNP3Point<AnalogConfig>* point = &pair.second;
            Element* element = engine->GetElement(point->elementName);

            // Set the element if found
            if (element != nullptr)
            {
                point->element = element;
            }
        }

        // Validate analog outputs
        for (auto& pair : session._analogOutputs)
        {
            DNP3Point<AOStatusConfig>* point = &pair.second;
            Element* element = engine->GetElement(point->elementName);

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
void DNP3Outstation::Update()
{
    if (engine == nullptr)
        return;

    // Iterate over sessions and update their points
    for (uint8_t j = 0; j < outstationConfig._sessions.size(); j++)
    {
        DNP3OutstationSessionConfig& session = outstationConfig._sessions[j];
        UpdateBuilder builder;

        // Update binary inputs
        for (auto& pair : session._binaryInputs)
        {
            DNP3Point<BinaryConfig>* point = &pair.second;

            if (point->element != nullptr)
            {
                Node<bool>* element = (Node<bool>*)point->element;
                builder.Update(Binary(element->GetValue(), Flags(0x01), app->Now()), point->index);
            }
        }

        // Update binary outputs
        for (auto& pair : session._binaryOutputs)
        {
            DNP3Point<BOStatusConfig>* point = &pair.second;

            if (point->element != nullptr)
            {
                Node<bool>* element = (Node<bool>*)point->element;
                builder.Update(BinaryOutputStatus(element->GetValue(), Flags(0x01), app->Now()), point->index);
            }
        }

        // Update analog inputs
        for (auto& pair : session._analogInputs)
        {
            DNP3Point<AnalogConfig>* point = &pair.second;

            if (point->element != nullptr)
            {
                Node<float>* element = (Node<float>*)point->element;
                builder.Update(Analog(element->GetValue(), Flags(0x01), app->Now()), point->index);
            }
        }

        // Update analog outputs
        for (auto& pair : session._analogOutputs)
        {
            DNP3Point<AOStatusConfig>* point = &pair.second;

            if (point->element != nullptr)
            {
                Node<float>* element = (Node<float>*)point->element;
                builder.Update(AnalogOutputStatus(element->GetValue(), Flags(0x01), app->Now()), point->index);
            }
        }

        // Apply the built updates to the outstation
        outstations[j]->Apply(builder.Build());
    }
}

/**
 * @brief Enables the outstation for communication.
 */
void DNP3Outstation::Enable()
{
    for (auto& outstation : outstations)
    {
        outstation->Enable();
    }
}

/**
 * @brief Disables the outstation for communication.
 */
void DNP3Outstation::Disable()
{
    for (auto& outstation : outstations)
    {
        outstation->Disable();
    }
}

#endif

} // namespace LogicElements
