#pragma once

#include "le_Element.hpp"

// Include STD C++ libs
#include <vector>
#include <cstdint>
#include <cstdlib>

template <typename T>
class le_Node;

template <typename T>
class le_Base : public le_Element
{
protected:
	le_Base(uint16_t nInputs, uint16_t nOutputs) : le_Element(nInputs)
	{
		// Set extrinsic variables
		this->nOutputs = nOutputs;

		// Set intrinsic variables
		this->_outputs = new T[nInputs];

		// Clear input refs
		for (uint16_t i = 0; i < nInputs; i++)
		{
			this->_outputs[i] = (T)0;
		}
	}

	~le_Base()
	{
		delete[] this->_outputs;
	}

	static void Connect(le_Base* output, uint16_t outputSlot, le_Base* input, uint16_t inputSlot)
	{
		// Call base class function
		le_Element::Connect(output, outputSlot, input, inputSlot);
	}
	
	const T GetValue(uint16_t outputSlot)
	{
		return this->_outputs[outputSlot];
	}
	
	void SetValue(uint16_t outputSlot, T value)
	{
		this->_outputs[outputSlot] = value;
	}
	
	virtual void Update(float timeStep)
	{
		// Template function, nothing here...
	}

private:
	// Declare I/O
	uint16_t nOutputs;

	// Declare value
	T* _outputs;
	
	// Make friends, shake hands
	friend class le_Node<T>;
	friend class le_AND;
	friend class le_OR;
	friend class le_NOT;
	friend class le_RTrig;
	friend class le_FTrig;
	friend class le_Counter;
	friend class le_Timer;
	friend class le_Overcurrent;
	friend class le_Engine;
	friend class le_Math;
};
