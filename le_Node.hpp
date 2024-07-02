#pragma once

#include "le_Base.hpp"

/**
 * @brief Template class for le_Node, representing a node with history.
 *        Inherits from le_Base with a specified type.
 * @tparam T The type of the values stored in the node.
 */
template <typename T>
class le_Node : protected le_Base<T>
{
private:
    /**
     * @brief Constructor that initializes the node with a specified history length.
     * @param historyLength The length of the history buffer.
     */
    le_Node(uint16_t historyLength);

    /**
     * @brief Destructor to clean up the node.
     */
    ~le_Node();

    /**
     * @brief Updates the node with the current input value.
     * @param timeStep The current timestamp.
     */
    void Update(float timeStep);

    /**
     * @brief Gets the history buffer with an offset from the circular buffer.
     * @param buffer Pointer to store the history buffer.
     * @param startOffset Pointer to store the start offset in the circular buffer.
     */
    void GetHistory(T** buffer, uint16_t* startOffset);

    /**
     * @brief Connects an output slot of another element to the input of this node.
     * @param e The element to connect from.
     * @param outputSlot The output slot of the element to connect from.
     */
    void Connect(le_Base<T>* e, uint16_t outputSlot);

    using le_Base<T>::GetValue;
    using le_Base<T>::SetValue;

private:
    uint16_t uHistoryLength; ///< The length of the history buffer.
    T* _history;             ///< The history buffer.
    uint16_t uWrite;         ///< The current write position in the history buffer.

    // Allow le_Engine to access private members
    friend class le_Engine;
};

// Implementation of the le_Node class

/**
 * @brief Constructor implementation that initializes the node with a specified history length.
 * @tparam T The type of the values stored in the node.
 * @param historyLength The length of the history buffer.
 */
template<typename T>
le_Node<T>::le_Node(uint16_t historyLength) : le_Base<T>(1, 1)
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

/**
 * @brief Destructor implementation to clean up the node.
 * @tparam T The type of the values stored in the node.
 */
template<typename T>
le_Node<T>::~le_Node()
{
    delete[] this->_history;
}

/**
 * @brief Updates the node with the current input value.
 * @param timeStep The current timestamp.
 */
template<typename T>
void le_Node<T>::Update(float timeStep)
{
    // Get input value
    le_Base<T>* e = (le_Base<T>*)this->_inputs[0];
    if (e != nullptr)
    {
        T inputValue = e->GetValue(this->_outputSlots[0]);

        // Set next value
        this->SetValue(0, inputValue);
    }

    // Write to circular buffer and shift write head
    this->_history[this->uWrite] = this->GetValue(0);
    this->uWrite = (this->uWrite - 1 + this->uHistoryLength) % this->uHistoryLength;
}

/**
 * @brief Gets the history buffer with an offset from the circular buffer.
 * @param buffer Pointer to store the history buffer.
 * @param startOffset Pointer to store the start offset in the circular buffer.
 */
template<typename T>
void le_Node<T>::GetHistory(T** buffer, uint16_t* startOffset)
{
    // TODO: Implement this
}

/**
 * @brief Connects an output slot of another element to the input of this node.
 * @param e The element to connect from.
 * @param outputSlot The output slot of the element to connect from.
 */
template<typename T>
void le_Node<T>::Connect(le_Base<T>* e, uint16_t outputSlot)
{
    // Use default connection function
    le_Element::Connect(e, outputSlot, this, 0);
}
