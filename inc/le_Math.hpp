#pragma once

#include "le_Base.hpp"

// Include STD libs
#include <string>

// Include tinyexpr lib
extern "C"
{
#include "tinyexpr/tinyexpr.h"
}

/**
 * @brief Class representing a mathematical expression evaluator.
 *        Inherits from le_Base with float type.
 */
class le_Math : protected le_Base<float>
{
LE_ELEMENT_ACCESS_MOD:
    /**
     * @brief Constructor that initializes the le_Math with specified number of inputs and expression.
     * @param nInputs Number of inputs for the mathematical expression.
     * @param expr The mathematical expression to evaluate.
     */
    le_Math(uint16_t nInputs, std::string expr);

    /**
     * @brief Destructor to clean up the le_Math.
     */
    ~le_Math();

    /**
     * @brief Updates the le_Math by evaluating the expression with the current input values.
     * @param timeStep The current timestamp.
     */
    void Update(float timeStep);

public:
    /**
     * @brief Sets the input for the mathematical expression.
     * @param e The element providing the input value.
     * @param outputSlot The output slot of the element providing the input.
     * @param inputSlot The input slot of this le_Math element to connect to.
     */
    void SetInput(le_Base<float>* e, uint16_t outputSlot, uint16_t inputSlot);

private:
    std::string sExpr;    ///< The mathematical expression as a string.
    double* _vars;        ///< Array of variables used in the expression.
    te_variable* te_vars; ///< Array of tinyexpr variables.
    te_expr* te_expression;     ///< Compiled tinyexpr expression.
    int te_err;           ///< Error code for tinyexpr expression compilation.

    // Allow le_Engine to access private members
    friend class le_Engine;
};
