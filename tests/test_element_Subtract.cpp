#include "test_framework.hpp"

using namespace LogicElements;

#ifdef LE_ELEMENTS_ANALOG

bool test_Subtract_basic()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "IN0", ElementType::NodeAnalog);
    TestFramework::CreateElement(&engine, "IN1", ElementType::NodeAnalog);
    TestFramework::CreateElement(&engine, "SUB", ElementType::Subtract);
    TestFramework::CreateElement(&engine, "OUT", ElementType::NodeAnalog);
    
    TestFramework::ConnectElements(&engine, "IN0", "output", "SUB", "input_0");
    TestFramework::ConnectElements(&engine, "IN1", "output", "SUB", "input_1");
    TestFramework::ConnectElements(&engine, "SUB", "output", "OUT", "input");
    
    NodeAnalog* in0 = (NodeAnalog*)engine.GetElement("IN0");
    NodeAnalog* in1 = (NodeAnalog*)engine.GetElement("IN1");
    NodeAnalog* out = (NodeAnalog*)engine.GetElement("OUT");
    
    Time t = Time::GetTime();
    
    // Test: 10.0 - 3.0 = 7.0
    in0->SetValue(10.0f);
    in1->SetValue(3.0f);
    engine.Update(t);
    ASSERT_NEAR(out->GetOutput(), 7.0f, 0.001f);
    
    // Test: 5.0 - 7.0 = -2.0
    in0->SetValue(5.0f);
    in1->SetValue(7.0f);
    engine.Update(t);
    ASSERT_NEAR(out->GetOutput(), -2.0f, 0.001f);
    
    // Test: 0.0 - 0.0 = 0.0
    in0->SetValue(0.0f);
    in1->SetValue(0.0f);
    engine.Update(t);
    ASSERT_NEAR(out->GetOutput(), 0.0f, 0.001f);
    
    return true;
}

bool test_Subtract_port_names()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "SUB", ElementType::Subtract);
    Element* sub = engine.GetElement("SUB");
    
    ASSERT_TRUE(sub->GetInputPort("input_0") != nullptr);
    ASSERT_TRUE(sub->GetInputPort("input_1") != nullptr);
    ASSERT_TRUE(sub->GetOutputPort("output") != nullptr);
    ASSERT_EQUAL(sub->GetInputPort("input_0")->GetType(), PortType::ANALOG);
    
    return true;
}

void RunSubtractTests()
{
    std::cout << "\n=== Subtract Tests ===" << std::endl;
    TestFramework::RunTest("Subtract - Basic subtraction", test_Subtract_basic);
    TestFramework::RunTest("Subtract - Port names", test_Subtract_port_names);
}

#endif

