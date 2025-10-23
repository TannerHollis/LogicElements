#include "test_framework.hpp"

using namespace LogicElements;

bool test_RTrig_rising_edge()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "IN", ElementType::NodeDigital);
    TestFramework::CreateElement(&engine, "RTRIG", ElementType::RTrig);
    TestFramework::CreateElement(&engine, "OUT", ElementType::NodeDigital);
    
    TestFramework::ConnectElements(&engine, "IN", LE_PORT_OUTPUT_PREFIX, "RTRIG", LE_PORT_INPUT_PREFIX);
    TestFramework::ConnectElements(&engine, "RTRIG", LE_PORT_OUTPUT_PREFIX, "OUT", LE_PORT_INPUT_PREFIX);
    
    NodeDigital* in = (NodeDigital*)engine.GetElement("IN");
    NodeDigital* out = (NodeDigital*)engine.GetElement("OUT");
    
    Time t = Time::GetTime();
    
    // Initial low
    in->SetValue(false);
    engine.Update(t);
    ASSERT_FALSE(out->GetOutput());
    
    // Rising edge: should trigger
    in->SetValue(true);
    engine.Update(t);
    ASSERT_TRUE(out->GetOutput());
    
    // Hold high: no trigger
    engine.Update(t);
    ASSERT_FALSE(out->GetOutput());
    
    // Falling edge: no trigger
    in->SetValue(false);
    engine.Update(t);
    ASSERT_FALSE(out->GetOutput());
    
    return true;
}

bool test_RTrig_multiple_edges()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "IN", ElementType::NodeDigital);
    TestFramework::CreateElement(&engine, "RTRIG", ElementType::RTrig);
    TestFramework::CreateElement(&engine, "OUT", ElementType::NodeDigital);
    
    TestFramework::ConnectElements(&engine, "IN", LE_PORT_OUTPUT_PREFIX, "RTRIG", LE_PORT_INPUT_PREFIX);
    TestFramework::ConnectElements(&engine, "RTRIG", LE_PORT_OUTPUT_PREFIX, "OUT", LE_PORT_INPUT_PREFIX);
    
    NodeDigital* in = (NodeDigital*)engine.GetElement("IN");
    NodeDigital* out = (NodeDigital*)engine.GetElement("OUT");
    
    Time t = Time::GetTime();
    
    // First rising edge
    in->SetValue(false);
    engine.Update(t);
    in->SetValue(true);
    engine.Update(t);
    ASSERT_TRUE(out->GetOutput());
    
    // Second rising edge
    in->SetValue(false);
    engine.Update(t);
    in->SetValue(true);
    engine.Update(t);
    ASSERT_TRUE(out->GetOutput());
    
    return true;
}

void RunRTrigTests()
{
    std::cout << "\n=== RTrig Tests ===" << std::endl;
    TestFramework::RunTest("RTrig - Rising edge detection", test_RTrig_rising_edge);
    TestFramework::RunTest("RTrig - Multiple edges", test_RTrig_multiple_edges);
}

