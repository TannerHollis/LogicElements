#include "Device/le_Board.hpp"

#include "Core/le_Version.hpp"

// Define a macro to handle weak attribute based on the compiler
#if defined(_MSC_VER)
#define WEAK_ATTR 
#elif defined(__GNUC__) || defined(__clang__)
#define WEAK_ATTR __attribute__((weak))
#endif

namespace LogicElements {

/**
 * @brief Constructor to initialize the Board object with specified numbers of digital inputs, analog inputs, and outputs.
 * @param nInputs_Digital Number of digital inputs.
 * @param nInputs_Analog Number of analog inputs.
 * @param nOutputs Number of outputs.
 */
    Board::Board(BoardConfig config) : 
        config(config),
        _inputs_Digital(new BoardIODigital[config.digitalInputs]),
        _inputs_Analog(new BoardIOAnalog[config.analogInputs]),
        _outputs(new BoardIODigital[config.digitalOutputs]),
        engine(nullptr),
        bEnginePaused(true),
        bIOInvalidated(true),
        bInputsNeedUpdated(false)
{
#ifdef LE_DNP3
    outstation = nullptr;
#endif
}

/**
 * @brief Destructor to clean up the Board object.
 */
Board::~Board()
{
    delete[] this->_inputs_Digital;
    delete[] this->_inputs_Analog;
    delete[] this->_outputs;

#ifdef LE_DNP3
    delete outstation;
#endif
}

/**
 * @brief Adds a digital input to the specified slot.
 * @param slot The slot to add the input to.
 * @param name The name of the input.
 * @param gpioPort The GPIO port of the input.
 * @param gpioPin The GPIO pin number of the input.
 * @param invert Whether the input is inverted.
 */
void Board::AddInput(uint16_t slot, const char* name, void* gpioPort, uint16_t gpioPin, bool invert)
{
    AddIO(slot, name, gpioPort, gpioPin, invert, true);
}

/**
 * @brief Adds an analog input to the specified slot.
 * @param slot The slot to add the input to.
 * @param name The name of the input.
 * @param addr The address of the analog value.
 */
void Board::AddInput(uint16_t slot, const char* name, float* addr)
{
    BoardIOAnalog* io = &this->_inputs_Analog[slot];
    Engine::CopyAndClampString(name, io->name, LE_ELEMENT_NAME_LENGTH);
    io->addr = addr;
    io->element = nullptr;
}

/**
 * @brief Adds an output to the specified slot.
 * @param slot The slot to add the output to.
 * @param name The name of the output.
 * @param gpioPort The GPIO port of the output.
 * @param gpioPin The GPIO pin number of the output.
 * @param invert Whether the output is inverted.
 */
void Board::AddOutput(uint16_t slot, const char* name, void* gpioPort, uint16_t gpioPin, bool invert)
{
    AddIO(slot, name, gpioPort, gpioPin, invert, false);
}

/**
 * @brief Attaches an engine to the board.
 * @param e The engine to attach.
 */
void Board::AttachEngine(Engine* engine)
{
    this->engine = engine;
    this->bIOInvalidated = true;
}

/**
 * @brief Runs the board for the specified time step.
 * @param timeStamp The current timestamp.
 */
void Board::Update(const Time& timeStamp)
{
    // Check IO Validation
    if (this->bIOInvalidated)
        this->ValidateIO();

    // If still needs validation, return
    if (this->bIOInvalidated)
        return;

    // Run engine if not paused
    if (!this->bEnginePaused)
    {
        this->engine->Update(timeStamp);
#ifdef LE_DNP3
        if (this->outstation != nullptr)
            this->outstation->Update();
#endif
    }
}

/**
 * @brief Unpauses the engine.
 */
void Board::Start()
{
    this->bEnginePaused = false;
}

/**
 * @brief Pauses the engine.
 */
void Board::Pause()
{
    this->bEnginePaused = true;
}

/**
 * @brief Flags the input for update.
 */
void Board::FlagInputForUpdate()
{
    this->bInputsNeedUpdated = true;
}

bool Board::IsPaused() const
{
    return this->bEnginePaused;
}

Engine* Board::GetEngine()
{
    return this->engine;
}

uint16_t Board::GetInfo(char* buffer, uint16_t length)
{
    return snprintf(
        buffer,
        length,
        "Device Name: %s\r\nDevice PN: %s\r\nFirmware: %s\r\nDigital Inputs: %u\r\nDigital Outputs: %u\r\nAnalog Inputs: %u\r\n",
        this->config.deviceName,
        this->config.devicePN,
        Version::GetVersion(),
        this->config.digitalInputs,
        this->config.digitalOutputs,
        this->config.analogInputs);
}

/**
 * @brief Updates a digital input. This function is weakly linked and can be overridden by board-specific implementations.
 * @param io The digital input to update.
 */
WEAK_ATTR void Board::BoardUpdateInput(BoardIODigital* io)
{
    UNUSED(io);
    // Implement board specific read
}

/**
 * @brief Updates an analog input. This function is weakly linked and can be overridden by board-specific implementations.
 * @param io The analog input to update.
 */
WEAK_ATTR void Board::BoardUpdateInput(BoardIOAnalog* io)
{
    UNUSED(io);
    // Implement board specific read
}

/**
 * @brief Updates a digital output. This function is weakly linked and can be overridden by board-specific implementations.
 * @param io The digital output to update.
 */
WEAK_ATTR void Board::BoardUpdateOutput(BoardIODigital* io)
{
    UNUSED(io);
    // Implement board specific write
}

/**
 * @brief Adds an I/O to the specified slot.
 * @param slot The slot to add the I/O to.
 * @param name The name of the I/O.
 * @param gpioPort The GPIO port of the I/O.
 * @param gpioPin The GPIO pin number of the I/O.
 * @param invert Whether the I/O is inverted.
 * @param input Whether the I/O is an input.
 */
inline void Board::AddIO(uint16_t slot, const char* name, void* gpioPort, uint16_t gpioPin, bool invert, bool input)
{
    if (input)
    {
        Engine::CopyAndClampString(name, this->_inputs_Digital[slot].name, LE_ELEMENT_NAME_LENGTH);
        this->_inputs_Digital[slot].gpioPort = gpioPort;
        this->_inputs_Digital[slot].gpioPin = gpioPin;
        this->_inputs_Digital[slot].invert = invert;
        this->_inputs_Digital[slot].element = nullptr;
    }
    else
    {
        Engine::CopyAndClampString(name, this->_outputs[slot].name, LE_ELEMENT_NAME_LENGTH);
        this->_outputs[slot].gpioPort = gpioPort;
        this->_outputs[slot].gpioPin = gpioPin;
        this->_outputs[slot].invert = invert;
        this->_outputs[slot].element = nullptr;
    }
}

/**
 * @brief Validates the I/O configurations.
 */
void Board::ValidateIO()
{
    // Check if engine is valid
    if (this->engine == nullptr)
        return;

    // Validate Analog Inputs
    for (uint16_t i = 0; i < this->config.analogInputs; i++)
    {
        this->_inputs_Analog[i].element = this->engine->GetElement(this->_inputs_Analog[i].name);
        if (this->_inputs_Analog[i].element == nullptr)
            return;
    }

    // Validate Digital Inputs
    for (uint16_t i = 0; i < this->config.digitalInputs; i++)
    {
        this->_inputs_Digital[i].element = this->engine->GetElement(this->_inputs_Digital[i].name);
        if (this->_inputs_Digital[i].element == nullptr)
            return;
    }

    // Validate Outputs
    for (uint16_t i = 0; i < this->config.digitalOutputs; i++)
    {
        this->_outputs[i].element = this->engine->GetElement(this->_outputs[i].name);
        if (this->_outputs[i].element == nullptr)
            return;
    }

#ifdef LE_DNP3
    if (outstation != nullptr)
    {
        outstation->ValidatePoints(this->engine);
        outstation->Enable();
    }
#endif

    this->bIOInvalidated = false;
}

/**
 * @brief Updates all inputs.
 */
void Board::UpdateInputs()
{
    // Update analogs
    for (uint16_t i = 0; i < this->config.analogInputs; i++)
    {
        // Get element
        Node<float>* element = (Node<float>*)this->_inputs_Analog[i].element;
        if (element == nullptr)
            continue;

        BoardUpdateInput(&_inputs_Analog[i]);
    }

    // Update digitals
    for (uint16_t i = 0; i < this->config.digitalInputs; i++)
    {
        // Get element
        Node<bool>* element = (Node<bool>*)this->_inputs_Digital[i].element;
        if (element == nullptr)
            continue;

        BoardUpdateInput(&_inputs_Digital[i]);
    }
    this->bInputsNeedUpdated = false;
}

/**
 * @brief Updates all outputs.
 */
void Board::UpdateOutputs()
{
    for (uint16_t i = 0; i < this->config.digitalOutputs; i++)
    {
        // Get the element associated with the current output
        Node<bool>* element = (Node<bool>*)this->_outputs[i].element;
        if (element == nullptr)
            continue;

        // Update the output using the board-specific implementation
        BoardUpdateOutput(&_outputs[i]);
    }
}

#ifdef LE_DNP3
void Board::AttachDNP3Outstation(DNP3Outstation* outstation)
{
    this->bIOInvalidated = true;
    this->outstation = outstation;
}
#endif

} // namespace LogicElements
