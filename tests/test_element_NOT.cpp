#include "test_framework.hpp"

using namespace LogicElements;

bool test_NOT_basic()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "IN", ElementType::NodeDigital);
    TestFramework::CreateElement(&engine, "NOT1", ElementType::NOT);
    TestFramework::CreateElement(&engine, "OUT", ElementType::NodeDigital);
    
    TestFramework::ConnectElements(&engine, "IN", "output", "NOT1", "input");
    TestFramework::ConnectElements(&engine, "NOT1", "output", "OUT", "input");
    
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
    
    TestFramework::ConnectElements(&engine, "IN", "output", "NOT1", "input");
    TestFramework::ConnectElements(&engine, "NOT1", "output", "NOT2", "input");
    TestFramework::ConnectElements(&engine, "NOT2", "output", "OUT", "input");
    
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
    ASSERT_TRUE(notGate->GetOutputPort("output") != nullptr);
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

