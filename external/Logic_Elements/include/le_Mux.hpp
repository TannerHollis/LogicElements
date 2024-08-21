#pragma once

#include "le_Base.hpp"

/**
 * @brief Class representing a multiplexer in an analog system.
 *        Inherits from le_Base with a specified type.
 *
 * @tparam T The data type of the multiplexer (e.g., float, int).
 */
template <typename T>
class le_Mux : protected le_Base<T>
{
LE_ELEMENT_ACCESS_MOD:
    /**
     * @brief Constructor that initializes the le_Mux with specified signal width and number of inputs.
     * @param signalWidth The width of the signal.
     * @param nInputs The number of inputs to the multiplexer.
     */
    le_Mux(uint8_t signalWidth, uint8_t nInputs);

    /**
     * @brief Destructor to clean up the multiplexer.
     */
    ~le_Mux();

    /**
     * @brief Updates the multiplexer with the current input value.
     * @param timeStep The current time step.
     */
    void Update(float timeStep);

public:
    /**
     * @brief Connects an output slot of another element to the input of this multiplexer.
     * @param e The element to connect from.
     * @param outputSlot The output slot of the element to connect from.
     * @param inputSlot The input slot of this multiplexer to connect to.
     */
    void SetInput(le_Base<T>* e, uint8_t outputSlot, uint8_t inputSlot);

    /**
     * @brief Connects an output slot of another element to the select input of this multiplexer.
     * @param e The element to connect from.
     * @param outputSlot The output slot of the element to connect from.
     */
    void SetInput_Select(le_Base<bool>* e, uint8_t outputSlot);

private:
    uint8_t uSignalWidth; ///< The width of the signal.
    uint8_t nInputs;      ///< Number of input signals.

    // Allow le_Engine to access private members
    friend class le_Engine;
};

template<typename T>
le_Mux<T>::le_Mux(uint8_t signalWidth, uint8_t nInputs) : le_Base<T>(signalWidth * nInputs + 1, signalWidth)
{
    // Set extrinsic variables
    this->uSignalWidth = signalWidth;
    this->nInputs = nInputs;
}

/**
* @brief Destructor to clean up the multiplexer.
*/
template<typename T>
le_Mux<T>::~le_Mux() {}

/**
* @brief Updates the multiplexer with the current input value.
* @param timeStep The current time step.
*/
template<typename T>
void le_Mux<T>::Update(float timeStep)
{
    UNUSED(timeStep);

    uint8_t selectorIndex = this->uSignalWidth - 1;
    le_Base<bool>* sel = this->template GetInput<le_Base<bool>>(selectorIndex);

    // Check null reference
    if (sel == nullptr)
        return;

    bool selector = sel->GetValue(this->GetOutputSlot(selectorIndex));

    for (uint8_t i = 0; i < this->uSignalWidth; i++)
    {
        // Get index based on selector
        uint8_t index = selector ? i + this->uSignalWidth : i;

        // Get element
        le_Base<T>* e = this->template GetInput<le_Base<T>>(index);

        // Check null reference
        if (e != nullptr)
        {
            T inputValue = e->GetValue(this->GetOutputSlot(index));

            // Set next value
            this->SetValue(i, inputValue);
        }
    }
}

/**
* @brief Connects an output slot of another element to the input of this multiplexer.
* @param e The element to connect from.
* @param outputSlot The output slot of the element to connect from.
* @param inputSlot The input slot of this multiplexer to connect to.
*/
template<typename T>
void le_Mux<T>::SetInput(le_Base<T>* e, uint8_t outputSlot, uint8_t inputSlot)
{
    le_Element::Connect(e, outputSlot, this, inputSlot);
}

/**
* @brief Connects an output slot of another element to the select input of this multiplexer.
* @param e The element to connect from.
* @param outputSlot The output slot of the element to connect from.
*/
template<typename T>
void le_Mux<T>::SetInput_Select(le_Base<bool>* e, uint8_t outputSlot)
{
    le_Element::Connect(e, outputSlot, this, this->uSignalWidth * this->nInputs - 1);
}
