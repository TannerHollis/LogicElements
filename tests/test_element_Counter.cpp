#include "test_framework.hpp"

using namespace LogicElements;

bool test_Counter_basic()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "CNT_IN", ElementType::NodeDigital);
    TestFramework::CreateElement(&engine, "RST_IN", ElementType::NodeDigital);
    TestFramework::CreateElement(&engine, "CNT", ElementType::Counter, 3); // Count to 3
    TestFramework::CreateElement(&engine, "OUT", ElementType::NodeDigital);
    
    TestFramework::ConnectElements(&engine, "CNT_IN", "output", "CNT", "count_up");
    TestFramework::ConnectElements(&engine, "RST_IN", "output", "CNT", "reset");
    TestFramework::ConnectElements(&engine, "CNT", "output", "OUT", "input");
    
    NodeDigital* cntIn = (NodeDigital*)engine.GetElement("CNT_IN");
    NodeDigital* rstIn = (NodeDigital*)engine.GetElement("RST_IN");
    NodeDigital* out = (NodeDigital*)engine.GetElement("OUT");
    
    Time t = Time::GetTime();
    
    // Initialize
    cntIn->SetValue(false);
    rstIn->SetValue(false);
    engine.Update(t);
    ASSERT_FALSE(out->GetOutput());
    
    // Count 1
    cntIn->SetValue(true);
    engine.Update(t);
    cntIn->SetValue(false);
    engine.Update(t);
    ASSERT_FALSE(out->GetOutput());
    
    // Count 2
    cntIn->SetValue(true);
    engine.Update(t);
    cntIn->SetValue(false);
    engine.Update(t);
    ASSERT_FALSE(out->GetOutput());
    
    // Count 3 - should output true
    cntIn->SetValue(true);
    engine.Update(t);
    ASSERT_TRUE(out->GetOutput());
    
    // Reset
    rstIn->SetValue(true);
    engine.Update(t);
    ASSERT_FALSE(out->GetOutput());
    
    return true;
}

bool test_Counter_port_names()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "CNT", ElementType::Counter, 5);
    Element* counter = engine.GetElement("CNT");
    
    ASSERT_TRUE(counter->GetInputPort("count_up") != nullptr);
    ASSERT_TRUE(counter->GetInputPort("reset") != nullptr);
    ASSERT_TRUE(counter->GetOutputPort("output") != nullptr);
    
    return true;
}

void RunCounterTests()
{
    std::cout << "\n=== Counter Tests ===" << std::endl;
    TestFramework::RunTest("Counter - Basic counting", test_Counter_basic);
    TestFramework::RunTest("Counter - Port names", test_Counter_port_names);
}

