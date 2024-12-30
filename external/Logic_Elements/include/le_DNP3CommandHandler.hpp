#pragma once

#ifdef LE_DNP3

// Include le components
#include "le_DNP3Outstation_Config.hpp"

// Include dnp3 libraries
#include <opendnp3/outstation/ICommandHandler.h>
#include <opendnp3/outstation/IUpdateHandler.h>

// Include STL
#include <memory>

/**
 * @brief A class that handles DNP3 command processing for the outstation.
 *
 * This class is responsible for handling the selection and operation of control
 * relay output blocks (CROB) and analog outputs. It manages the interaction with
 * the associated session configuration and update handler.
 */
class le_DNP3CommandHandler : public opendnp3::ICommandHandler
{
public:
    /**
     * @brief Constructor for le_DNP3CommandHandler.
     *
     * @param session Pointer to the outstation session configuration.
     */
    le_DNP3CommandHandler(le_DNP3Outstation_Session_Config* session)
    {
        this->session = session;
    }

    /**
     * @brief Factory method to create an instance of le_DNP3CommandHandler.
     *
     * @param session Pointer to the outstation session configuration.
     * @return A shared pointer to the created ICommandHandler.
     */
    static std::shared_ptr<opendnp3::ICommandHandler> Create(le_DNP3Outstation_Session_Config* session)
    {
        return std::make_shared<le_DNP3CommandHandler>(session);
    }

    /**
     * @brief Begins a series of commands.
     *
     * Called by the outstation to signify the beginning of a sequence of commands.
     */
    virtual void Begin() override
    {
        // No action needed
    }

    /**
     * @brief Ends a series of commands.
     *
     * Called by the outstation to signify the end of a sequence of commands.
     */
    virtual void End() override
    {
        // No action needed
    }

    /**
     * @brief Handles the selection of a Control Relay Output Block (CROB) command.
     *
     * @param command The CROB command to be processed.
     * @param index The index of the binary output in the session.
     * @return CommandStatus indicating the result of the operation.
     */
    CommandStatus Select(const opendnp3::ControlRelayOutputBlock& command, uint16_t index) override;

    /**
     * @brief Handles the operation of a Control Relay Output Block (CROB) command.
     *
     * @param command The CROB command to be processed.
     * @param index The index of the binary output in the session.
     * @param handler Reference to the IUpdateHandler for updating the status.
     * @param opType The operation type (e.g., SelectBeforeOperate, DirectOperate).
     * @return CommandStatus indicating the result of the operation.
     */
    CommandStatus Operate(const opendnp3::ControlRelayOutputBlock& command,
        uint16_t index,
        opendnp3::IUpdateHandler& handler,
        opendnp3::OperateType opType) override;

    /**
     * @brief Handles the selection of an Analog Output command (16-bit).
     *
     * @param command The Analog Output command to be processed.
     * @param index The index of the analog output in the session.
     * @return CommandStatus indicating the result of the operation.
     */
    CommandStatus Select(const opendnp3::AnalogOutputInt16& command, uint16_t index) override;

    /**
     * @brief Handles the operation of an Analog Output command (16-bit).
     *
     * @param command The Analog Output command to be processed.
     * @param index The index of the analog output in the session.
     * @param handler Reference to the IUpdateHandler for updating the status.
     * @param opType The operation type (e.g., SelectBeforeOperate, DirectOperate).
     * @return CommandStatus indicating the result of the operation.
     */
    CommandStatus Operate(const opendnp3::AnalogOutputInt16& command,
        uint16_t index,
        opendnp3::IUpdateHandler& handler,
        opendnp3::OperateType opType) override;

    /**
     * @brief Handles the selection of an Analog Output command (32-bit integer).
     *
     * @param command The Analog Output command to be processed.
     * @param index The index of the analog output in the session.
     * @return CommandStatus indicating the result of the operation.
     */
    CommandStatus Select(const opendnp3::AnalogOutputInt32& command, uint16_t index) override;

    /**
     * @brief Handles the operation of an Analog Output command (32-bit integer).
     *
     * @param command The Analog Output command to be processed.
     * @param index The index of the analog output in the session.
     * @param handler Reference to the IUpdateHandler for updating the status.
     * @param opType The operation type (e.g., SelectBeforeOperate, DirectOperate).
     * @return CommandStatus indicating the result of the operation.
     */
    CommandStatus Operate(const opendnp3::AnalogOutputInt32& command,
        uint16_t index,
        opendnp3::IUpdateHandler& handler,
        opendnp3::OperateType opType) override;

    /**
     * @brief Handles the selection of an Analog Output command (32-bit float).
     *
     * @param command The Analog Output command to be processed.
     * @param index The index of the analog output in the session.
     * @return CommandStatus indicating the result of the operation.
     */
    CommandStatus Select(const opendnp3::AnalogOutputFloat32& command, uint16_t index) override;

    /**
     * @brief Handles the operation of an Analog Output command (32-bit float).
     *
     * @param command The Analog Output command to be processed.
     * @param index The index of the analog output in the session.
     * @param handler Reference to the IUpdateHandler for updating the status.
     * @param opType The operation type (e.g., SelectBeforeOperate, DirectOperate).
     * @return CommandStatus indicating the result of the operation.
     */
    CommandStatus Operate(const opendnp3::AnalogOutputFloat32& command,
        uint16_t index,
        opendnp3::IUpdateHandler& handler,
        opendnp3::OperateType opType) override;

    /**
     * @brief Handles the selection of an Analog Output command (64-bit double).
     *
     * @param command The Analog Output command to be processed.
     * @param index The index of the analog output in the session.
     * @return CommandStatus indicating the result of the operation.
     */
    CommandStatus Select(const opendnp3::AnalogOutputDouble64& command, uint16_t index) override;

    /**
     * @brief Handles the operation of an Analog Output command (64-bit double).
     *
     * @param command The Analog Output command to be processed.
     * @param index The index of the analog output in the session.
     * @param handler Reference to the IUpdateHandler for updating the status.
     * @param opType The operation type (e.g., SelectBeforeOperate, DirectOperate).
     * @return CommandStatus indicating the result of the operation.
     */
    CommandStatus Operate(const opendnp3::AnalogOutputDouble64& command,
        uint16_t index,
        opendnp3::IUpdateHandler& handler,
        opendnp3::OperateType opType) override;

private:
    /**
     * @brief Helper function for selecting an analog output.
     *
     * @param index The index of the analog output in the session.
     * @return CommandStatus indicating the result of the operation.
     */
    CommandStatus AnalogSelect(uint16_t index);

    /**
     * @brief Helper function for operating on an analog output.
     *
     * @param value The value to set for the analog output.
     * @param index The index of the analog output in the session.
     * @param handler Reference to the IUpdateHandler for updating the status.
     * @param opType The operation type (e.g., SelectBeforeOperate, DirectOperate).
     * @return CommandStatus indicating the result of the operation.
     */
    CommandStatus AnalogOperate(float value,
        uint16_t index,
        opendnp3::IUpdateHandler& handler,
        opendnp3::OperateType opType);

    le_DNP3Outstation_Session_Config* session; ///< Pointer to the outstation session configuration.
};

#endif