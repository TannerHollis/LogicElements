#pragma once

#include "le_Element.hpp"

/**
 * @brief Template class for le_Base, derived from le_Element.
 *        Represents a base element with inputs and outputs.
 * @tparam T The type of the output values.
 */
template <typename T>
class le_Node;

template <typename T>
class le_Mux;

template <typename T>
class le_Base : public le_Element
{
public:
    /**
     * @brief Gets the value of the specified output slot.
     * @param outputSlot The index of the output slot (0-based).
     * @return The value of the specified output slot.
     */
    T GetValue(uint8_t outputSlot) const;

    /**
     * @brief Gets the output slot index associated with a given input slot.
     * @param inputSlot The index of the input slot (0-based).
     * @return The index of the corresponding output slot.
     */
    uint8_t GetOutputSlot(uint8_t inputSlot) const;

LE_ELEMENT_ACCESS_MOD:
    /**
     * @brief Constructor that initializes the base element with specified inputs and outputs.
     * @param nInputs The number of inputs for the element.
     * @param nOutputs The number of outputs for the element.
     */
    le_Base(uint8_t nInputs, uint8_t nOutputs);

    /**
     * @brief Destructor to clean up allocated memory.
     */
    ~le_Base();

    /**
     * @brief Virtual function to update the element. Can be overridden by derived classes.
     * @param timeStep The elapsed time since the last update.
     */
    virtual void Update(float timeStep);

    /**
     * @brief Gets a pointer to the input at the specified input slot.
     * @tparam Tout The expected type of the input value.
     * @param inputSlot The index of the input slot (0-based).
     * @return A pointer to the input value, or nullptr if the type doesn't match.
     */
    template<typename Tout>
    Tout* GetInput(uint8_t inputSlot) const;

    /**
     * @brief Sets the value of the specified output slot.
     * @param outputSlot The index of the output slot (0-based).
     * @param value The new value for the output slot.
     */
    void SetValue(uint8_t outputSlot, T value);

private:
    uint8_t nOutputs; ///< Number of outputs.

    T* _outputs; ///< Array of output values.

    // Friend classes for access to protected members (Make friends, shake hands)
    friend class le_Node<T>;
    friend class le_Mux<float>; // Special case since le_Mux<float> always requires a boolean (le_Base<bool>) input
    friend class le_Mux<T>;

    // Make le_Engine a friend for factory
    friend class le_Engine;
};

// Implementation of the le_Base class

/**
 * @brief Constructor implementation that initializes the base element with specified inputs and outputs.
 * @tparam T The type of the output values.
 * @param nInputs The number of inputs for the element.
 * @param nOutputs The number of outputs for the element.
 */
template<typename T>
le_Base<T>::le_Base(uint8_t nInputs, uint8_t nOutputs) : le_Element(nInputs)
{
    // Set the number of outputs
    this->nOutputs = nOutputs;

    // Allocate memory for outputs
    this->_outputs = new T[nOutputs];

    // Initialize output values
    for (uint8_t i = 0; i < nOutputs; i++)
    {
        this->_outputs[i] = (T)0;
    }
}

/**
 * @brief Destructor implementation to clean up allocated memory.
 * @tparam T The type of the output values.
 */
template<typename T>
le_Base<T>::~le_Base()
{
    // Deallocate memory for outputs
    delete[] this->_outputs;
}

/**
 * @brief Gets the value of the specified output slot.
 * @tparam T The type of the output values.
 * @param outputSlot The index of the output slot (0-based).
 * @return The value of the specified output slot.
 */
template<typename T>
T le_Base<T>::GetValue(uint8_t outputSlot) const
{
    return this->_outputs[outputSlot];
}

template<typename T>
uint8_t le_Base<T>::GetOutputSlot(uint8_t inputSlot) const
{
    return this->_outputSlots[inputSlot];
}

/**
 * @brief Sets the value of the specified output slot.
 * @tparam T The type of the output values.
 * @param outputSlot The index of the output slot (0-based).
 * @param value The new value for the output slot.
 */
template<typename T>
void le_Base<T>::SetValue(uint8_t outputSlot, T value)
{
    this->_outputs[outputSlot] = value;
}

/**
 * @brief Virtual function to update the element. Can be overridden by derived classes.
 * @tparam T The type of the output values.
 * @param timeStep The elapsed time since the last update.
 */
template<typename T>
void le_Base<T>::Update(float timeStep)
{
    UNUSED(timeStep);
    // Default implementation does nothing
}

template<typename T>
template<typename Tout>
Tout* le_Base<T>::GetInput(uint8_t inputSlot) const
{
    return (Tout*)this->_inputs[inputSlot];
}
