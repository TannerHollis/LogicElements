#pragma once

#include "le_Base.hpp"

// Include STD libs
#include <string>

// Include tinyexpr lib
extern "C"
{
#include "tinyexpr/tinyexpr.h"
}

class le_Math : protected le_Base<float>
{
private:
	le_Math(uint16_t nInputs_Digital, std::string expr) : le_Base<float>(nInputs_Digital, 1)
	{
		// Declare extrinsic variables
		this->sExpr = expr;

		// Declare intrinsic variables
		this->te_vars = (te_variable*)malloc( sizeof(te_variable) * nInputs_Digital );
		this->_vars = (double*)malloc( sizeof(double) * nInputs_Digital );
		for (uint16_t i = 0; i < nInputs_Digital; i++)
		{
			snprintf(this->te_vars[i].name, 63, "x%d", i);
			this->te_vars[i].address = &this->_vars[i];
		}

		// Compile expression
		this->te_expr = te_compile(expr.c_str(), this->te_vars, nInputs_Digital, &this->te_err);
	}

	~le_Math()
	{
		// Free allocated memory
		free(this->te_vars);
		free(this->_vars);
		te_free(this->te_expr);
	}

	void Update(float timeStep)
	{
		// Set default to true
		bool nextValue = true;

		// Iterate through all input values and apply inversion if necessary
		for (uint16_t i = 0; i < this->nInputs_Digital; i++)
		{
			le_Base<float>* e = this->_inputs[i];
			if (e != nullptr)
			{
				this->_vars[i] = (double)e->GetValue(this->_outputSlots[i]);
			}
		}

		// Attempt expression
		if (this->te_expr)
		{
			double exprEvaluation = te_eval(this->expr);

			// Set next value
			this->SetValue(0, (float)exprEvaluation);
		}
	}

public:
	void Connect(le_Base<float>* e, uint16_t outputSlot, uint16_t inputSlot)
	{
		// Use default connection function
		le_Base<float>::Connect(e, outputSlot, this, inputSlot);
	}

private:
	// Expression string
	std::string sExpr;

	// Expression variables
	double* _vars;
	te_variable* te_vars;
	te_expr* te_expr;
	int te_err;

	friend class le_Engine;
};