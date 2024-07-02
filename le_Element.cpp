#include "le_Element.hpp"

le_Element::le_Element(uint16_t nInputs)
{
	// Set extrinsic variables
	this->nInputs = nInputs;

	// Set intrinsic variables
	this->_inputs = new le_Element * [nInputs];
	this->_outputSlots = new uint16_t[nInputs];

	// Clear input refs
	for (uint16_t i = 0; i < nInputs; i++)
	{
		this->_outputSlots[i] = 0;
		this->_inputs[i] = nullptr;
	}

	// Set default order to zero
	this->iOrder = 0;
	this->bUpdateOrder = false;
}

le_Element::~le_Element()
{
	delete[] this->_inputs;
	delete[] this->_outputSlots;
}

uint16_t le_Element::GetOrder()
{
	if (this->bUpdateOrder)
	{
		// Reset order to zero
		this->iOrder = 0;

		// Recalculate order
		this->FindOrder(this, &(this->iOrder));

		// Reset flag
		this->bUpdateOrder = false;
	}

	return this->iOrder;
}

void le_Element::Update(float timeStamp)
{
	// Do nothing...
}

void le_Element::Connect(le_Element* output, uint16_t outputSlot, le_Element* input, uint16_t inputSlot)
{
	// Connect the two via a "net"
	input->_inputs[inputSlot] = output;

	// Set Update Flags
	input->SetUpdateOrderFlag(input);
}

void le_Element::FindOrder(le_Element* original, uint16_t* order)
{
	// Increment order
	*order += 1;

	// Iterate through inputs
	for (uint16_t i = 0; i < this->nInputs; i++)
	{
		uint16_t orderTmp = *order;
		le_Element* e = this->_inputs[i];
		if (e != original && e != nullptr)
		{
			e->FindOrder(this, &orderTmp);
			if (orderTmp > *order)
				*order = orderTmp;
		}
	}
}

void le_Element::SetUpdateOrderFlag(le_Element* original)
{
	// Update order flag
	this->bUpdateOrder = true;

	// Recurse through inputs
	for (uint16_t i = 0; i < this->nInputs; i++)
	{
		le_Element* e = _inputs[i];

		if (e == original)
			continue;

		if (e == nullptr)
			continue;

		e->SetUpdateOrderFlag(this);
	}
}
