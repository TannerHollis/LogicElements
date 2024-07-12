#pragma once

#include "le_Engine.hpp"

class le_Board
{
private:
	typedef struct le_Board_IO_Digital
	{
		char name[LE_ELEMENT_NAME_LENGTH];
		void* gpioPort;
		uint16_t gpioPin;
		
		bool invert;
		le_Base<bool>* element;
	} le_Board_IO_Digital;

	typedef struct le_Board_IO_Analog
	{
		char name[LE_ELEMENT_NAME_LENGTH];
		float* addr;

		le_Base<float>* element;
	} le_Board_IO_Analog;

public:
	le_Board(uint16_t nInputs_Digital, uint16_t nInputs_Analog, uint16_t nOutputs);
	~le_Board();
	
	void AddInput(uint16_t slot, const char* name, void* gpioPort, uint16_t gpioPin, bool invert);
	void AddInput(uint16_t slot, const char* name, float* addr);
	
	void AddOutput(uint16_t slot, const char* name, void* gpioPort, uint16_t gpioPin, bool invert);
	
	void AttachEngine(le_Engine* e);
	
	void Run(float timeStep);
	
	void UnpauseEngine();
	
	void FlagInputForUpdate();
	
	bool IsPaused()
	{
		return this->bEnginePaused;
	}

private:
	static void le_Board_UpdateInput(le_Board_IO_Digital* io);
	static void le_Board_UpdateInput(le_Board_IO_Analog* io);
	static void le_Board_UpdateOutput(le_Board_IO_Digital* io);

	void AddIO(uint16_t slot, const char* name, void* gpioPort, uint16_t gpioPin, bool invert, bool input);
	
	void ValidateIO();
	
	void UpdateInputs();
	
	void UpdateOutputs();
	
	le_Engine* e;
	bool bEnginePaused;
	bool bIOInvalidated;
	
	uint16_t nInputs_Digital;
	le_Board_IO_Digital* _inputs_Digital;
	uint16_t nInputs_Analog;
	le_Board_IO_Analog* _inputs_Analog;
	bool bInputsNeedUpdated;

	uint16_t nOutputs;
	le_Board_IO_Digital* _outputs;
	bool bOutputsNeedUpdated;
	
};


