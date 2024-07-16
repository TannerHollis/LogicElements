#pragma once

#include "le_Engine.hpp"

/**
 * @brief Class representing a board with digital and analog inputs and outputs.
 */
class le_Board
{
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
    le_Board(uint16_t nInputs_Digital, uint16_t nInputs_Analog, uint16_t nOutputs);

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
    void AttachEngine(le_Engine* e);

    /**
     * @brief Runs the board for the specified time step.
     * @param timeStep The time step for running the board.
     */
    void Run(float timeStep);

    /**
     * @brief Unpauses the engine.
     */
    void UnpauseEngine();

    /**
     * @brief Flags the input for update.
     */
    void FlagInputForUpdate();

    /**
     * @brief Checks if the engine is paused.
     * @return True if the engine is paused, false otherwise.
     */
    bool IsPaused()
    {
        return this->bEnginePaused;
    }

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
    void AddIO(uint16_t slot, const char* name, void* gpioPort, uint16_t gpioPin, bool invert, bool input);

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

    le_Engine* e; ///< Pointer to the attached engine
    bool bEnginePaused; ///< Engine paused flag
    bool bIOInvalidated; ///< I/O invalidation flag

    uint16_t nInputs_Digital; ///< Number of digital inputs
    le_Board_IO_Digital* _inputs_Digital; ///< Array of digital inputs
    uint16_t nInputs_Analog; ///< Number of analog inputs
    le_Board_IO_Analog* _inputs_Analog; ///< Array of analog inputs
    bool bInputsNeedUpdated; ///< Inputs update flag

    uint16_t nOutputs; ///< Number of outputs
    le_Board_IO_Digital* _outputs; ///< Array of outputs
    bool bOutputsNeedUpdated; ///< Outputs update flag
};
