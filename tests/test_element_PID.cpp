#include "test_framework.hpp"

using namespace LogicElements;

#ifdef LE_ELEMENTS_PID

bool test_PID_proportional()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "SP", ElementType::NodeAnalog);
    TestFramework::CreateElement(&engine, "FB", ElementType::NodeAnalog);
    TestFramework::CreateElement(&engine, "PID", ElementType::PID, 1.0f, 0.0f, 0.0f, -100.0f, 100.0f);
    TestFramework::CreateElement(&engine, "OUT", ElementType::NodeAnalog);
    
    TestFramework::ConnectElements(&engine, "SP", LE_PORT_OUTPUT_PREFIX, "PID", "setpoint");
    TestFramework::ConnectElements(&engine, "FB", LE_PORT_OUTPUT_PREFIX, "PID", "feedback");
    TestFramework::ConnectElements(&engine, "PID", LE_PORT_OUTPUT_PREFIX, "OUT", LE_PORT_INPUT_PREFIX);
    
    NodeAnalog* sp = (NodeAnalog*)engine.GetElement("SP");
    NodeAnalog* fb = (NodeAnalog*)engine.GetElement("FB");
    NodeAnalog* out = (NodeAnalog*)engine.GetElement("OUT");
    
    Time t = Time::GetTime();
    
    // Test P-only: error = 50 - 30 = 20, output = 1.0 * 20 = 20
    sp->SetValue(50.0f);
    fb->SetValue(30.0f);
    engine.Update(t);
    
    ASSERT_NEAR(out->GetOutput(), 20.0f, 0.1f);
    
    // Test negative error: error = 30 - 50 = -20
    sp->SetValue(30.0f);
    fb->SetValue(50.0f);
    engine.Update(t);
    
    ASSERT_NEAR(out->GetOutput(), -20.0f, 0.1f);
    
    return true;
}

bool test_PID_clamping()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "SP", ElementType::NodeAnalog);
    TestFramework::CreateElement(&engine, "FB", ElementType::NodeAnalog);
    TestFramework::CreateElement(&engine, "PID", ElementType::PID, 1.0f, 0.0f, 0.0f, -10.0f, 10.0f);
    TestFramework::CreateElement(&engine, "OUT", ElementType::NodeAnalog);
    
    TestFramework::ConnectElements(&engine, "SP", LE_PORT_OUTPUT_PREFIX, "PID", "setpoint");
    TestFramework::ConnectElements(&engine, "FB", LE_PORT_OUTPUT_PREFIX, "PID", "feedback");
    TestFramework::ConnectElements(&engine, "PID", LE_PORT_OUTPUT_PREFIX, "OUT", LE_PORT_INPUT_PREFIX);
    
    NodeAnalog* sp = (NodeAnalog*)engine.GetElement("SP");
    NodeAnalog* fb = (NodeAnalog*)engine.GetElement("FB");
    NodeAnalog* out = (NodeAnalog*)engine.GetElement("OUT");
    
    Time t = Time::GetTime();
    
    // Test upper clamp: error = 100 - 0 = 100, but clamped to 10
    sp->SetValue(100.0f);
    fb->SetValue(0.0f);
    engine.Update(t);
    
    ASSERT_NEAR(out->GetOutput(), 10.0f, 0.01f);
    
    // Test lower clamp: error = 0 - 100 = -100, but clamped to -10
    sp->SetValue(0.0f);
    fb->SetValue(100.0f);
    engine.Update(t);
    
    ASSERT_NEAR(out->GetOutput(), -10.0f, 0.01f);
    
    return true;
}

bool test_PID_port_names()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "PID", ElementType::PID, 1.0f, 0.0f, 0.0f, -100.0f, 100.0f);
    Element* pid = engine.GetElement("PID");
    
    // Verify named ports
    ASSERT_TRUE(pid->GetInputPort("setpoint") != nullptr);
    ASSERT_TRUE(pid->GetInputPort("feedback") != nullptr);
    ASSERT_TRUE(pid->GetOutputPort(LE_PORT_OUTPUT_PREFIX) != nullptr);
    
    // Verify all ANALOG types
    ASSERT_EQUAL(pid->GetInputPort("setpoint")->GetType(), PortType::ANALOG);
    ASSERT_EQUAL(pid->GetInputPort("feedback")->GetType(), PortType::ANALOG);
    ASSERT_EQUAL(pid->GetOutputPort(LE_PORT_OUTPUT_PREFIX)->GetType(), PortType::ANALOG);
    
    return true;
}

void RunPIDTests()
{
    std::cout << "\n=== PID Tests ===" << std::endl;
    TestFramework::RunTest("PID - Proportional control", test_PID_proportional);
    TestFramework::RunTest("PID - Output clamping", test_PID_clamping);
    TestFramework::RunTest("PID - Port names", test_PID_port_names);
}

#endif

