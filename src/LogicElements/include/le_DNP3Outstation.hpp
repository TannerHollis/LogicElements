#pragma once

// Include le components
#include "le_DNP3OutstationConfig.hpp"

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

#endif

namespace LogicElements {

#ifdef LE_DNP3

/**
 * @brief A class that represents a DNP3 outstation and manages its operations.
 *
 * This class handles the creation, validation, and management of DNP3 outstation
 * sessions, including updating points and enabling/disabling the outstation.
 */
class DNP3Outstation
{
public:
    /**
     * @brief Constructs an instance of DNP3Outstation.
     *
     * @param config The configuration for the DNP3 outstation.
     */
    DNP3Outstation(DNP3OutstationConfig config);

    /**
     * @brief Destroys the DNP3Outstation instance.
     */
    ~DNP3Outstation();

    /**
     * @brief Validates the points associated with the outstation sessions.
     *
     * @param engine Pointer to the engine responsible for managing elements.
     */
    void ValidatePoints(Engine* engine);

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
    DNP3OutstationConfig outstationConfig;  ///< Configuration for the DNP3 outstation.
    Engine* engine;                          ///< Pointer to the engine managing elements.

    std::shared_ptr<opendnp3::IChannel> channel;  ///< Communication channel for the outstation.
    std::vector<std::shared_ptr<opendnp3::IOutstation>> outstations;  ///< List of outstations.

    opendnp3::DNP3Manager* manager;  ///< Manager responsible for handling DNP3 operations.
    std::shared_ptr<opendnp3::IOutstationApplication> app;  ///< Application interface for the outstation.
};

#endif

} // namespace LogicElements
