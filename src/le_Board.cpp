#include "le_Board.hpp"

// Define a macro to handle weak attribute based on the compiler
#if defined(_MSC_VER)
    #define WEAK_ATTR __declspec(selectany)
#elif defined(__GNUC__) || defined(__clang__)
    #define WEAK_ATTR __attribute__((weak))
#else
    #define WEAK_ATTR
#endif

le_Board::le_Board(uint16_t nInputs_Digital, uint16_t nInputs_Analog, uint16_t nOutputs)
{
	// Declare extrinsic variables
	this->nInputs_Digital = nInputs_Digital;
	this->nInputs_Analog = nInputs_Analog;
	this->nOutputs = nOutputs;

	// Declare intrinsic variables
	this->_inputs_Digital = (le_Board_IO_Digital*)malloc(sizeof(le_Board_IO_Digital) * nInputs_Digital);
	this->_inputs_Analog = (le_Board_IO_Analog*)malloc(sizeof(le_Board_IO_Analog) * nInputs_Analog);
	this->_outputs = (le_Board_IO_Digital*)malloc(sizeof(le_Board_IO_Digital) * nOutputs);
	this->e = nullptr;
	this->bEnginePaused = true;
	this->bIOInvalidated = true;
	this->bInputsNeedUpdated = false;
	this->bOutputsNeedUpdated = false;
}

le_Board::~le_Board()
{
	free(this->_inputs_Digital);
	free(this->_inputs_Analog);
	free(this->_outputs);
}

void le_Board::AddInput(uint16_t slot, const char* name, void* gpioPort, uint16_t gpioPin, bool invert)
{
	AddIO(slot, name, gpioPort, gpioPin, invert, true);
}

void le_Board::AddInput(uint16_t slot, const char* name, float* addr)
{
	le_Board_IO_Analog* io = &this->_inputs_Analog[slot];
	strncpy(io->name, name, LE_ELEMENT_NAME_LENGTH);
	io->addr = addr;
	io->element = nullptr;
}

void le_Board::AddOutput(uint16_t slot, const char* name, void* gpioPort, uint16_t gpioPin, bool invert)
{
	AddIO(slot, name, gpioPort, gpioPin, invert, false);
}

void le_Board::AttachEngine(le_Engine* e)
{
	this->e = e;
	this->bIOInvalidated = true;
}

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

void le_Board::UnpauseEngine()
{
	this->bEnginePaused = false;
}

void le_Board::FlagInputForUpdate()
{
	this->bInputsNeedUpdated = true;
}


WEAK_ATTR void le_Board::le_Board_UpdateInput(le_Board_IO_Digital* io)
{
	// Implement board specific read
}

WEAK_ATTR void le_Board::le_Board_UpdateInput(le_Board_IO_Analog* io)
{
	// Implement board specific read
}

WEAK_ATTR void le_Board::le_Board_UpdateOutput(le_Board_IO_Digital* io)
{
	// Implement board specific write
}

void le_Board::AddIO(uint16_t slot, const char* name, void* gpioPort, uint16_t gpioPin, bool invert, bool input)
{
	if (input)
	{
		strncpy(this->_inputs_Digital[slot].name, name, LE_ELEMENT_NAME_LENGTH);
		this->_inputs_Digital[slot].gpioPort = gpioPort;
		this->_inputs_Digital[slot].gpioPin = gpioPin;
		this->_inputs_Digital[slot].invert = invert;
		this->_inputs_Digital[slot].element = nullptr;
	}
	else
	{
		strncpy(this->_outputs[slot].name, name, LE_ELEMENT_NAME_LENGTH);
		this->_outputs[slot].gpioPort = gpioPort;
		this->_outputs[slot].gpioPin = gpioPin;
		this->_outputs[slot].invert = invert;
		this->_outputs[slot].element = nullptr;
	}
}

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

	this->bIOInvalidated = false;
}

void le_Board::UpdateInputs()
{
	// Update analogs
	for (uint16_t i = 0; i < this->nInputs_Analog; i++)
	{
		// Get element
		le_Node<float>* e = (le_Node<float>*)this->_inputs_Analog[i].element;
		if (e == nullptr)
			continue;

		le_Board_UpdateInput(&_inputs_Analog[i]);
	}

	// Update digitals
	for (uint16_t i = 0; i < this->nInputs_Digital; i++)
	{
		// Get element
		le_Node<bool>* e = (le_Node<bool>*)this->_inputs_Digital[i].element;
		if (e == nullptr)
			continue;

		le_Board_UpdateInput(&_inputs_Digital[i]);
	}
	this->bInputsNeedUpdated = false;
}

void le_Board::UpdateOutputs()
{
	for (uint16_t i = 0; i < this->nOutputs; i++)
	{
		// Get element
		le_Node<bool>* e = (le_Node<bool>*)this->_outputs[i].element;
		if (e == nullptr)
			continue;

		//Set element
		le_Board_UpdateOutput(&_outputs[i]);
	}
	this->bOutputsNeedUpdated = false;
}
