#include "test_framework.hpp"

using namespace LogicElements;

#ifdef LE_ELEMENTS_ANALOG

bool test_Add_basic()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "IN0", ElementType::NodeAnalog);
    TestFramework::CreateElement(&engine, "IN1", ElementType::NodeAnalog);
    TestFramework::CreateElement(&engine, "ADD", ElementType::Add);
    TestFramework::CreateElement(&engine, "OUT", ElementType::NodeAnalog);
    
    TestFramework::ConnectElements(&engine, "IN0", "output", "ADD", "input_0");
    TestFramework::ConnectElements(&engine, "IN1", "output", "ADD", "input_1");
    TestFramework::ConnectElements(&engine, "ADD", "output", "OUT", "input");
    
    NodeAnalog* in0 = (NodeAnalog*)engine.GetElement("IN0");
    NodeAnalog* in1 = (NodeAnalog*)engine.GetElement("IN1");
    NodeAnalog* out = (NodeAnalog*)engine.GetElement("OUT");
    
    Time t = Time::GetTime();
    
    // Test: 5.0 + 3.0 = 8.0
    in0->SetValue(5.0f);
    in1->SetValue(3.0f);
    engine.Update(t);
    ASSERT_NEAR(out->GetOutput(), 8.0f, 0.001f);
    
    // Test: -2.5 + 7.5 = 5.0
    in0->SetValue(-2.5f);
    in1->SetValue(7.5f);
    engine.Update(t);
    ASSERT_NEAR(out->GetOutput(), 5.0f, 0.001f);
    
    // Test: 0.0 + 0.0 = 0.0
    in0->SetValue(0.0f);
    in1->SetValue(0.0f);
    engine.Update(t);
    ASSERT_NEAR(out->GetOutput(), 0.0f, 0.001f);
    
    return true;
}

bool test_Add_port_names()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "ADD", ElementType::Add);
    Element* add = engine.GetElement("ADD");
    
    ASSERT_TRUE(add->GetInputPort("input_0") != nullptr);
    ASSERT_TRUE(add->GetInputPort("input_1") != nullptr);
    ASSERT_TRUE(add->GetOutputPort("output") != nullptr);
    ASSERT_EQUAL(add->GetInputPort("input_0")->GetType(), PortType::ANALOG);
    ASSERT_EQUAL(add->GetOutputPort("output")->GetType(), PortType::ANALOG);
    
    return true;
}

void RunAddTests()
{
    std::cout << "\n=== Add Tests ===" << std::endl;
    TestFramework::RunTest("Add - Basic addition", test_Add_basic);
    TestFramework::RunTest("Add - Port names", test_Add_port_names);
}

#endif

