#pragma once

#include "le_Base.hpp"


template <typename T>
class le_Mux : protected le_Base<T>
{
protected:
    le_Mux(uint8_t signalWidth, uint16_t nInputs) : le_Base<T>(signalWidth * nInputs + 1, signalWidth)
    {
        this->uSignalWidth = signalWidth;
    }

    /**
     * @brief Destructor to clean up the multiplexer.
     */
    ~le_Mux() {}

    /**
     * @brief Updates the multiplexer with the current input value.
     * @param timeStep The current timestamp.
     */
    void Update(float timeStep)
    {
        le_Base<bool>* sel = (le_Base<bool>*)this->_inputs[this->uSignalWidth - 1];

        // Check null reference
        if(sel == nullptr)
            return;

        bool selector = sel->GetValue(this->_outputSlots[this->uSignalWidth - 1]);

        for(uint16_t i = 0; i < this->uSignalWidth; i++)
        {
            // Get index based on selector
            uint16_t index = selector ? i + this->uSignalWidth : i;

            // Get element
            le_Base<T>* e = (le_Base<T>*)this->_input[index];

            // Check null reference
            if (e != nullptr)
            {
                T inputValue = e->GetValue(this->_outputSlots[index]);

                // Set next value
                this->SetValue(i, inputValue);
            }
        }
    }

public:
    /**
     * @brief Connects an output slot of another element to the input of this node.
     * @param e The element to connect from.
     * @param outputSlot The output slot of the element to connect from.
     */
    void SetInput(le_Base<T>* e, uint16_t outputSlot, uint16_t inputSlot)
    {
        le_Element::Connect(e, outputSlot, this, inputSlot);
    }

    /**
     * @brief Connects an output slot of another element to the input of this node.
     * @param e The element to connect from.
     * @param outputSlot The output slot of the element to connect from.
     */
    void SetInput_Selector(le_Base<T>* e, uint16_t outputSlot)
    {
        le_Element::Connect(e, outputSlot, this, this->uSignalWidth - 1);
    }

private:
    uint8_t uSignalWidth;

    // Allow le_Engine to access private members
    friend class le_Engine;
};