#include "test_framework.hpp"

using namespace LogicElements;

bool test_NOT_basic()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "IN", ElementType::NodeDigital);
    TestFramework::CreateElement(&engine, "NOT1", ElementType::NOT);
    TestFramework::CreateElement(&engine, "OUT", ElementType::NodeDigital);
    
    TestFramework::ConnectElements(&engine, "IN", LE_PORT_OUTPUT_PREFIX, "NOT1", LE_PORT_INPUT_PREFIX);
    TestFramework::ConnectElements(&engine, "NOT1", LE_PORT_OUTPUT_PREFIX, "OUT", LE_PORT_INPUT_PREFIX);
    
    NodeDigital* in = (NodeDigital*)engine.GetElement("IN");
    NodeDigital* out = (NodeDigital*)engine.GetElement("OUT");
    
    Time t = Time::GetTime();
    
    // Test: NOT false = true
    in->SetValue(false);
    engine.Update(t);
    ASSERT_TRUE(out->GetOutput());
    
    // Test: NOT true = false
    in->SetValue(true);
    engine.Update(t);
    ASSERT_FALSE(out->GetOutput());
    
    return true;
}

bool test_NOT_double_inversion()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "IN", ElementType::NodeDigital);
    TestFramework::CreateElement(&engine, "NOT1", ElementType::NOT);
    TestFramework::CreateElement(&engine, "NOT2", ElementType::NOT);
    TestFramework::CreateElement(&engine, "OUT", ElementType::NodeDigital);
    
    TestFramework::ConnectElements(&engine, "IN", LE_PORT_OUTPUT_PREFIX, "NOT1", LE_PORT_INPUT_PREFIX);
    TestFramework::ConnectElements(&engine, "NOT1", LE_PORT_OUTPUT_PREFIX, "NOT2", LE_PORT_INPUT_PREFIX);
    TestFramework::ConnectElements(&engine, "NOT2", LE_PORT_OUTPUT_PREFIX, "OUT", LE_PORT_INPUT_PREFIX);
    
    NodeDigital* in = (NodeDigital*)engine.GetElement("IN");
    NodeDigital* out = (NodeDigital*)engine.GetElement("OUT");
    
    Time t = Time::GetTime();
    
    // Double inversion should restore original
    in->SetValue(true);
    engine.Update(t);
    ASSERT_TRUE(out->GetOutput());
    
    in->SetValue(false);
    engine.Update(t);
    ASSERT_FALSE(out->GetOutput());
    
    return true;
}

bool test_NOT_port_names()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "NOT1", ElementType::NOT);
    Element* notGate = engine.GetElement("NOT1");
    
    ASSERT_TRUE(notGate->GetInputPort("input") != nullptr);
    ASSERT_TRUE(notGate->GetOutputPort(LE_PORT_OUTPUT_PREFIX) != nullptr);
    ASSERT_EQUAL(notGate->GetInputPortCount(), (size_t)1);
    ASSERT_EQUAL(notGate->GetOutputPortCount(), (size_t)1);
    
    return true;
}

void RunNOTTests()
{
    std::cout << "\n=== NOT Tests ===" << std::endl;
    TestFramework::RunTest("NOT - Basic inversion", test_NOT_basic);
    TestFramework::RunTest("NOT - Double inversion", test_NOT_double_inversion);
    TestFramework::RunTest("NOT - Port names", test_NOT_port_names);
}

