#pragma once

// Include STD C++ libs
#include <vector>
#include <cstdint>
#include <cstdlib>

class le_Element
{
public:
	le_Element(uint16_t nInputs);

	~le_Element();

	uint16_t GetOrder();

	bool operator <(le_Element& other)
	{
		return this->GetOrder() < other.GetOrder();
	}

	virtual void Update(float timeStamp);

	static void Connect(le_Element* output, uint16_t outputSlot, le_Element* input, uint16_t inputSlot);

private:
	void FindOrder(le_Element* original, uint16_t* order);

	void SetUpdateOrderFlag(le_Element* original);

private:
	// Declare order
	bool bUpdateOrder;
	uint16_t iOrder;

protected:
	// IO
	uint16_t nInputs;
	le_Element** _inputs;
	uint16_t* _outputSlots;
};