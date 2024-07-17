
# Logic Elements

A versatile framework for creating and managing logical elements and their interactions. This project includes various logical components like AND, OR, NOT gates, as well as more complex elements such as timers, counters, and overcurrent protection elements. It is designed to be easily extensible and provides a solid foundation for building complex logical circuits and simulations.

![LogicElement_DoubleClickExample](./example_double_click.png)

## Table of Contents

- [Introduction](#introduction)
- [Features](#features)
- [Installation](#installation)
- [Usage](#usage)
- [Classes](#classes)
  - [le_Element](#le_element)
  - [le_Base](#le_base)
  - [le_AND](#le_and)
  - [le_OR](#le_or)
  - [le_NOT](#le_not)
  - [le_RTrig](#le_rtrig)
  - [le_FTrig](#le_ftrig)
  - [le_Counter](#le_counter)
  - [le_Node](#le_node)
  - [le_Overcurrent](#le_overcurrent)
  - [le_Timer](#le_timer)
- [Contributing](#contributing)
- [License](#license)

## Introduction

This project provides a framework for creating and managing logical elements and their interactions. It includes various logical components like AND, OR, NOT gates, as well as more complex elements like timers, counters, and overcurrent protection elements.

## Features

- Logical AND, OR, NOT gates
- Rising and Falling edge triggers
- Counter with settable final count
- Node with historical data
- Overcurrent protection element with customizable curves
- Timer with pickup and dropout times

## Installation

To install and use this project, clone the repository and include the necessary files in your project.

```bash
git clone https://github.com/TannerHollis/LogicElements.git
```

Include the headers and source files in your build system.

## Usage

Unfortunately, to make our lives easier, we first must make it harder. This library uses a "factory" class to manage and condense the loop code under the `le_Engine` class. This class will manage all memory and manage the `Update()` loop. It will also manage and sort the connections between elements based on their "order", which can be thought of as a dependency ranking. Those with a larger firing order will be updated last and those with a lower firing order will be updated first.

```cpp
#include "le_Engine.hpp"

int main() {
    le_Engine engine = le_Engine("TEST");
	le_Node<float>* le_vAIn;
	le_Node<float>* le_vAOutR;
	le_Node<float>* le_vAOutI;

	// Define inputs
	le_Engine::le_Element_TypeDef vAIn("VA_IN", le_Element_Type::LE_NODE_ANALOG);
	vAIn.args[0].uArg = 100;

	le_Engine::le_Element_TypeDef vBIn("VB_IN", le_Element_Type::LE_NODE_ANALOG);
	vBIn.args[0].uArg = 100;

	le_Engine::le_Element_TypeDef vCIn("VC_IN", le_Element_Type::LE_NODE_ANALOG);
	vCIn.args[0].uArg = 100;

	// Define elements
	le_Engine::le_Element_TypeDef v("V", le_Element_Type::LE_ANALOG_3P);
	v.args[0].uArg = 64;

	// Define outputs
	le_Engine::le_Element_TypeDef vOutReal("VAR", le_Element_Type::LE_NODE_ANALOG);
	vOutReal.args[0].uArg = 100;

	le_Engine::le_Element_TypeDef vOutImag("VAI", le_Element_Type::LE_NODE_ANALOG);;
	vOutImag.args[0].uArg = 100;

	// Add elements
	le_vAIn = (le_Node<float>*)engine.AddElement(&vAIn);
	le_vBIn = (le_Node<float>*)engine.AddElement(&vBIn);
	le_vCIn = (le_Node<float>*)engine.AddElement(&vCIn);
	engine.AddElement(&v);
	le_vAOutR = (le_Node<float>*)engine.AddElement(&vOutReal);
	le_vAOutI = (le_Node<float>*)engine.AddElement(&vOutImag);

	// Define net
	le_Engine::le_Element_Net_TypeDef rawA("VA_IN", 0);
	rawA.AddInput("V", 0);

	le_Engine::le_Element_Net_TypeDef rawB("VB_IN", 0);
	rawB.AddInput("V", 1);

	le_Engine::le_Element_Net_TypeDef rawC("VA_IN", 0);
	rawC.AddInput("V", 2);

	le_Engine::le_Element_Net_TypeDef vaReal("V", 0);
	vaReal.AddInput("V", 3);
	vaReal.AddInput("VAR", 0);

	le_Engine::le_Element_Net_TypeDef vaImag("V", 1);
	vaImag.AddInput("V", 4);
	vaImag.AddInput("VAI", 0);

	// Add nets
	engine.AddNet(&rawA);
	engine.AddNet(&rawB);
	engine.AddNet(&rawC);
	engine.AddNet(&vaReal);
	engine.AddNet(&vaImag);

	// Print engine details
	engine.Print();

	// iterate through
	double vARaw = 0.0f;
	float tSample = 1.0f / 3840.0f;
	for (float t = 0.0f; t < 10.0f; t += tSample)
	{
		vARaw = 10 * std::sin(2.0 * M_PI * 60.0 * (double)t);
		le_vAIn->SetValue(0, (float)vARaw);
		engine.Update(tSample);

		float r = le_vAOutR->GetValue(0);
		float i = le_vAOutI->GetValue(0);
		float mag = (float)std::sqrtf(r * r + i * i);
		printf("%+4.2f, %+4.2f (%+4.2f, %+4.2fj)\r\n", vARaw, mag, r, i);
	}

    return 0;
}
```

## Classes

### le_Element

Base class for elements with inputs and an update mechanism. It provides a common interface and shared functionality for all derived elements.

- **Constructor**: `le_Element(uint16_t nInputs)`
  - Initializes the element with a specified number of inputs.
- **Destructor**: `virtual ~le_Element()`
  - Virtual destructor to allow proper cleanup of derived classes.
- **GetOrder**: `uint16_t GetOrder() const`
  - Returns the update order of the element.
- **Update**: `virtual void Update(float timeStamp)`
  - Virtual function to update the element. Can be overridden by derived classes.
- **Connect**: `static void Connect(std::shared_ptr<le_Element> output, uint16_t outputSlot, std::shared_ptr<le_Element> input, uint16_t inputSlot)`
  - Static function to connect the output of one element to the input of another element.

### le_Base

Template base class for elements with inputs and outputs. It extends `le_Element` and provides functionality for handling inputs and outputs of various types.

- **Constructor**: `le_Base(uint16_t nInputs, uint16_t nOutputs)`
  - Initializes the base element with a specified number of inputs and outputs.
- **Destructor**: `~le_Base()`
  - Cleans up the dynamically allocated arrays for outputs.
- **GetValue**: `const T GetValue(uint16_t outputSlot)`
  - Returns the value of the specified output slot.
- **SetValue**: `void SetValue(uint16_t outputSlot, T value)`
  - Sets the value of the specified output slot.
- **Connect**: `static void Connect(le_Base* output, uint16_t outputSlot, le_Base* input, uint16_t inputSlot)`
  - Connects the output slot of one base element to the input slot of another base element.

### le_AND

Logical AND gate with a specified number of inputs. It calculates the logical AND of all inputs.

- **Constructor**: `le_AND(uint16_t nInputs)`
  - Initializes the AND gate with a specified number of inputs.
- **Update**: `void Update(float timeStep)`
  - Updates the AND gate by calculating the logical AND of all input values.
- **Connect**: `void Connect(le_Base<bool>* e, uint16_t outputSlot, uint16_t inputSlot)`
  - Connects an output slot of another element to an input slot of this AND gate.

### le_OR

Logical OR gate with a specified number of inputs. It calculates the logical OR of all inputs.

- **Constructor**: `le_OR(uint16_t nInputs)`
  - Initializes the OR gate with a specified number of inputs.
- **Update**: `void Update(float timeStep)`
  - Updates the OR gate by calculating the logical OR of all input values.
- **Connect**: `void Connect(le_Base<bool>* e, uint16_t outputSlot, uint16_t inputSlot)`
  - Connects an output slot of another element to an input slot of this OR gate.

### le_NOT

Logical NOT gate with a single input. It calculates the logical NOT of the input value.

- **Constructor**: `le_NOT()`
  - Initializes the NOT gate.
- **Update**: `void Update(float timeStep)`
  - Updates the NOT gate by calculating the logical NOT of the input value.
- **Connect**: `void Connect(le_Base<bool>* e, uint16_t outputSlot)`
  - Connects an output slot of another element to the input of this NOT gate.

### le_RTrig

Rising edge trigger element. It detects rising edge transitions on its input.

- **Constructor**: `le_RTrig()`
  - Initializes the rising edge trigger element.
- **Update**: `void Update(float timeStep)`
  - Updates the rising edge trigger element by detecting rising edge transitions.
- **Connect**: `void Connect(le_Base<bool>* e, uint16_t outputSlot)`
  - Connects an output slot of another element to the input of this rising edge trigger element.

### le_FTrig

Falling edge trigger element. It detects falling edge transitions on its input.

- **Constructor**: `le_FTrig()`
  - Initializes the falling edge trigger element.
- **Update**: `void Update(float timeStep)`
  - Updates the falling edge trigger element by detecting falling edge transitions.
- **Connect**: `void Connect(le_Base<bool>* e, uint16_t outputSlot)`
  - Connects an output slot of another element to the input of this falling edge trigger element.

### le_Counter

Counter element with a settable final count. It counts the number of rising edge transitions on its input until it reaches the final count.

- **Constructor**: `le_Counter(uint16_t countFinal)`
  - Initializes the counter with a specified final count.
- **Update**: `void Update(float timeStep)`
  - Updates the counter by incrementing the count on rising edge transitions.
- **SetInput_CountUp**: `void SetInput_CountUp(le_Base<bool>* e, uint16_t outputSlot)`
  - Sets the input element for counting up.
- **SetInput_Reset**: `void SetInput_Reset(le_Base<bool>* e, uint16_t outputSlot)`
  - Sets the input element for resetting the counter.

### le_Node

Template class representing a node with historical data. It stores a history of values and provides access to historical data.

- **Constructor**: `le_Node(uint16_t historyLength)`
  - Initializes the node with a specified history length.
- **Destructor**: `~le_Node()`
  - Cleans up the dynamically allocated history buffer.
- **Update**: `void Update(float timeStep)`
  - Updates the node by storing the current input value in the history buffer.
- **GetHistory**: `void GetHistory(T** buffer, uint16_t* startOffset)`
  - Retrieves the history buffer with an offset from the circular buffer.
- **Connect**: `void Connect(le_Base<T>* e, uint16_t outputSlot)`
  - Connects an output slot of another element to the input of this node.

### le_Overcurrent

Overcurrent protection element with customizable curves. It trips when the current exceeds a specified pickup value for a certain duration, based on the curve parameters.

- **Constructor**: `le_Overcurrent(std::string curve, float pickup, float timeDial, float timeAdder, bool emReset)`
  - Initializes the overcurrent protection element with specified parameters.
- **Update**: `void Update(float timeStep)`
  - Updates the overcurrent protection element by calculating the trip time based on the current and curve parameters.
- **SetInput**: `void SetInput(le_Base<float>* e)`
  - Sets the input element providing the current value.

### le_Timer

Timer element with pickup and dropout times. It activates after the input is true for the pickup time and deactivates after the input is false for the dropout time.

- **Constructor**: `le_Timer(float pickup, float dropout)`
  - Initializes the timer element with specified pickup and dropout times.
- **Update**: `void Update(float timeStep)`
  - Updates the timer element by tracking the input state and adjusting the timer accordingly.
- **Connect**: `void Connect(le_Base<bool>* e, uint16_t outputSlot)`
  - Connects an output slot of another element to the input of this timer element.


## Contributing

Contributions are welcome! Please fork the repository and submit a pull request with your changes.

## License

This project is licensed under the MIT License. See the LICENSE file for details.
