#include "test_framework.hpp"

using namespace LogicElements;

#ifdef LE_ELEMENTS_ANALOG

bool test_Multiply_basic()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "IN0", ElementType::NodeAnalog);
    TestFramework::CreateElement(&engine, "IN1", ElementType::NodeAnalog);
    TestFramework::CreateElement(&engine, "MUL", ElementType::Multiply);
    TestFramework::CreateElement(&engine, "OUT", ElementType::NodeAnalog);
    
    TestFramework::ConnectElements(&engine, "IN0", "output", "MUL", "input_0");
    TestFramework::ConnectElements(&engine, "IN1", "output", "MUL", "input_1");
    TestFramework::ConnectElements(&engine, "MUL", "output", "OUT", "input");
    
    NodeAnalog* in0 = (NodeAnalog*)engine.GetElement("IN0");
    NodeAnalog* in1 = (NodeAnalog*)engine.GetElement("IN1");
    NodeAnalog* out = (NodeAnalog*)engine.GetElement("OUT");
    
    Time t = Time::GetTime();
    
    // Test: 5.0 * 3.0 = 15.0
    in0->SetValue(5.0f);
    in1->SetValue(3.0f);
    engine.Update(t);
    ASSERT_NEAR(out->GetOutput(), 15.0f, 0.001f);
    
    // Test: -2.0 * 4.0 = -8.0
    in0->SetValue(-2.0f);
    in1->SetValue(4.0f);
    engine.Update(t);
    ASSERT_NEAR(out->GetOutput(), -8.0f, 0.001f);
    
    // Test: 0.0 * 100.0 = 0.0
    in0->SetValue(0.0f);
    in1->SetValue(100.0f);
    engine.Update(t);
    ASSERT_NEAR(out->GetOutput(), 0.0f, 0.001f);
    
    return true;
}

bool test_Multiply_port_names()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "MUL", ElementType::Multiply);
    Element* mul = engine.GetElement("MUL");
    
    ASSERT_TRUE(mul->GetInputPort("input_0") != nullptr);
    ASSERT_TRUE(mul->GetInputPort("input_1") != nullptr);
    ASSERT_TRUE(mul->GetOutputPort("output") != nullptr);
    ASSERT_EQUAL(mul->GetInputPort("input_0")->GetType(), PortType::ANALOG);
    
    return true;
}

void RunMultiplyTests()
{
    std::cout << "\n=== Multiply Tests ===" << std::endl;
    TestFramework::RunTest("Multiply - Basic multiplication", test_Multiply_basic);
    TestFramework::RunTest("Multiply - Port names", test_Multiply_port_names);
}

#endif

