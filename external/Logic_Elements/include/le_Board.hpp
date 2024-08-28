#pragma once

#include "le_Engine.hpp"

#define BOARD_ID_LENGTH 32

/**
 * @brief Class representing a board with digital and analog inputs and outputs.
 */
class le_Board
{
public:
    typedef struct le_Board_Config
    {
        char deviceName[BOARD_ID_LENGTH];
        char devicePN[BOARD_ID_LENGTH];
        uint16_t digitalInputs;
        uint16_t digitalOutputs;
        uint16_t analogInputs;

        le_Board_Config(std::string name, std::string pn) : digitalInputs(0), digitalOutputs(0), analogInputs(0)
        {
            le_Engine::CopyAndClampString(name, this->deviceName, BOARD_ID_LENGTH);
            le_Engine::CopyAndClampString(pn, this->devicePN, BOARD_ID_LENGTH);
        }
    } le_Board_Config;

private:
    /**
     * @brief Structure representing a digital I/O on the board.
     */
    typedef struct le_Board_IO_Digital
    {
        char name[LE_ELEMENT_NAME_LENGTH]; ///< Name of the I/O
        void* gpioPort; ///< GPIO port
        uint16_t gpioPin; ///< GPIO pin number

        bool invert; ///< Inversion flag
        le_Base<bool>* element; ///< Associated element
    } le_Board_IO_Digital;

    /**
     * @brief Structure representing an analog I/O on the board.
     */
    typedef struct le_Board_IO_Analog
    {
        char name[LE_ELEMENT_NAME_LENGTH]; ///< Name of the I/O
        float* addr; ///< Address of the analog value

        le_Base<float>* element; ///< Associated element
    } le_Board_IO_Analog;

public:
    /**
     * @brief Constructor to initialize the le_Board object with specified numbers of digital inputs, analog inputs, and outputs.
     * @param nInputs_Digital Number of digital inputs.
     * @param nInputs_Analog Number of analog inputs.
     * @param nOutputs Number of outputs.
     */
    le_Board(le_Board_Config config);

    /**
     * @brief Destructor to clean up the le_Board object.
     */
    ~le_Board();

    /**
     * @brief Adds a digital input to the specified slot.
     * @param slot The slot to add the input to.
     * @param name The name of the input.
     * @param gpioPort The GPIO port of the input.
     * @param gpioPin The GPIO pin number of the input.
     * @param invert Whether the input is inverted.
     */
    void AddInput(uint16_t slot, const char* name, void* gpioPort, uint16_t gpioPin, bool invert);

    /**
     * @brief Adds an analog input to the specified slot.
     * @param slot The slot to add the input to.
     * @param name The name of the input.
     * @param addr The address of the analog value.
     */
    void AddInput(uint16_t slot, const char* name, float* addr);

    /**
     * @brief Adds an output to the specified slot.
     * @param slot The slot to add the output to.
     * @param name The name of the output.
     * @param gpioPort The GPIO port of the output.
     * @param gpioPin The GPIO pin number of the output.
     * @param invert Whether the output is inverted.
     */
    void AddOutput(uint16_t slot, const char* name, void* gpioPort, uint16_t gpioPin, bool invert);

    /**
     * @brief Attaches an engine to the board.
     * @param e The engine to attach.
     */
    void AttachEngine(le_Engine* engine);

    /**
     * @brief Runs the board for the specified time step.
     * @param timeStamp The current timestamp.
     */
    void Update(const le_Time& timeStamp);

    /**
     * @brief Unpauses the engine.
     */
    void UnpauseEngine();

    /**
     * @brief Pauses the engine.
     */
    void PauseEngine();

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
    le_Engine* GetEngine();

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
    static void le_Board_UpdateInput(le_Board_IO_Digital* io);

    /**
     * @brief Updates an analog input.
     * @param io The analog input to update.
     */
    static void le_Board_UpdateInput(le_Board_IO_Analog* io);

    /**
     * @brief Updates a digital output.
     * @param io The digital output to update.
     */
    static void le_Board_UpdateOutput(le_Board_IO_Digital* io);

    /**
     * @brief Adds an I/O to the specified slot.
     * @param slot The slot to add the I/O to.
     * @param name The name of the I/O.
     * @param gpioPort The GPIO port of the I/O.
     * @param gpioPin The GPIO pin number of the I/O.
     * @param invert Whether the I/O is inverted.
     * @param input Whether the I/O is an input.
     */
    inline void AddIO(uint16_t slot, const char* name, void* gpioPort, uint16_t gpioPin, bool invert, bool input);

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

    le_Engine* engine; ///< Pointer to the attached engine
    bool bEnginePaused; ///< Engine paused flag
    bool bIOInvalidated; ///< I/O invalidation flag

    le_Board_Config config;
    le_Board_IO_Digital* _inputs_Digital; ///< Array of digital inputs
    le_Board_IO_Analog* _inputs_Analog; ///< Array of analog inputs
    bool bInputsNeedUpdated; ///< Inputs update flag

    le_Board_IO_Digital* _outputs; ///< Array of outputs

#ifdef LE_DNP3
public:
    void AttachDNP3Outstation(le_DNP3Outstation* outstation);

    le_DNP3Outstation* outstation;
#endif
};
