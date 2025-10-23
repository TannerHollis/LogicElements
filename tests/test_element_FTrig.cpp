#include "test_framework.hpp"

using namespace LogicElements;

bool test_FTrig_falling_edge()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "IN", ElementType::NodeDigital);
    TestFramework::CreateElement(&engine, "FTRIG", ElementType::FTrig);
    TestFramework::CreateElement(&engine, "OUT", ElementType::NodeDigital);
    
    TestFramework::ConnectElements(&engine, "IN", LE_PORT_OUTPUT_PREFIX, "FTRIG", LE_PORT_INPUT_PREFIX);
    TestFramework::ConnectElements(&engine, "FTRIG", LE_PORT_OUTPUT_PREFIX, "OUT", LE_PORT_INPUT_PREFIX);
    
    NodeDigital* in = (NodeDigital*)engine.GetElement("IN");
    NodeDigital* out = (NodeDigital*)engine.GetElement("OUT");
    
    Time t = Time::GetTime();
    
    // Initial high
    in->SetValue(true);
    engine.Update(t);
    ASSERT_FALSE(out->GetOutput());
    
    // Falling edge: should trigger
    in->SetValue(false);
    engine.Update(t);
    ASSERT_TRUE(out->GetOutput());
    
    // Hold low: no trigger
    engine.Update(t);
    ASSERT_FALSE(out->GetOutput());
    
    // Rising edge: no trigger
    in->SetValue(true);
    engine.Update(t);
    ASSERT_FALSE(out->GetOutput());
    
    return true;
}

bool test_FTrig_port_names()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "FTRIG", ElementType::FTrig);
    Element* ftrig = engine.GetElement("FTRIG");
    
    ASSERT_TRUE(ftrig->GetInputPort("input") != nullptr);
    ASSERT_TRUE(ftrig->GetOutputPort(LE_PORT_OUTPUT_PREFIX) != nullptr);
    
    return true;
}

void RunFTrigTests()
{
    std::cout << "\n=== FTrig Tests ===" << std::endl;
    TestFramework::RunTest("FTrig - Falling edge detection", test_FTrig_falling_edge);
    TestFramework::RunTest("FTrig - Port names", test_FTrig_port_names);
}

