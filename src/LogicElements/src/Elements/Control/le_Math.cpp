#include "Elements/Control/le_Math.hpp"

#ifdef LE_ELEMENTS_ANALOG
#ifdef LE_ELEMENTS_MATH

namespace LogicElements {

/**
 * @brief Constructor that initializes the ElementType::Math with specified number of inputs and expression.
 * @param nInputs Number of inputs for the mathematical expression.
 * @param expr The mathematical expression to evaluate.
 */
Math::Math(uint8_t nInputs, std::string expr) : Element(ElementType::Math), nInputCount(nInputs)
{
    // Create input ports (x0, x1, x2, ...)
    for (uint8_t i = 0; i < nInputs; i++)
    {
        AddInputPort<float>(LE_PORT_MATH_VAR_NAME(i));
    }

    // Create output port
    pOutput = AddOutputPort<float>(LE_PORT_OUTPUT_PREFIX);

    // Declare extrinsic variables
    this->sExpr = expr;

    // Declare intrinsic variables
    this->te_vars = new te_variable[nInputs];
    this->_vars = new double[nInputs];
    for (uint8_t i = 0; i < nInputs; i++)
    {
        // Initialize all to default (0)
        this->te_vars[i].context = 0;
        this->te_vars[i].type = 0;

        // Assign name & address using port naming macro
        std::string varName = LE_PORT_MATH_VAR_NAME(i);
        this->te_vars[i].name = new char[varName.length() + 1];
        strcpy((char*)this->te_vars[i].name, varName.c_str());
        this->te_vars[i].address = &this->_vars[i];
    }

    // Compile expression
    this->te_expression = te_compile(expr.c_str(), this->te_vars, nInputs, &this->te_err);

    if (!this->te_expression)
    {
        printf("\t%s\r\n", expr.c_str());
        printf("\t%*s^\nError near here\r\n", this->te_err - 1, "");
    }
}

/**
 * @brief Destructor to clean up the ElementType::Math.
 */
Math::~Math()
{
    // Free allocated memory
    for (uint16_t i = 0; i < this->nInputCount; i++)
    {
        delete[] this->te_vars[i].name;
    }
    delete[] this->te_vars;
    delete[] this->_vars;
    te_free(this->te_expression);
}

/**
 * @brief Updates the ElementType::Math by evaluating the expression with the current input values.
 * @param timeStamp The current timestamp.
 */
void Math::Update(const Time& timeStamp)
{
    UNUSED(timeStamp);

    // Retrieve input values from connected ports
    for (size_t i = 0; i < _inputPorts.size(); i++)
    {
        InputPort<float>* inputPort = dynamic_cast<InputPort<float>*>(_inputPorts[i]);
        if (inputPort && inputPort->IsConnected())
        {
            this->_vars[i] = (double)inputPort->GetValue();
        }
        else
        {
            this->_vars[i] = 0.0;
        }
    }

    // Evaluate expression
    if (this->te_expression)
    {
        double result = te_eval(this->te_expression);
        pOutput->SetValue((float)result);
    }
}

/**
 * @brief Sets the input for the mathematical expression.
 * @param sourceElement The element providing the input value.
 * @param sourcePortName The name of the output port on the source element.
 * @param inputPortName The name of the input port on this ElementType::Math element.
 */
void Math::SetInput(Element* sourceElement, const std::string& sourcePortName, const std::string& inputPortName)
{
    Element::Connect(sourceElement, sourcePortName, this, inputPortName);
}

/**
 * @brief Gets the result output value.
 * @return The evaluated result.
 */
float Math::GetOutput() const
{
    return pOutput->GetValue();
}

#endif // LE_ELEMENTS_MATH
#endif // LE_ELEMENTS_ANALOG

} // namespace LogicElements
