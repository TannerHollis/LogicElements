#include "le_Board.hpp"

// Define a macro to handle weak attribute based on the compiler
#if defined(_MSC_VER)
#define WEAK_ATTR 
#elif defined(__GNUC__) || defined(__clang__)
#define WEAK_ATTR __attribute__((weak))
#endif

/**
 * @brief Constructor to initialize the le_Board object with specified numbers of digital inputs, analog inputs, and outputs.
 * @param nInputs_Digital Number of digital inputs.
 * @param nInputs_Analog Number of analog inputs.
 * @param nOutputs Number of outputs.
 */
    le_Board::le_Board(uint16_t nInputs_Digital, uint16_t nInputs_Analog, uint16_t nOutputs)
{
    // Declare extrinsic variables
    this->nInputs_Digital = nInputs_Digital;
    this->nInputs_Analog = nInputs_Analog;
    this->nOutputs = nOutputs;

    // Declare intrinsic variables
    this->_inputs_Digital = new le_Board_IO_Digital[nInputs_Digital];
    this->_inputs_Analog = new le_Board_IO_Analog[nInputs_Analog];
    this->_outputs = new le_Board_IO_Digital[nOutputs];
    this->e = nullptr;
    this->bEnginePaused = true;
    this->bIOInvalidated = true;
    this->bInputsNeedUpdated = false;
    this->bOutputsNeedUpdated = false;

#ifdef LE_DNP3
    outstation = nullptr;
#endif
}

/**
 * @brief Destructor to clean up the le_Board object.
 */
le_Board::~le_Board()
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
void le_Board::AddInput(uint16_t slot, const char* name, void* gpioPort, uint16_t gpioPin, bool invert)
{
    AddIO(slot, name, gpioPort, gpioPin, invert, true);
}

/**
 * @brief Adds an analog input to the specified slot.
 * @param slot The slot to add the input to.
 * @param name The name of the input.
 * @param addr The address of the analog value.
 */
