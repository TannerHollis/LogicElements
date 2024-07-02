#pragma once

// Include standard C++ libraries
#include <vector>
#include <cstdint>
#include <cstdlib>

/**
 * @brief Base class for le_Element which represents an element with inputs and an update mechanism.
 */
class le_Element
{
public:
    /**
     * @brief Constructor that initializes the element with a specified number of inputs.
     * @param nInputs Number of inputs for the element.
     */
    le_Element(uint16_t nInputs);

    /**
     * @brief Virtual destructor to allow proper cleanup of derived classes.
     */
    virtual ~le_Element();

    /**
     * @brief Gets the update order of the element.
     * @return The update order.
     */
    uint16_t GetOrder();

    /**
     * @brief Comparison operator for ordering elements by their update order.
     * @param left The left-hand element to compare with.
     * @param right The right-hand element to compare with.
     * @return True if this element has a smaller update order than the other element.
     */
    static bool CompareElementOrders(le_Element* left, le_Element* right);

    /**
     * @brief Virtual function to update the element. Can be overridden by derived classes.
     * @param timeStamp The current timestamp.
     */
    virtual void Update(float timeStamp);

    /**
     * @brief Static function to connect the output of one element to the input of another element.
     * @param output The output element.
     * @param outputSlot The slot of the output element.
     * @param input The input element.
     * @param inputSlot The slot of the input element.
     */
    static void Connect(le_Element* output, uint16_t outputSlot, le_Element* input, uint16_t inputSlot);

private:
    /**
     * @brief Finds the order of the element recursively.
     * @param original The original element to start from.
     * @param order Pointer to store the order.
     */
    void FindOrder(le_Element* original, uint16_t* order);

    /**
     * @brief Sets the flag for updating the order of the element.
     * @param original The original element to start from.
     */
    void SetUpdateOrderFlag(le_Element* original);

private:
    bool bUpdateOrder;            ///< Flag indicating if the update order needs to be recalculated.
    uint16_t iOrder;              ///< The update order of the element.

protected:
    uint16_t nInputs;             ///< Number of inputs.
    le_Element** _inputs;         ///< Array of pointers to input elements.
    uint16_t* _outputSlots;       ///< Array of output slots.
};