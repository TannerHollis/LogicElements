#include "le_Math.hpp"

/**
 * @brief Constructor that initializes the le_Math with specified number of inputs and expression.
 * @param nInputs Number of inputs for the mathematical expression.
 * @param expr The mathematical expression to evaluate.
 */
le_Math::le_Math(uint16_t nInputs, std::string expr) : le_Base<float>(nInputs, 1)
{
    // Declare extrinsic variables
    this->sExpr = expr;

    // Declare intrinsic variables
    this->te_vars = new te_variable[nInputs];
    this->_vars = new double[nInputs];
    for (uint16_t i = 0; i < nInputs; i++)
    {
        snprintf((char*)this->te_vars[i].name, 5, "x%d", i);
        this->te_vars[i].address = &this->_vars[i];
    }

    // Compile expression
    this->te_expression = te_compile(expr.c_str(), this->te_vars, nInputs, &this->te_err);
}

/**
 * @brief Destructor to clean up the le_Math.
 */
inline le_Math::~le_Math()
{
    // Free allocated memory
    delete[] this->te_vars;
    delete[] this->_vars;
    te_free(this->te_expression);
}

/**
 * @brief Updates the le_Math by evaluating the expression with the current input values.
 * @param timeStep The current timestamp.
 */
void le_Math::Update(float timeStep)
{
    // Iterate through all input values and apply inversion if necessary
    for (uint16_t i = 0; i < this->nInputs; i++)
    {
        le_Base<float>* e = (le_Base<float>*)this->_inputs[i];
        if (e != nullptr)
        {
            this->_vars[i] = (double)e->GetValue(this->_outputSlots[i]);
        }
    }

    // Attempt expression
    if (this->te_expression)
    {
        double exprEvaluation = te_eval(this->te_expression);

        // Set next value
        this->SetValue(0, (float)exprEvaluation);
    }
}

/**
 * @brief Sets the input for the mathematical expression.
 * @param e The element providing the input value.
 * @param outputSlot The output slot of the element providing the input.
 * @param inputSlot The input slot of this le_Math element to connect to.
 */
void le_Math::SetInput(le_Base<float>* e, uint16_t outputSlot, uint16_t inputSlot)
{
    // Use default connection function
    le_Base<float>::Connect(e, outputSlot, this, inputSlot);
}
