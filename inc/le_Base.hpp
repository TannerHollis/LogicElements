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
protected:
    /**
     * @brief Constructor that initializes the base element with specified inputs and outputs.
     * @param nInputs Number of inputs for the element.
     * @param nOutputs Number of outputs for the element.
     */
    le_Base(uint16_t nInputs, uint16_t nOutputs);

    /**
     * @brief Destructor to clean up allocated memory.
     */
    ~le_Base();

    /**
     * @brief Virtual function to update the element. Can be overridden by derived classes.
     * @param timeStep The current timestamp.
     */
    virtual void Update(float timeStep);

    /**
     * @brief Gets the value of the specified output slot.
     * @param outputSlot The slot of the output.
     * @return The value of the specified output slot.
     */
    const T GetValue(uint16_t outputSlot);

    /**
     * @brief Sets the value of the specified output slot.
     * @param outputSlot The slot of the output.
     * @param value The value to set.
     */
    void SetValue(uint16_t outputSlot, T value);

private:
    uint16_t nOutputs;  ///< Number of outputs.

    T* _outputs;  ///< Array of output values.

    // Friend classes for access to protected members (Make friends, shake hands)
    friend class le_Node<T>;
    friend class le_Mux<float>; // Special case since le_Mux<float> always requires a boolean (le_Base<bool>) input
    friend class le_Mux<T>;
    friend class le_AND;
    friend class le_OR;
    friend class le_NOT;
    friend class le_RTrig;
    friend class le_FTrig;
    friend class le_Timer;
    friend class le_Counter;
    friend class le_Analog1PWinding;
    friend class le_Analog3PWinding;
    friend class le_Overcurrent;
    friend class le_Math;
    friend class le_PID;

    // Make le_Engine a friend for factory
    friend class le_Engine;
};

// Implementation of the le_Base class

/**
 * @brief Constructor implementation that initializes the base element with specified inputs and outputs.
 * @tparam T The type of the output values.
 * @param nInputs Number of inputs for the element.
 * @param nOutputs Number of outputs for the element.
 */
template<typename T>
le_Base<T>::le_Base(uint16_t nInputs, uint16_t nOutputs) : le_Element(nInputs)
{
    // Set the number of outputs
    this->nOutputs = nOutputs;

    // Allocate memory for outputs
    this->_outputs = new T[nOutputs];

    // Initialize output values
    for (uint16_t i = 0; i < nOutputs; i++)
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
 * @param outputSlot The slot of the output.
 * @return The value of the specified output slot.
 */
template<typename T>
const T le_Base<T>::GetValue(uint16_t outputSlot)
{
    return this->_outputs[outputSlot];
}

/**
 * @brief Sets the value of the specified output slot.
 * @tparam T The type of the output values.
 * @param outputSlot The slot of the output.
 * @param value The value to set.
 */
template<typename T>
void le_Base<T>::SetValue(uint16_t outputSlot, T value)
{
    this->_outputs[outputSlot] = value;
}

/**
 * @brief Virtual function to update the element. Can be overridden by derived classes.
 * @tparam T The type of the output values.
 * @param timeStep The current timestamp.
 */
template<typename T>
void le_Base<T>::Update(float timeStep)
{
    // Default implementation does nothing
}