void le_Board::AddInput(uint16_t slot, const char* name, float* addr)
{
    le_Board_IO_Analog* io = &this->_inputs_Analog[slot];
    le_Engine::CopyAndClampString(name, io->name, LE_ELEMENT_NAME_LENGTH);
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
void le_Board::AddOutput(uint16_t slot, const char* name, void* gpioPort, uint16_t gpioPin, bool invert)
{
    AddIO(slot, name, gpioPort, gpioPin, invert, false);
}

/**
 * @brief Attaches an engine to the board.
 * @param e The engine to attach.
 */
void le_Board::AttachEngine(le_Engine* engine)
{
    this->e = engine;
    this->bIOInvalidated = true;
}

/**
 * @brief Runs the board for the specified time step.
 * @param timeStep The time step for running the board.
 */
void le_Board::Run(float timeStep)
{
    // Check IO Validation
    if (this->bIOInvalidated)
        this->ValidateIO();

    // If still needs validation, return
    if (this->bIOInvalidated)
        return;

    // Update inputs if needed
    if (this->bInputsNeedUpdated)
        this->UpdateInputs();

    // Run engine if not paused
    if (!this->bEnginePaused)
    {
        this->e->Update(timeStep);
        this->bEnginePaused = true;
        this->bOutputsNeedUpdated = true;
    }

    // Update outputs if needed
    if (this->bOutputsNeedUpdated)
        this->UpdateOutputs();
}

/**
 * @brief Unpauses the engine.
 */
void le_Board::UnpauseEngine()
{
    this->bEnginePaused = false;
}

/**
 * @brief Flags the input for update.
 */
void le_Board::FlagInputForUpdate()
{
    this->bInputsNeedUpdated = true;
}

/**
 * @brief Updates a digital input. This function is weakly linked and can be overridden by board-specific implementations.
 * @param io The digital input to update.
 */
WEAK_ATTR void le_Board::le_Board_UpdateInput(le_Board_IO_Digital* io)
{
    UNUSED(io);
    // Implement board specific read
}

/**
 * @brief Updates an analog input. This function is weakly linked and can be overridden by board-specific implementations.
 * @param io The analog input to update.
 */
WEAK_ATTR void le_Board::le_Board_UpdateInput(le_Board_IO_Analog* io)
{
    UNUSED(io);
    // Implement board specific read
}

/**
 * @brief Updates a digital output. This function is weakly linked and can be overridden by board-specific implementations.
 * @param io The digital output to update.
 */
WEAK_ATTR void le_Board::le_Board_UpdateOutput(le_Board_IO_Digital* io)
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
inline void le_Board::AddIO(uint16_t slot, const char* name, void* gpioPort, uint16_t gpioPin, bool invert, bool input)
{
    if (input)
    {
        le_Engine::CopyAndClampString(name, this->_inputs_Digital[slot].name, LE_ELEMENT_NAME_LENGTH);
        this->_inputs_Digital[slot].gpioPort = gpioPort;
        this->_inputs_Digital[slot].gpioPin = gpioPin;
        this->_inputs_Digital[slot].invert = invert;
        this->_inputs_Digital[slot].element = nullptr;
    }
    else
    {
        le_Engine::CopyAndClampString(name, this->_outputs[slot].name, LE_ELEMENT_NAME_LENGTH);
        this->_outputs[slot].gpioPort = gpioPort;
        this->_outputs[slot].gpioPin = gpioPin;
        this->_outputs[slot].invert = invert;
        this->_outputs[slot].element = nullptr;
    }
}

/**
 * @brief Validates the I/O configurations.
 */
void le_Board::ValidateIO()
{
    // Check if engine is valid
    if (this->e == nullptr)
        return;

    // Validate Analog Inputs
    for (uint16_t i = 0; i < this->nInputs_Analog; i++)
    {
        this->_inputs_Analog[i].element = (le_Base<float>*)this->e->GetElement(this->_inputs_Analog[i].name);
        if (this->_inputs_Analog[i].element == nullptr)
            return;
    }

    // Validate Digital Inputs
    for (uint16_t i = 0; i < this->nInputs_Digital; i++)
    {
        this->_inputs_Digital[i].element = (le_Base<bool>*)this->e->GetElement(this->_inputs_Digital[i].name);
        if (this->_inputs_Digital[i].element == nullptr)
            return;
    }

    // Validate Outputs
    for (uint16_t i = 0; i < this->nOutputs; i++)
    {
        this->_outputs[i].element = (le_Base<bool>*)this->e->GetElement(this->_outputs[i].name);
        if (this->_outputs[i].element == nullptr)
            return;
    }

#ifdef LE_DNP3
    if (outstation != nullptr)
        outstation->ValidatePoints(this->e);
#endif

    this->bIOInvalidated = false;
}

/**
 * @brief Updates all inputs.
 */
void le_Board::UpdateInputs()
{
    // Update analogs
    for (uint16_t i = 0; i < this->nInputs_Analog; i++)
    {
        // Get element
        le_Node<float>* element = (le_Node<float>*)this->_inputs_Analog[i].element;
        if (element == nullptr)
            continue;

        le_Board_UpdateInput(&_inputs_Analog[i]);
    }

    // Update digitals
    for (uint16_t i = 0; i < this->nInputs_Digital; i++)
    {
        // Get element
        le_Node<bool>* element = (le_Node<bool>*)this->_inputs_Digital[i].element;
        if (element == nullptr)
            continue;

        le_Board_UpdateInput(&_inputs_Digital[i]);
    }
    this->bInputsNeedUpdated = false;
}

/**
 * @brief Updates all outputs.
 */
void le_Board::UpdateOutputs()
{
    for (uint16_t i = 0; i < this->nOutputs; i++)
    {
        // Get the element associated with the current output
        le_Node<bool>* element = (le_Node<bool>*)this->_outputs[i].element;
        if (element == nullptr)
            continue;

        // Update the output using the board-specific implementation
        le_Board_UpdateOutput(&_outputs[i]);
    }
    this->bOutputsNeedUpdated = false;
}

#ifdef LE_DNP3
void le_Board::AttachDNP3Outstation(le_DNP3Outstation* outstation)
{
    this->bIOInvalidated = true;
    this->outstation = outstation;
}
#endif