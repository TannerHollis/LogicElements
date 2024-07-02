#pragma once

#include "le_Base.hpp"
#include "le_AND.hpp"
#include "le_OR.hpp"
#include "le_NOT.hpp"
#include "le_RTrig.hpp"
#include "le_FTrig.hpp"
#include "le_Node.hpp"
#include "le_Timer.hpp"
#include "le_Counter.hpp"
#include "le_Overcurrent.hpp"

// Include STD C++ libs
#include <string>
#include <map>
#include <algorithm>
#include <cstdio>
#include <cstring>

#define LE_ELEMENT_NAME_LENGTH 8
#define LE_ELEMENT_ARGUMENT_LENGTH 4

enum class le_Element_Type : int8_t {
	LE_AND = 0,
	LE_OR = 1,
	LE_NOT = 2,
	LE_RTRIG = 3,
	LE_FTRIG = 4,
	LE_NODE_DIGITAL = 10,
	LE_NODE_ANALOG = 11,
	LE_TIMER = 20,
	LE_COUNTER = 21,
	LE_OVERCURRENT = 30,
	LE_INVALID = -1
};

class le_Engine
{
public:
	static le_Element_Type ParseElementType(std::string& type);

	static void CopyAndClampString(std::string src, char* dst, uint16_t dstLength);

public:	
	typedef union le_Element_Argument_TypeDef
	{
		char sArg[LE_ELEMENT_ARGUMENT_LENGTH];
		float fArg;
		uint16_t uArg;
		bool bArg;
	} le_Element_Argument_TypeDef;

	typedef struct le_Element_TypeDef
	{
		char name[LE_ELEMENT_NAME_LENGTH];
		int8_t type;
		le_Element_Argument_TypeDef args[5];
	} le_Element_TypeDef;

	typedef struct le_Element_Net_Connection_TypeDef
	{
		char name[LE_ELEMENT_NAME_LENGTH];
		uint16_t slot;
	} le_Element_Net_Connection_TypeDef;

	typedef struct le_Element_Net_TypeDef
	{
		le_Element_Net_Connection_TypeDef output;
		std::vector<le_Element_Net_Connection_TypeDef> inputs;
		
		void SetOutput(const char(&elementName)[LE_ELEMENT_NAME_LENGTH], uint16_t outputSlot);
		void SetOutput(std::string elementName, uint16_t outputSlot);

		void AddInput(const char(&elementName)[LE_ELEMENT_NAME_LENGTH], uint16_t inputSlot);
		void AddInput(std::string elementName, uint16_t inputSlot);
	} le_Element_Net_TypeDef;

	le_Engine(std::string name);
	
	~le_Engine();
	
	le_Element* AddElement(le_Element_TypeDef* comp);
	
	void AddNet(le_Element_Net_TypeDef* net);
	
	void Update(float timeStep);
	
	le_Element* GetElement(std::string elementName);

	std::string GetElementName(le_Element* e);

	void Print();

private:
	le_Element* AddElement(const std::string& name, le_Element* e);

	void SortElements();

	static bool CompareElementOrders(le_Element* left, le_Element* right);

	/* Engine Name */
	std::string sName;

	/* Digital Elements */
	std::vector<le_Element*> _elements;
	std::map<std::string, le_Element*> _elementsByName;
};
