#include "test_framework.hpp"

using namespace LogicElements;

#ifdef LE_ELEMENTS_ANALOG

bool test_Divide_basic()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "IN0", ElementType::NodeAnalog);
    TestFramework::CreateElement(&engine, "IN1", ElementType::NodeAnalog);
    TestFramework::CreateElement(&engine, "DIV", ElementType::Divide);
    TestFramework::CreateElement(&engine, "OUT", ElementType::NodeAnalog);
    
    TestFramework::ConnectElements(&engine, "IN0", "output", "DIV", "input_0");
    TestFramework::ConnectElements(&engine, "IN1", "output", "DIV", "input_1");
    TestFramework::ConnectElements(&engine, "DIV", "output", "OUT", "input");
    
    NodeAnalog* in0 = (NodeAnalog*)engine.GetElement("IN0");
    NodeAnalog* in1 = (NodeAnalog*)engine.GetElement("IN1");
    NodeAnalog* out = (NodeAnalog*)engine.GetElement("OUT");
    
    Time t = Time::GetTime();
    
    // Test: 10.0 / 2.0 = 5.0
    in0->SetValue(10.0f);
    in1->SetValue(2.0f);
    engine.Update(t);
    ASSERT_NEAR(out->GetOutput(), 5.0f, 0.001f);
    
    // Test: 7.5 / 2.5 = 3.0
    in0->SetValue(7.5f);
    in1->SetValue(2.5f);
    engine.Update(t);
    ASSERT_NEAR(out->GetOutput(), 3.0f, 0.001f);
    
    // Test: Divide by zero protection (returns 0)
    in0->SetValue(10.0f);
    in1->SetValue(0.0f);
    engine.Update(t);
    ASSERT_NEAR(out->GetOutput(), 0.0f, 0.001f);
    
    return true;
}

bool test_Divide_port_names()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "DIV", ElementType::Divide);
    Element* div = engine.GetElement("DIV");
    
    ASSERT_TRUE(div->GetInputPort("input_0") != nullptr);
    ASSERT_TRUE(div->GetInputPort("input_1") != nullptr);
    ASSERT_TRUE(div->GetOutputPort("output") != nullptr);
    ASSERT_EQUAL(div->GetInputPort("input_0")->GetType(), PortType::ANALOG);
    
    return true;
}

void RunDivideTests()
{
    std::cout << "\n=== Divide Tests ===" << std::endl;
    TestFramework::RunTest("Divide - Basic division", test_Divide_basic);
    TestFramework::RunTest("Divide - Port names", test_Divide_port_names);
}

#endif

