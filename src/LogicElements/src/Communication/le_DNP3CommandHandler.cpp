// DNP3CommandHandler.cpp
#include "Communication/le_DNP3CommandHandler.hpp"

#ifdef LE_DNP3

#include "Core/le_Engine.hpp"

namespace LogicElements {

/**
 * @brief Handles the selection of a Control Relay Output Block (CROB) command.
 *
 * @param command The CROB command to be processed.
 * @param index The index of the binary output in the session.
 * @return CommandStatus indicating the result of the operation.
 */
CommandStatus DNP3CommandHandler::Select(const ControlRelayOutputBlock& command, uint16_t index)
{
    // Check if the index is out of range
    if (index >= session->_binaryOutputs.size())
        return CommandStatus::OUT_OF_RANGE;

    // Retrieve the point configuration
    DNP3Point<BOStatusConfig> point = session->_binaryOutputs[index];

    // Check for null reference to the element
    if (point.element == nullptr)
        return CommandStatus::UNDEFINED;

    // Cast the element to the appropriate type
    Node<bool>* node = (Node<bool>*)point.element;

    // Check if the element is already overridden
    if (node->IsOverridden())
        return CommandStatus::ALREADY_ACTIVE;

    // Get the current value of the node
    bool value = node->GetValue();

    // Handle the CROB command based on its operation type
    switch (command.opType)
    {
    case OperationType::LATCH_ON:
        return (value == false) ? CommandStatus::SUCCESS : CommandStatus::CANCELLED;

    case OperationType::LATCH_OFF:
        return (value == true) ? CommandStatus::SUCCESS : CommandStatus::CANCELLED;

    case OperationType::PULSE_ON:
        return (value == false) ? CommandStatus::SUCCESS : CommandStatus::CANCELLED;

    case OperationType::PULSE_OFF:
        return (value == true) ? CommandStatus::SUCCESS : CommandStatus::CANCELLED;

    default:
        return CommandStatus::CANCELLED;
    }
}

/**
 * @brief Handles the operation of a Control Relay Output Block (CROB) command.
 *
 * @param command The CROB command to be processed.
 * @param index The index of the binary output in the session.
 * @param handler Reference to the IUpdateHandler for updating the status.
 * @param opType The operation type (e.g., SelectBeforeOperate, DirectOperate).
 * @return CommandStatus indicating the result of the operation.
 */
CommandStatus DNP3CommandHandler::Operate(const ControlRelayOutputBlock& command, uint16_t index, IUpdateHandler& handler, OperateType opType)
{
    switch (opType)
    {
    case OperateType::SelectBeforeOperate:
    {
        // Check if the index is out of range
        if (index >= session->_binaryOutputs.size())
            return CommandStatus::OUT_OF_RANGE;

        // Retrieve the point configuration
        DNP3Point<BOStatusConfig> point = session->_binaryOutputs[index];

        // Check for null reference to the element
        if (point.element == nullptr)
            return CommandStatus::UNDEFINED;

        // Cast the element to the appropriate type
        Node<bool>* node = (Node<bool>*)point.element;

        // Handle the CROB command based on its operation type
        switch (command.opType)
        {
        case OperationType::LATCH_ON:
            node->SetValue(true);
            handler.Update(BinaryOutputStatus(node->GetValue()), index);
            return CommandStatus::SUCCESS;

        case OperationType::LATCH_OFF:
            node->SetValue(false);
            handler.Update(BinaryOutputStatus(node->GetValue()), index);
            return CommandStatus::SUCCESS;

        case OperationType::PULSE_ON:
            node->OverrideValue(true, command.onTimeMS / 1000.0f);
            return CommandStatus::SUCCESS;

        case OperationType::PULSE_OFF:
            node->OverrideValue(true, command.offTimeMS / 1000.0f);
            return CommandStatus::SUCCESS;

        default:
            return CommandStatus::CANCELLED;
        }
    }

    case OperateType::DirectOperate:
        return CommandStatus::BLOCKED;

    case OperateType::DirectOperateNoAck:
        return CommandStatus::BLOCKED;

    default:
        return CommandStatus::UNDEFINED;
    }
}

/**
 * @brief Handles the selection of an Analog Output command (16-bit).
 *
 * @param command The Analog Output command to be processed.
 * @param index The index of the analog output in the session.
 * @return CommandStatus indicating the result of the operation.
 */
CommandStatus DNP3CommandHandler::Select(const AnalogOutputInt16& command, uint16_t index)
{
    return AnalogSelect(index);
}

/**
 * @brief Handles the operation of an Analog Output command (16-bit).
 *
 * @param command The Analog Output command to be processed.
 * @param index The index of the analog output in the session.
 * @param handler Reference to the IUpdateHandler for updating the status.
 * @param opType The operation type (e.g., SelectBeforeOperate, DirectOperate).
 * @return CommandStatus indicating the result of the operation.
 */
CommandStatus DNP3CommandHandler::Operate(const AnalogOutputInt16& command, uint16_t index, IUpdateHandler& handler, OperateType opType)
{
    return AnalogOperate(command.value, index, handler, opType);
}

/**
 * @brief Handles the selection of an Analog Output command (32-bit integer).
 *
 * @param command The Analog Output command to be processed.
 * @param index The index of the analog output in the session.
 * @return CommandStatus indicating the result of the operation.
 */
CommandStatus DNP3CommandHandler::Select(const AnalogOutputInt32& command, uint16_t index)
{
    return AnalogSelect(index);
}

/**
 * @brief Handles the operation of an Analog Output command (32-bit integer).
 *
 * @param command The Analog Output command to be processed.
 * @param index The index of the analog output in the session.
 * @param handler Reference to the IUpdateHandler for updating the status.
 * @param opType The operation type (e.g., SelectBeforeOperate, DirectOperate).
 * @return CommandStatus indicating the result of the operation.
 */
CommandStatus DNP3CommandHandler::Operate(const AnalogOutputInt32& command, uint16_t index, IUpdateHandler& handler, OperateType opType)
{
    return AnalogOperate(command.value, index, handler, opType);
}

/**
 * @brief Handles the selection of an Analog Output command (32-bit float).
 *
 * @param command The Analog Output command to be processed.
 * @param index The index of the analog output in the session.
 * @return CommandStatus indicating the result of the operation.
 */
CommandStatus DNP3CommandHandler::Select(const AnalogOutputFloat32& command, uint16_t index)
{
    return AnalogSelect(index);
}

/**
 * @brief Handles the operation of an Analog Output command (32-bit float).
 *
 * @param command The Analog Output command to be processed.
 * @param index The index of the analog output in the session.
 * @param handler Reference to the IUpdateHandler for updating the status.
 * @param opType The operation type (e.g., SelectBeforeOperate, DirectOperate).
 * @return CommandStatus indicating the result of the operation.
 */
CommandStatus DNP3CommandHandler::Operate(const AnalogOutputFloat32& command, uint16_t index, IUpdateHandler& handler, OperateType opType)
{
    return AnalogOperate(command.value, index, handler, opType);
}

/**
 * @brief Handles the selection of an Analog Output command (64-bit double).
 *
 * @param command The Analog Output command to be processed.
 * @param index The index of the analog output in the session.
 * @return CommandStatus indicating the result of the operation.
 */
CommandStatus DNP3CommandHandler::Select(const AnalogOutputDouble64& command, uint16_t index)
{
    return AnalogSelect(index);
}

/**
 * @brief Handles the operation of an Analog Output command (64-bit double).
 *
 * @param command The Analog Output command to be processed.
 * @param index The index of the analog output in the session.
 * @param handler Reference to the IUpdateHandler for updating the status.
 * @param opType The operation type (e.g., SelectBeforeOperate, DirectOperate).
 * @return CommandStatus indicating the result of the operation.
 */
CommandStatus DNP3CommandHandler::Operate(const AnalogOutputDouble64& command, uint16_t index, IUpdateHandler& handler, OperateType opType)
{
    return AnalogOperate(command.value, index, handler, opType);
}

/**
 * @brief Helper function for selecting an analog output.
 *
 * @param index The index of the analog output in the session.
 * @return CommandStatus indicating the result of the operation.
 */
CommandStatus DNP3CommandHandler::AnalogSelect(uint16_t index)
{
    // Check if the index is out of range
    if (index >= session->_analogOutputs.size())
        return CommandStatus::OUT_OF_RANGE;

    // Retrieve the point configuration
    DNP3Point<AOStatusConfig> point = session->_analogOutputs[index];

    // Check for null reference to the element
    Node<float>* node = (Node<float>*)point.element;
    if (node == nullptr)
        return CommandStatus::UNDEFINED;

    // Return success (additional logic can be added here if needed)
    return CommandStatus::SUCCESS;
}

/**
 * @brief Helper function for operating on an analog output.
 *
 * @param value The value to set for the analog output.
 * @param index The index of the analog output in the session.
 * @param handler Reference to the IUpdateHandler for updating the status.
 * @param opType The operation type (e.g., SelectBeforeOperate, DirectOperate).
 * @return CommandStatus indicating the result of the operation.
 */
CommandStatus DNP3CommandHandler::AnalogOperate(float value, uint16_t index, IUpdateHandler& handler, OperateType opType)
{
    switch (opType)
    {
    case OperateType::SelectBeforeOperate:
    {
        // Check if the index is out of range
        if (index >= session->_analogOutputs.size())
            return CommandStatus::OUT_OF_RANGE;

        // Retrieve the point configuration
        DNP3Point<AOStatusConfig> point = session->_analogOutputs[index];

        // Check for null reference to the element
        Node<bool>* node = (Node<bool>*)point.element;
        if (node == nullptr)
            return CommandStatus::UNDEFINED;

        // Set the value and update the handler
        node->SetValue(value);
        handler.Update(AnalogOutputStatus(node->GetValue()), index);
        return CommandStatus::SUCCESS;
    }

    case OperateType::DirectOperate:
        return CommandStatus::BLOCKED;

    case OperateType::DirectOperateNoAck:
        return CommandStatus::BLOCKED;

    default:
        return CommandStatus::UNDEFINED;
    }
}

#endif

} // namespace LogicElements
