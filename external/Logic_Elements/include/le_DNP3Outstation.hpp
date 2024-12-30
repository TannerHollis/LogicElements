#pragma once

// Include le components
#include "le_DNP3Outstation_Config.hpp"

#ifdef LE_DNP3

#include "le_DNP3CommandHandler.hpp"

// Include necessary DNP3 library components
#include <opendnp3/DNP3Manager.h>
#include <opendnp3/channel/IChannel.h>
#include <opendnp3/outstation/IOutstation.h>
#include <opendnp3/outstation/UpdateBuilder.h>
#include <opendnp3/outstation/DefaultOutstationApplication.h>
#include <memory>
#include <vector>

/**
 * @brief A class that represents a DNP3 outstation and manages its operations.
 *
 * This class handles the creation, validation, and management of DNP3 outstation
 * sessions, including updating points and enabling/disabling the outstation.
 */
class le_DNP3Outstation
{
public:
    /**
     * @brief Constructs an instance of le_DNP3Outstation.
     *
     * @param config The configuration for the DNP3 outstation.
     */
    le_DNP3Outstation(le_DNP3Outstation_Config config);

    /**
     * @brief Destroys the le_DNP3Outstation instance.
     */
    ~le_DNP3Outstation();

    /**
     * @brief Validates the points associated with the outstation sessions.
     *
     * @param engine Pointer to the engine responsible for managing elements.
     */
    void ValidatePoints(le_Engine* engine);

    /**
     * @brief Updates the DNP3 outstation with the latest point values.
     */
    void Update();

    /**
     * @brief Enables the outstation for communication.
     */
    void Enable();

    /**
     * @brief Disables the outstation for communication.
     */
    void Disable();

private:
    le_DNP3Outstation_Config outstationConfig;  ///< Configuration for the DNP3 outstation.
    le_Engine* engine;                          ///< Pointer to the engine managing elements.

    std::shared_ptr<opendnp3::IChannel> channel;  ///< Communication channel for the outstation.
    std::vector<std::shared_ptr<opendnp3::IOutstation>> outstations;  ///< List of outstations.

    opendnp3::DNP3Manager* manager;  ///< Manager responsible for handling DNP3 operations.
    std::shared_ptr<opendnp3::IOutstationApplication> app;  ///< Application interface for the outstation.
};

#endif