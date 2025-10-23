#include "test_framework.hpp"
#include "le_SER.hpp"

bool test_SER_basic()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "IN1", ElementType::NodeDigital);
    TestFramework::CreateElement(&engine, "IN2", ElementType::NodeDigital);
    TestFramework::CreateElement(&engine, "SER", ElementType::SER, 2); // 2 inputs
    
    TestFramework::ConnectElements(&engine, "IN1", "output", "SER", "input_0");
    TestFramework::ConnectElements(&engine, "IN2", "output", "SER", "input_1");
    
    NodeDigital* in1 = (NodeDigital*)engine.GetElement("IN1");
    NodeDigital* in2 = (NodeDigital*)engine.GetElement("IN2");
    SER* ser = (SER*)engine.GetElement("SER");
    
    Time t = Time::GetTime();
    
    // Initial state
    in1->SetValue(false);
    in2->SetValue(false);
    engine.Update(t);
    
    // Trigger events
    in1->SetValue(true);  // Rising edge on input 0
    engine.Update(t);
    
    in2->SetValue(true);  // Rising edge on input 1
    engine.Update(t);
    
    // Check event log
    SER::SEREvent events[10];
    uint16_t count = ser->GetEventLog(events, 10);
    ASSERT_TRUE(count >= 2); // At least 2 events recorded
    
    return true;
}

bool test_SER_port_names()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "SER", ElementType::SER, 3);
    Element* ser = engine.GetElement("SER");
    
    // Verify input ports exist (SER has no outputs - recorder only)
    ASSERT_TRUE(ser->GetInputPort("input_0") != nullptr);
    ASSERT_TRUE(ser->GetInputPort("input_1") != nullptr);
    ASSERT_TRUE(ser->GetInputPort("input_2") != nullptr);
    ASSERT_EQUAL(ser->GetOutputPortCount(), (size_t)0); // No outputs!
    
    return true;
}

void RunSERTests()
{
    std::cout << "\n=== SER Tests ===" << std::endl;
    TestFramework::RunTest("SER - Basic event recording", test_SER_basic);
    TestFramework::RunTest("SER - Port names", test_SER_port_names);
}

