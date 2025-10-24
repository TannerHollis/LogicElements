#include "Device/le_Board.hpp"
#include "Core/le_Version.hpp"

namespace LogicElements {

/**
 * @brief Constructor to create a "blank canvas" board with specified I/O counts.
 * @param deviceName Name of the device.
 * @param devicePN Part number of the device.
 * @param numDigitalInputs Number of digital inputs.
 * @param numDigitalOutputs Number of digital outputs.
 * @param numAnalogInputs Number of analog inputs.
 * @param hal Pointer to platform-specific HAL implementation.
 */
Board::Board(const char* deviceName, const char* devicePN,
             uint16_t numDigitalInputs, uint16_t numDigitalOutputs,
             uint16_t numAnalogInputs, BoardHAL* hal) :
    engine(nullptr),
    hal(hal),
    bEnginePaused(true),
    bIOInvalidated(true),
    bInputsNeedUpdated(false),
    config(deviceName, devicePN),
    _inputs_Digital(new BoardIODigital[numDigitalInputs]),
    _inputs_Analog(new BoardIOAnalog[numAnalogInputs]),
    _outputs(new BoardIODigital[numDigitalOutputs])
{
    // Set I/O counts in config
    config.digitalInputs = numDigitalInputs;
    config.digitalOutputs = numDigitalOutputs;
    config.analogInputs = numAnalogInputs;
    
    // Initialize HAL
    if (hal)
    {
        if (!hal->Init())
        {
            printf("[Board] Warning: HAL initialization failed for platform: %s\n", 
                   hal->GetPlatformName());
        }
        else
        {
            printf("[Board] Initialized with HAL: %s\n", hal->GetPlatformName());
        }
    }

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
 * @param port The GPIO port number.
 * @param pin The GPIO pin number.
 * @param invert Whether the input is inverted.
 */
void Board::AddInput(uint16_t slot, const char* name, uint32_t port, uint32_t pin, bool invert)
{
    AddIO(slot, name, port, pin, invert, true);
}

/**
 * @brief Adds an analog input to the specified slot.
 * @param slot The slot to add the input to.
 * @param name The name of the input.
 * @param port The GPIO port number.
 * @param pin The GPIO pin number.
 */
void Board::AddAnalogInput(uint16_t slot, const char* name, uint32_t port, uint32_t pin)
{
    BoardIOAnalog* io = &this->_inputs_Analog[slot];
    Engine::CopyAndClampString(name, io->name, LE_ELEMENT_NAME_LENGTH);
    io->gpio = GPIOPin(port, pin);
    io->element = nullptr;
    
    // Configure pin via HAL
    if (hal)
    {
        hal->ConfigureAnalogInput(io->gpio);
    }
}

/**
 * @brief Adds a digital output to the specified slot.
 * @param slot The slot to add the output to.
 * @param name The name of the output.
 * @param port The GPIO port number.
 * @param pin The GPIO pin number.
 * @param invert Whether the output is inverted.
 */
void Board::AddOutput(uint16_t slot, const char* name, uint32_t port, uint32_t pin, bool invert)
{
    AddIO(slot, name, port, pin, invert, false);
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
 * @brief Updates a digital input using HAL.
 * @param io The digital input to update.
 */
void Board::BoardUpdateInput(BoardIODigital* io)
{
    if (!hal) return;
    
    // Read digital value from HAL
    bool value = hal->ReadDigital(io->gpio);
    
    // Apply inversion if configured
    if (io->invert)
        value = !value;
    
    // Update the associated element
    NodeDigital* node = (NodeDigital*)io->element;
    if (node)
        node->SetValue(value);
}

/**
 * @brief Updates an analog input using HAL.
 * @param io The analog input to update.
 */
void Board::BoardUpdateInput(BoardIOAnalog* io)
{
    if (!hal) return;
    
    // Read analog value from HAL
    float value;
    if (hal->ReadAnalog(io->gpio, value))
    {
        // Update the associated element
        NodeAnalog* node = (NodeAnalog*)io->element;
        if (node)
            node->SetValue(value);
    }
}

/**
 * @brief Updates a digital output using HAL.
 * @param io The digital output to update.
 */
void Board::BoardUpdateOutput(BoardIODigital* io)
{
    if (!hal) return;
    
    // Get value from associated element
    NodeDigital* node = (NodeDigital*)io->element;
    if (!node) return;
    
    bool value = node->GetOutput();
    
    // Apply inversion if configured
    if (io->invert)
        value = !value;
    
    // Write to hardware via HAL
    hal->WriteDigital(io->gpio, value);
}

/**
 * @brief Adds a digital I/O to the specified slot.
 * @param slot The slot to add the I/O to.
 * @param name The name of the I/O.
 * @param port The GPIO port number.
 * @param pin The GPIO pin number.
 * @param invert Whether the I/O is inverted.
 * @param input Whether the I/O is an input.
 */
inline void Board::AddIO(uint16_t slot, const char* name, uint32_t port, uint32_t pin, bool invert, bool input)
{
    if (input)
    {
        Engine::CopyAndClampString(name, this->_inputs_Digital[slot].name, LE_ELEMENT_NAME_LENGTH);
        this->_inputs_Digital[slot].gpio = GPIOPin(port, pin);
        this->_inputs_Digital[slot].invert = invert;
        this->_inputs_Digital[slot].element = nullptr;
        
        // Configure pin via HAL
        if (hal)
        {
            hal->ConfigureDigitalInput(this->_inputs_Digital[slot].gpio);
        }
    }
    else
    {
        Engine::CopyAndClampString(name, this->_outputs[slot].name, LE_ELEMENT_NAME_LENGTH);
        this->_outputs[slot].gpio = GPIOPin(port, pin);
        this->_outputs[slot].invert = invert;
        this->_outputs[slot].element = nullptr;
        
        // Configure pin via HAL
        if (hal)
        {
            hal->ConfigureDigitalOutput(this->_outputs[slot].gpio);
        }
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
