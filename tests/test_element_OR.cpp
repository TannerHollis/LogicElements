#include "test_framework.hpp"

using namespace LogicElements;

bool test_OR_2inputs()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "IN1", ElementType::NodeDigital);
    TestFramework::CreateElement(&engine, "IN2", ElementType::NodeDigital);
    TestFramework::CreateElement(&engine, "OR1", ElementType::OR, 2);
    TestFramework::CreateElement(&engine, "OUT", ElementType::NodeDigital);
    
    TestFramework::ConnectElements(&engine, "IN1", "output", "OR1", "input_0");
    TestFramework::ConnectElements(&engine, "IN2", "output", "OR1", "input_1");
    TestFramework::ConnectElements(&engine, "OR1", "output", "OUT", "input");
    
    NodeDigital* in1 = (NodeDigital*)engine.GetElement("IN1");
    NodeDigital* in2 = (NodeDigital*)engine.GetElement("IN2");
    NodeDigital* out = (NodeDigital*)engine.GetElement("OUT");
    
    Time t = Time::GetTime();
    
    // Test: false OR false = false
    in1->SetValue(false);
    in2->SetValue(false);
    engine.Update(t);
    ASSERT_FALSE(out->GetOutput());
    
    // Test: true OR false = true
    in1->SetValue(true);
    engine.Update(t);
    ASSERT_TRUE(out->GetOutput());
    
    // Test: false OR true = true
    in1->SetValue(false);
    in2->SetValue(true);
    engine.Update(t);
    ASSERT_TRUE(out->GetOutput());
    
    // Test: true OR true = true
    in1->SetValue(true);
    engine.Update(t);
    ASSERT_TRUE(out->GetOutput());
    
    return true;
}

bool test_OR_3inputs()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "IN1", ElementType::NodeDigital);
    TestFramework::CreateElement(&engine, "IN2", ElementType::NodeDigital);
    TestFramework::CreateElement(&engine, "IN3", ElementType::NodeDigital);
    TestFramework::CreateElement(&engine, "OR1", ElementType::OR, 3);
    TestFramework::CreateElement(&engine, "OUT", ElementType::NodeDigital);
    
    TestFramework::ConnectElements(&engine, "IN1", "output", "OR1", "input_0");
    TestFramework::ConnectElements(&engine, "IN2", "output", "OR1", "input_1");
    TestFramework::ConnectElements(&engine, "IN3", "output", "OR1", "input_2");
    TestFramework::ConnectElements(&engine, "OR1", "output", "OUT", "input");
    
    NodeDigital* in1 = (NodeDigital*)engine.GetElement("IN1");
    NodeDigital* in2 = (NodeDigital*)engine.GetElement("IN2");
    NodeDigital* in3 = (NodeDigital*)engine.GetElement("IN3");
    NodeDigital* out = (NodeDigital*)engine.GetElement("OUT");
    
    Time t = Time::GetTime();
    
    // Test: all false = false
    in1->SetValue(false);
    in2->SetValue(false);
    in3->SetValue(false);
    engine.Update(t);
    ASSERT_FALSE(out->GetOutput());
    
    // Test: one true = true
    in2->SetValue(true);
    engine.Update(t);
    ASSERT_TRUE(out->GetOutput());
    
    return true;
}

bool test_OR_port_names()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "OR1", ElementType::OR, 2);
    Element* orGate = engine.GetElement("OR1");
    
    ASSERT_TRUE(orGate->GetInputPort("input_0") != nullptr);
    ASSERT_TRUE(orGate->GetInputPort("input_1") != nullptr);
    ASSERT_TRUE(orGate->GetOutputPort("output") != nullptr);
    
    return true;
}

void RunORTests()
{
    std::cout << "\n=== OR Tests ===" << std::endl;
    TestFramework::RunTest("OR - 2 inputs logic", test_OR_2inputs);
    TestFramework::RunTest("OR - 3 inputs logic", test_OR_3inputs);
    TestFramework::RunTest("OR - Port names", test_OR_port_names);
}

