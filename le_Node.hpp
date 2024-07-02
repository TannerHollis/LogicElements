#pragma once

#include "le_Base.hpp"

template <typename T>
class le_Node : protected le_Base<T>
{
private:
	le_Node(uint16_t historyLength) : le_Base<T>(1, 1)
	{
		// Fixed length
		if (historyLength <= 0)
			historyLength = 1;

		// Set extrinsic variables
		this->uHistoryLength = historyLength;

		// Set intrinsic variables
		this->_history = new T[historyLength];
		this->uWrite = 0;
	}

	~le_Node()
	{
		delete[] this->_history;
	}
	
	void Update(float timeStep)
	{
		// Get input value
		le_Base<T>* e = (le_Base<T>*)this->_inputs[0];
		if (e != nullptr)
		{
			T inputValue = e->GetValue(this->_outputSlots[0]);

			// Set next value
			this->SetValue(0, inputValue);
		}

		// Shift read/write head
		//this->_history[this->uWrite] = this->GetValue(0);
		this->uWrite = (this->uWrite + 1) % this->uHistoryLength;
	}

	void GetHistory(T** buffer, uint16_t* startOffset)
	{
		// Get buffer with offset from circular buffer
		// TODO: Implement this
	}

	void Connect(le_Base<T>* e, uint16_t outputSlot)
	{
		// Use default connection function
		le_Element::Connect(e, outputSlot, this, 0);
	}
	
	using le_Base<T>::GetValue;
	using le_Base<T>::SetValue;
	
private:
	uint16_t uHistoryLength;
	T* _history;
	uint16_t uWrite;
	
	friend class le_Engine;
};