#include "test_framework.hpp"

using namespace LogicElements;

/**
 * @brief Test AND element with 2 inputs using Engine factory
 */
bool test_AND_2inputs()
{
    Engine engine("TestEngine");
    
    // Create elements using factory
    TestFramework::CreateElement(&engine, "IN1", ElementType::NodeDigital);
    TestFramework::CreateElement(&engine, "IN2", ElementType::NodeDigital);
    TestFramework::CreateElement(&engine, "AND1", ElementType::AND, 2);
    TestFramework::CreateElement(&engine, "OUT", ElementType::NodeDigital);
    
    // Connect using engine
    TestFramework::ConnectElements(&engine, "IN1", "output", "AND1", "input_0");
    TestFramework::ConnectElements(&engine, "IN2", "output", "AND1", "input_1");
    TestFramework::ConnectElements(&engine, "AND1", "output", "OUT", "input");
    
    // Get elements
    NodeDigital* in1 = (NodeDigital*)engine.GetElement("IN1");
    NodeDigital* in2 = (NodeDigital*)engine.GetElement("IN2");
    NodeDigital* out = (NodeDigital*)engine.GetElement("OUT");
    
    Time t = Time::GetTime();
    
    // Test: false AND false = false
    in1->SetValue(false);
    in2->SetValue(false);
    engine.Update(t);
    ASSERT_FALSE(out->GetOutput());
    
    // Test: true AND false = false
    in1->SetValue(true);
    engine.Update(t);
    ASSERT_FALSE(out->GetOutput());
    
    // Test: true AND true = true
    in2->SetValue(true);
    engine.Update(t);
    ASSERT_TRUE(out->GetOutput());
    
    // Test: false AND true = false
    in1->SetValue(false);
    engine.Update(t);
    ASSERT_FALSE(out->GetOutput());
    
    return true;
}

/**
 * @brief Test AND element with 4 inputs
 */
bool test_AND_4inputs()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "IN1", ElementType::NodeDigital);
    TestFramework::CreateElement(&engine, "IN2", ElementType::NodeDigital);
    TestFramework::CreateElement(&engine, "IN3", ElementType::NodeDigital);
    TestFramework::CreateElement(&engine, "IN4", ElementType::NodeDigital);
    TestFramework::CreateElement(&engine, "AND1", ElementType::AND, 4);
    TestFramework::CreateElement(&engine, "OUT", ElementType::NodeDigital);
    
    TestFramework::ConnectElements(&engine, "IN1", "output", "AND1", "input_0");
    TestFramework::ConnectElements(&engine, "IN2", "output", "AND1", "input_1");
    TestFramework::ConnectElements(&engine, "IN3", "output", "AND1", "input_2");
    TestFramework::ConnectElements(&engine, "IN4", "output", "AND1", "input_3");
    TestFramework::ConnectElements(&engine, "AND1", "output", "OUT", "input");
    
    NodeDigital* in1 = (NodeDigital*)engine.GetElement("IN1");
    NodeDigital* in2 = (NodeDigital*)engine.GetElement("IN2");
    NodeDigital* in3 = (NodeDigital*)engine.GetElement("IN3");
    NodeDigital* in4 = (NodeDigital*)engine.GetElement("IN4");
    NodeDigital* out = (NodeDigital*)engine.GetElement("OUT");
    
    Time t = Time::GetTime();
    
    // Test: all true = true
    in1->SetValue(true);
    in2->SetValue(true);
    in3->SetValue(true);
    in4->SetValue(true);
    engine.Update(t);
    ASSERT_TRUE(out->GetOutput());
    
    // Test: one false = false
    in3->SetValue(false);
    engine.Update(t);
    ASSERT_FALSE(out->GetOutput());
    
    return true;
}

/**
 * @brief Test AND port names
 */
bool test_AND_port_names()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "AND1", ElementType::AND, 3);
    Element* andGate = engine.GetElement("AND1");
    
    // Verify named input ports exist
    ASSERT_TRUE(andGate->GetInputPort("input_0") != nullptr);
    ASSERT_TRUE(andGate->GetInputPort("input_1") != nullptr);
    ASSERT_TRUE(andGate->GetInputPort("input_2") != nullptr);
    
    // Verify output port exists
    ASSERT_TRUE(andGate->GetOutputPort("output") != nullptr);
    
    // Verify port types
    ASSERT_EQUAL(andGate->GetInputPort("input_0")->GetType(), PortType::DIGITAL);
    ASSERT_EQUAL(andGate->GetOutputPort("output")->GetType(), PortType::DIGITAL);
    
    return true;
}

void RunANDTests()
{
    std::cout << "\n=== AND Tests ===" << std::endl;
    TestFramework::RunTest("AND - 2 inputs logic", test_AND_2inputs);
    TestFramework::RunTest("AND - 4 inputs logic", test_AND_4inputs);
    TestFramework::RunTest("AND - Port names", test_AND_port_names);
}

