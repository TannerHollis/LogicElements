#include "le_Element.hpp"

/**
 * @brief Constructor that initializes the element with a specified number of inputs.
 * @param nInputs Number of inputs for the element.
 */
le_Element::le_Element(uint16_t nInputs)
{
    // Set the number of inputs
    this->nInputs = nInputs;

    // Allocate memory for inputs and output slots
    this->_inputs = new le_Element * [nInputs];
    this->_outputSlots = new uint16_t[nInputs];

    // Initialize input references and output slots
    for (uint16_t i = 0; i < nInputs; i++)
    {
        this->_outputSlots[i] = 0;
        this->_inputs[i] = nullptr;
    }

    // Set default order and update flag
    this->iOrder = 0;
    this->bUpdateOrder = false;
}

/**
 * @brief Destructor to clean up allocated memory.
 */
le_Element::~le_Element()
{
    // Deallocate memory for inputs and output slots
    delete[] this->_inputs;
    delete[] this->_outputSlots;
}

/**
 * @brief Gets the update order of the element.
 * @return The update order.
 */
uint16_t le_Element::GetOrder()
{
    if (this->bUpdateOrder)
    {
        // Reset order to zero and recalculate it
        this->iOrder = 0;
        this->FindOrder(this, &(this->iOrder));
        this->bUpdateOrder = false; // Reset flag
    }

    return this->iOrder;
}

/**
* @brief Comparison operator for ordering elements by their update order.
* @param left The left-hand element to compare with.
* @param right The right-hand element to compare with.
* @return True if this element has a smaller update order than the other element.
*/
bool le_Element::CompareElementOrders(le_Element* left, le_Element* right)
{
    return left->GetOrder() < right->GetOrder();
}

/**
 * @brief Virtual function to update the element. Can be overridden by derived classes.
 * @param timeStamp The current timestamp.
 */
void le_Element::Update(float timeStamp)
{
    // Default implementation does nothing
}

/**
 * @brief Static function to connect the output of one element to the input of another element.
 * @param output The output element.
 * @param outputSlot The slot of the output element.
 * @param input The input element.
 * @param inputSlot The slot of the input element.
 */
void le_Element::Connect(le_Element* output, uint16_t outputSlot, le_Element* input, uint16_t inputSlot)
{
    // Connect the output element to the input element
    input->_inputs[inputSlot] = output;

    // Set the update order flag
    input->SetUpdateOrderFlag(input);
}

/**
 * @brief Finds the order of the element recursively.
 * @param original The original element to start from.
 * @param order Pointer to store the order.
 */
void le_Element::FindOrder(le_Element* original, uint16_t* order)
{
    // Increment the order
    (*order)++;

    // Iterate through inputs to calculate the order
    for (uint16_t i = 0; i < this->nInputs; i++)
    {
        uint16_t orderTmp = *order;
        le_Element* e = this->_inputs[i];

        // Avoid circular dependency and null pointers
        if (e != original && e != nullptr)
        {
            e->FindOrder(this, &orderTmp);
            if (orderTmp > *order)
                *order = orderTmp;
        }
    }
}

/**
 * @brief Sets the flag for updating the order of the element.
 * @param original The original element to start from.
 */
void le_Element::SetUpdateOrderFlag(le_Element* original)
{
    // Set the update order flag
    this->bUpdateOrder = true;

    // Recurse through inputs to propagate the flag
    for (uint16_t i = 0; i < this->nInputs; i++)
    {
        le_Element* e = _inputs[i];

        // Avoid circular dependency and null pointers
        if (e != original && e != nullptr)
        {
            e->SetUpdateOrderFlag(this);
        }
    }
}