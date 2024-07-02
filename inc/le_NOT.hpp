#pragma once

#include "le_Base.hpp"

/**
 * @brief Class representing a logical NOT element.
 *        Inherits from le_Base with boolean type.
 */
class le_NOT : protected le_Base<bool>
{
private:
    /**
     * @brief Constructor that initializes the NOT element.
     */
    le_NOT();

    /**
     * @brief Updates the NOT element. Calculates the logical NOT of the input.
     * @param timeStep The current timestamp.
     */
    void Update(float timeStep);

public:
    /**
     * @brief Connects an output slot of another element to the input of this NOT element.
     * @param e The element to connect from.
     * @param outputSlot The output slot of the element to connect from.
     */
    void Connect(le_Base<bool>* e, uint16_t outputSlot);

private:
    // Allow le_Engine to access private members
    friend class le_Engine;
};
