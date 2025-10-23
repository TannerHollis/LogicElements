#pragma once

#include "le_Element.hpp"

#ifdef LE_ELEMENTS_ANALOG
#ifdef LE_ELEMENTS_MATH

// Include STD libs
#include <string>

// Include tinyexpr lib
extern "C"
{
#include "tinyexpr.h"
}


namespace LogicElements {
/**
 * @brief Class representing a mathematical expression evaluator.
 *        Evaluates expressions like "x0 + x1 * 2" with named input ports.
 */
class Math : public Element
{
LE_ELEMENT_ACCESS_MOD:
    /**
     * @brief Constructor that initializes the ElementType::Math with specified number of inputs and expression.
     * @param nInputs Number of inputs for the mathematical expression.
     * @param expr The mathematical expression to evaluate (uses x0, x1, x2... for inputs).
     */
    Math(uint8_t nInputs, std::string expr);

    /**
     * @brief Destructor to clean up the ElementType::Math.
     */
    ~Math();

    /**
     * @brief Updates the ElementType::Math by evaluating the expression with the current input values.
     * @param timeStamp The current timestamp.
     */
    void Update(const Time& timeStamp) override;

public:
    /**
     * @brief Sets the input for the mathematical expression.
     * @param sourceElement The element providing the input value.
     * @param sourcePortName The name of the output port on the source element.
     * @param inputPortName The name of the input port on this ElementType::Math element.
     */
    void SetInput(Element* sourceElement, const std::string& sourcePortName, const std::string& inputPortName);

    /**
     * @brief Gets the result output value.
     * @return The evaluated result.
     */
    float GetOutput() const;

private:
    OutputPort<float>* pOutput;  ///< Output port.
    uint8_t nInputCount;            ///< Number of inputs.
    
    std::string sExpr;              ///< The mathematical expression as a string.
    double* _vars;                  ///< Array of variables used in the expression.
    te_variable* te_vars;           ///< Array of tinyexpr variables.
    te_expr* te_expression;         ///< Compiled tinyexpr expression.
    int te_err;                     ///< Error code for tinyexpr expression compilation.

    // Allow le_Engine to access private members
    friend class Engine;
};

#endif // LE_ELEMENTS_MATH
#endif // LE_ELEMENTS_ANALOG

} // namespace LogicElements
