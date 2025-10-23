#include "test_framework.hpp"
#include <thread>
#include <chrono>

bool test_Timer_basic()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "IN", ElementType::NodeDigital);
    TestFramework::CreateElement(&engine, "TMR", ElementType::Timer, 0.1f, 0.05f, 0.0f, 0.0f, 0.0f); // 100ms pickup, 50ms dropout
    TestFramework::CreateElement(&engine, "OUT", ElementType::NodeDigital);
    
    TestFramework::ConnectElements(&engine, "IN", "output", "TMR", "input");
    TestFramework::ConnectElements(&engine, "TMR", "output", "OUT", "input");
    
    NodeDigital* in = (NodeDigital*)engine.GetElement("IN");
    NodeDigital* out = (NodeDigital*)engine.GetElement("OUT");
    
    Time t0 = Time::GetTime();
    
    // Initially off
    in->SetValue(false);
    engine.Update(t0);
    ASSERT_FALSE(out->GetOutput());
    
    // Turn on - not yet picked up
    in->SetValue(true);
    engine.Update(t0);
    std::cout << "  After setting input true (same timestamp): output=" << out->GetOutput() << " (expected: false)" << std::endl;
    ASSERT_FALSE(out->GetOutput());
    
    // Wait for pickup
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    Time t1 = Time::GetTime();
    engine.Update(t1);
    ASSERT_TRUE(out->GetOutput());
    
    return true;
}

bool test_Timer_port_names()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "TMR", ElementType::Timer, 0.1f, 0.05f, 0.0f, 0.0f, 0.0f);
    Element* timer = engine.GetElement("TMR");
    
    ASSERT_TRUE(timer->GetInputPort("input") != nullptr);
    ASSERT_TRUE(timer->GetOutputPort("output") != nullptr);
    
    return true;
}

void RunTimerTests()
{
    std::cout << "\n=== Timer Tests ===" << std::endl;
    TestFramework::RunTest("Timer - Basic pickup", test_Timer_basic);
    TestFramework::RunTest("Timer - Port names", test_Timer_port_names);
}

