#include "le_Engine.hpp"

le_Element_Type le_Engine::ParseElementType(std::string& type)
{
	if (type == "LE_AND") return le_Element_Type::LE_AND;
	if (type == "LE_OR") return le_Element_Type::LE_OR;
	if (type == "LE_NOT") return le_Element_Type::LE_NOT;
	if (type == "LE_RTRIG") return le_Element_Type::LE_RTRIG;
	if (type == "LE_FTRIG") return le_Element_Type::LE_FTRIG;
	if (type == "LE_NODE_DIGITAL") return le_Element_Type::LE_NODE_DIGITAL;
	if (type == "LE_NODE_ANALOG") return le_Element_Type::LE_NODE_ANALOG;
	if (type == "LE_TIMER") return le_Element_Type::LE_TIMER;
	if (type == "LE_COUNTER") return le_Element_Type::LE_COUNTER;
	if (type == "LE_OVERCURRENT") return le_Element_Type::LE_OVERCURRENT;

	return le_Element_Type::LE_INVALID;
}

void le_Engine::CopyAndClampString(std::string src, char* dst, uint16_t dstLength)
{
	// Adjust copying range
	uint16_t cpyLength = dstLength;
	if (src.length() < dstLength)
		cpyLength = (uint16_t)src.length();

	// Perform mem copy
	strncpy(dst, src.c_str(), cpyLength);
	if (src.length() < dstLength)
		dst[cpyLength] = '\0';
	else
		dst[dstLength - 1] = '\0';
}

le_Engine::le_Engine(std::string name)
{
	// Set extrinsic variables
	this->sName = name;

	// Set intrinsic variables
	this->_elements = std::vector<le_Element*>();
	this->_elementsByName = std::map<std::string, le_Element*>();
}

le_Engine::~le_Engine()
{
	// Iterate through elements and call destructor
	for (le_Element* e : this->_elements)
	{
		delete e;
	}
}

le_Element* le_Engine::AddElement(le_Element_TypeDef* comp)
{
	// Conver to std::string
	std::string compName = std::string(comp->name);

	switch ((le_Element_Type)comp->type)
	{
	case le_Element_Type::LE_AND:
		return this->AddElement(compName, new le_AND(comp->args[0].uArg));

	case le_Element_Type::LE_OR:
		return this->AddElement(compName, new le_OR(comp->args[0].uArg));

	case le_Element_Type::LE_NOT:
		return this->AddElement(compName, new le_NOT());

	case le_Element_Type::LE_RTRIG:
		return this->AddElement(compName, new le_RTrig());

	case le_Element_Type::LE_FTRIG:
		return this->AddElement(compName, new le_FTrig());

	case le_Element_Type::LE_NODE_DIGITAL:
		return this->AddElement(compName, new le_Node<bool>(comp->args[0].uArg));

	case le_Element_Type::LE_NODE_ANALOG:
		return this->AddElement(compName, new le_Node<float>(comp->args[0].uArg));

	case le_Element_Type::LE_TIMER:
		return this->AddElement(compName, new le_Timer(comp->args[0].fArg, comp->args[1].fArg));

	case le_Element_Type::LE_COUNTER:
		return this->AddElement(compName, new le_Counter(comp->args[0].uArg));

	case le_Element_Type::LE_OVERCURRENT:
	{
		std::string curveName = std::string(comp->args[0].sArg);
		return this->AddElement(compName, new le_Overcurrent(
			comp->args[0].sArg, // Curve type
			comp->args[1].fArg, // Pickup value
			comp->args[2].fArg, // Time Dial
			comp->args[3].fArg,  // Time Adder
			comp->args[4].bArg)); // Electromechanical reset
	}

	default:
		// TODO : implement printf() function
		// printf("Unknown component type\n");
		return nullptr;
	}
}

void le_Engine::AddNet(le_Element_Net_TypeDef* net)
{
	// Get number of inputs
	uint16_t n = (uint16_t)net->inputs.size();

	// Get output element
	std::string outputName = std::string(net->output.name);
	le_Element* outputElement = this->GetElement(outputName);
	if (outputElement == nullptr)
		return;

	// Iterate through inputs and connect
	for (uint16_t i = 0; i < n; i++)
	{
		// Get input element
		std::string inputName = std::string(net->inputs[i].name);
		le_Element* inputElement = this->GetElement(inputName);

		// If element doesn't exist
		if (inputElement == nullptr)
			continue;

		// Connect elements
		le_Element::Connect(outputElement, net->output.slot, inputElement, net->inputs[i].slot);
	}

	// Sort elements
	this->SortElements();
}

void le_Engine::Update(float timeStep)
{
	// Call each element update individually by highest to lowest order
	for (le_Element* e : this->_elements)
	{
		e->Update(timeStep);
	}
}

le_Element* le_Engine::GetElement(std::string elementName)
{
	// Find element by name
	auto it = this->_elementsByName.find(elementName);

	// If key doesn't exist, return nullptr
	if (it == this->_elementsByName.end())
		return nullptr;
	else
		return it->second;
}

std::string le_Engine::GetElementName(le_Element* e)
{
	for (const auto& pair : this->_elementsByName) {
		if (pair.second == e) {
			return pair.first;
		}
	}
	return ""; // Return an empty string if the value is not found
}

void le_Engine::Print()
{
	printf("Engine Name: %s\r\n", this->sName.c_str());
	for (le_Element* e : this->_elements)
	{
		std::string elementName = this->GetElementName(e);
		printf("  Element: %-8s \tOrder: %-3u\r\n", elementName.c_str(), e->GetOrder());
	}
}

le_Element* le_Engine::AddElement(const std::string& name, le_Element* e)
{
	// Create new map member
	std::pair<std::string, le_Element*> newElement = std::make_pair(name, e);

	// Insert new memeber into map
	auto insertResult = this->_elementsByName.insert(newElement);

	// Add element to vector
	this->_elements.push_back(insertResult.first->second);

	// Sort elements
	this->SortElements();

	return insertResult.first->second;
}

void le_Engine::SortElements()
{
	std::sort(this->_elements.begin(), this->_elements.end(), le_Engine::CompareElementOrders);
}

bool le_Engine::CompareElementOrders(le_Element* left, le_Element* right)
{
	return left->GetOrder() < right->GetOrder();
}

void le_Engine::le_Element_Net_TypeDef::SetOutput(const char(&elementName)[LE_ELEMENT_NAME_LENGTH], uint16_t outputSlot)
{
	this->SetOutput(std::string(elementName), outputSlot);
}

void le_Engine::le_Element_Net_TypeDef::SetOutput(std::string elementName, uint16_t outputSlot)
{
	// Create output
	le_Engine::CopyAndClampString(elementName, this->output.name, LE_ELEMENT_NAME_LENGTH);
	this->output.slot = outputSlot;
}

void le_Engine::le_Element_Net_TypeDef::AddInput(const char(&elementName)[LE_ELEMENT_NAME_LENGTH], uint16_t inputSlot)
{
	this->AddInput(std::string(elementName), inputSlot);
}

void le_Engine::le_Element_Net_TypeDef::AddInput(std::string elementName, uint16_t inputSlot)
{
	// Create connection
	le_Element_Net_Connection_TypeDef c;
	le_Engine::CopyAndClampString(elementName, c.name, LE_ELEMENT_NAME_LENGTH);
	c.slot = inputSlot; // Set slot

	// Add to inputs
	this->inputs.push_back(c);
}