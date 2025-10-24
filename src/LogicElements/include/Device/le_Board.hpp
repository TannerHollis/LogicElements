#pragma once

#include "Core/le_Engine.hpp"
#include "Device/HAL/le_BoardHAL.hpp"

namespace LogicElements {

#define BOARD_ID_LENGTH 32

/**
 * @brief Class representing a board with digital and analog inputs and outputs.
 */
class Board
{
public:
    typedef struct BoardConfig
    {
        char deviceName[BOARD_ID_LENGTH];
        char devicePN[BOARD_ID_LENGTH];
        uint16_t digitalInputs;
        uint16_t digitalOutputs;
        uint16_t analogInputs;

        BoardConfig(std::string name, std::string pn) : digitalInputs(0), digitalOutputs(0), analogInputs(0)
        {
            Engine::CopyAndClampString(name, this->deviceName, BOARD_ID_LENGTH);
            Engine::CopyAndClampString(pn, this->devicePN, BOARD_ID_LENGTH);
        }
    } BoardConfig;

private:
    /**
     * @brief Structure representing a digital I/O on the board.
     */
    typedef struct BoardIODigital
    {
        char name[LE_ELEMENT_NAME_LENGTH]; ///< Name of the I/O
        GPIOPin gpio; ///< GPIO pin (platform-agnostic)

        bool invert; ///< Inversion flag
        Element* element; ///< Associated element (will be cast to access digital port)
    } BoardIODigital;

    /**
     * @brief Structure representing an analog I/O on the board.
     */
    typedef struct BoardIOAnalog
    {
        char name[LE_ELEMENT_NAME_LENGTH]; ///< Name of the I/O
        GPIOPin gpio; ///< GPIO pin (platform-agnostic)

        Element* element; ///< Associated element (will be cast to access analog port)
    } BoardIOAnalog;

public:
    /**
     * @brief Constructor to create a "blank canvas" board with specified I/O counts
     * @param deviceName Name of the device
     * @param devicePN Part number of the device
     * @param numDigitalInputs Number of digital inputs
     * @param numDigitalOutputs Number of digital outputs
     * @param numAnalogInputs Number of analog inputs
     * @param hal Pointer to platform-specific HAL implementation
     */
    Board(const char* deviceName, const char* devicePN,
          uint16_t numDigitalInputs, uint16_t numDigitalOutputs, 
          uint16_t numAnalogInputs, BoardHAL* hal);

    /**
     * @brief Destructor to clean up the Board object.
     */
    ~Board();

    /**
     * @brief Adds a digital input to the specified slot.
     * @param slot The slot to add the input to.
     * @param name The name of the input.
     * @param port The GPIO port number (platform-specific, e.g., 0=GPIOA for STM32).
     * @param pin The GPIO pin number.
     * @param invert Whether the input is inverted.
     */
    void AddInput(uint16_t slot, const char* name, uint32_t port, uint32_t pin, bool invert);

    /**
     * @brief Adds an analog input to the specified slot.
     * @param slot The slot to add the input to.
     * @param name The name of the input.
     * @param port The GPIO port number (platform-specific).
     * @param pin The GPIO pin number.
     */
    void AddAnalogInput(uint16_t slot, const char* name, uint32_t port, uint32_t pin);

    /**
     * @brief Adds a digital output to the specified slot.
     * @param slot The slot to add the output to.
     * @param name The name of the output.
     * @param port The GPIO port number (platform-specific).
     * @param pin The GPIO pin number.
     * @param invert Whether the output is inverted.
     */
    void AddOutput(uint16_t slot, const char* name, uint32_t port, uint32_t pin, bool invert);

    /**
     * @brief Attaches an engine to the board.
     * @param e The engine to attach.
     */
    void AttachEngine(Engine* engine);

    /**
     * @brief Runs the board for the specified time step.
     * @param timeStamp The current timestamp.
     */
    void Update(const Time& timeStamp);

    /**
     * @brief Unpauses the engine.
     */
    void Start();

    /**
     * @brief Pauses the engine.
     */
    void Pause();

    /**
     * @brief Flags the input for update.
     */
    void FlagInputForUpdate();

    /**
     * @brief Checks if the engine is paused.
     * @return True if the engine is paused, false otherwise.
     */
    bool IsPaused() const;

    /**
     * @brief Returns the current attached engine.
     * @return Pointer to the current engine if any, otherwise nullptr.
     */
    Engine* GetEngine();

    /**
     * @brief Retrieves information about the board.
     * @param buffer The buffer to store the information.
     * @param length The maximum length of the buffer.
     * @return The actual length of the information written to the buffer.
     */
    uint16_t GetInfo(char* buffer, uint16_t length);

private:
    /**
     * @brief Updates a digital input.
     * @param io The digital input to update.
     */
    void BoardUpdateInput(BoardIODigital* io);

    /**
     * @brief Updates an analog input.
     * @param io The analog input to update.
     */
    void BoardUpdateInput(BoardIOAnalog* io);

    /**
     * @brief Updates a digital output.
     * @param io The digital output to update.
     */
    void BoardUpdateOutput(BoardIODigital* io);

    /**
     * @brief Adds a digital I/O to the specified slot.
     * @param slot The slot to add the I/O to.
     * @param name The name of the I/O.
     * @param port The GPIO port number.
     * @param pin The GPIO pin number.
     * @param invert Whether the I/O is inverted.
     * @param input Whether the I/O is an input.
     */
    inline void AddIO(uint16_t slot, const char* name, uint32_t port, uint32_t pin, bool invert, bool input);

    /**
     * @brief Validates the I/O configurations.
     */
    void ValidateIO();

    /**
     * @brief Updates all inputs.
     */
    void UpdateInputs();

    /**
     * @brief Updates all outputs.
     */
    void UpdateOutputs();

    Engine* engine; ///< Pointer to the attached engine
    BoardHAL* hal; ///< Platform-specific HAL implementation
    bool bEnginePaused; ///< Engine paused flag
    bool bIOInvalidated; ///< I/O invalidation flag

    BoardConfig config;
    BoardIODigital* _inputs_Digital; ///< Array of digital inputs
    BoardIOAnalog* _inputs_Analog; ///< Array of analog inputs
    bool bInputsNeedUpdated; ///< Inputs update flag

    BoardIODigital* _outputs; ///< Array of outputs

#ifdef LE_DNP3
public:
    void AttachDNP3Outstation(DNP3Outstation* outstation);

    DNP3Outstation* outstation;
#endif
};

} // namespace LogicElements
