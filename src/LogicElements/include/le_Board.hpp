#pragma once

#include "le_Engine.hpp"

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
        void* gpioPort; ///< GPIO port
        uint16_t gpioPin; ///< GPIO pin number

        bool invert; ///< Inversion flag
        Element* element; ///< Associated element (will be cast to access digital port)
    } BoardIODigital;

    /**
     * @brief Structure representing an analog I/O on the board.
     */
    typedef struct BoardIOAnalog
    {
        char name[LE_ELEMENT_NAME_LENGTH]; ///< Name of the I/O
        float* addr; ///< Address of the analog value

        Element* element; ///< Associated element (will be cast to access analog port)
    } BoardIOAnalog;

public:
    /**
     * @brief Constructor to initialize the Board object with specified numbers of digital inputs, analog inputs, and outputs.
     * @param nInputs_Digital Number of digital inputs.
     * @param nInputs_Analog Number of analog inputs.
     * @param nOutputs Number of outputs.
     */
    Board(BoardConfig config);

    /**
     * @brief Destructor to clean up the Board object.
     */
    ~Board();

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
    static void BoardUpdateInput(BoardIODigital* io);

    /**
     * @brief Updates an analog input.
     * @param io The analog input to update.
     */
    static void BoardUpdateInput(BoardIOAnalog* io);

    /**
     * @brief Updates a digital output.
     * @param io The digital output to update.
     */
    static void BoardUpdateOutput(BoardIODigital* io);

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

    Engine* engine; ///< Pointer to the attached engine
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
