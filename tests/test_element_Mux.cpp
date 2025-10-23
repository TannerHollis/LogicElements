#include "test_framework.hpp"

using namespace LogicElements;

bool test_Mux_Digital_basic()
{
    Engine engine("TestEngine");
    
    // Create 2x2 mux (2 signals, 2 input sets)
    TestFramework::CreateElement(&engine, "SIG0_0", ElementType::NodeDigital);
    TestFramework::CreateElement(&engine, "SIG0_1", ElementType::NodeDigital);
    TestFramework::CreateElement(&engine, "SIG1_0", ElementType::NodeDigital);
    TestFramework::CreateElement(&engine, "SIG1_1", ElementType::NodeDigital);
    TestFramework::CreateElement(&engine, "SEL", ElementType::NodeDigital);
    TestFramework::CreateElement(&engine, "MUX", ElementType::MuxDigital, 2, 2); // signalWidth=2, nInputs=2
    TestFramework::CreateElement(&engine, "OUT0", ElementType::NodeDigital);
    TestFramework::CreateElement(&engine, "OUT1", ElementType::NodeDigital);
    
    // Connect signals
    TestFramework::ConnectElements(&engine, "SIG0_0", LE_PORT_OUTPUT_PREFIX, "MUX", LE_PORT_INPUT_2D_NAME(0, 0));
    TestFramework::ConnectElements(&engine, "SIG0_1", LE_PORT_OUTPUT_PREFIX, "MUX", LE_PORT_INPUT_2D_NAME(0, 1));
    TestFramework::ConnectElements(&engine, "SIG1_0", LE_PORT_OUTPUT_PREFIX, "MUX", LE_PORT_INPUT_2D_NAME(1, 0));
    TestFramework::ConnectElements(&engine, "SIG1_1", LE_PORT_OUTPUT_PREFIX, "MUX", LE_PORT_INPUT_2D_NAME(1, 1));
    TestFramework::ConnectElements(&engine, "SEL", LE_PORT_OUTPUT_PREFIX, "MUX", LE_PORT_SELECTOR_NAME);
    TestFramework::ConnectElements(&engine, "MUX", LE_PORT_OUTPUT_NAME(0), "OUT0", LE_PORT_INPUT_PREFIX);
    TestFramework::ConnectElements(&engine, "MUX", LE_PORT_OUTPUT_NAME(1), "OUT1", LE_PORT_INPUT_PREFIX);
    
    NodeDigital* sig0_0 = (NodeDigital*)engine.GetElement("SIG0_0");
    NodeDigital* sig0_1 = (NodeDigital*)engine.GetElement("SIG0_1");
    NodeDigital* sig1_0 = (NodeDigital*)engine.GetElement("SIG1_0");
    NodeDigital* sig1_1 = (NodeDigital*)engine.GetElement("SIG1_1");
    NodeDigital* sel = (NodeDigital*)engine.GetElement("SEL");
    NodeDigital* out0 = (NodeDigital*)engine.GetElement("OUT0");
    NodeDigital* out1 = (NodeDigital*)engine.GetElement("OUT1");
    
    Time t = Time::GetTime();
    
    // Set input signals
    sig0_0->SetValue(true);  sig0_1->SetValue(false);
    sig1_0->SetValue(false); sig1_1->SetValue(true);
    
    // Select set 0
    sel->SetValue(false);
    engine.Update(t);
    std::cout << "  Selector=false: out0=" << out0->GetOutput() << " (expected: true), out1=" << out1->GetOutput() << " (expected: false)" << std::endl;
    ASSERT_TRUE(out0->GetOutput());   // sig0_0
    ASSERT_FALSE(out1->GetOutput());  // sig0_1
    
    // Select set 1 (HETEROGENEOUS: bool selector controls signals!)
    sel->SetValue(true);
    engine.Update(t);
    ASSERT_FALSE(out0->GetOutput());  // sig1_0
    ASSERT_TRUE(out1->GetOutput());   // sig1_1
    
    return true;
}

bool test_Mux_port_heterogeneous()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "MUX", ElementType::MuxDigital, 2, 2);
    Element* mux = engine.GetElement("MUX");
    
    // Verify signal input ports (DIGITAL type)
    auto port = mux->GetInputPort(LE_PORT_INPUT_2D_NAME(0, 0).c_str());
    std::cout << "  Port " << LE_PORT_INPUT_2D_NAME(0, 0) << ": " << (port ? "exists" : "NULL") << std::endl;
    ASSERT_TRUE(port != nullptr);
    ASSERT_EQUAL(port->GetType(), PortType::DIGITAL);
    
    // Verify selector port (also DIGITAL but semantically different!)
    ASSERT_TRUE(mux->GetInputPort(LE_PORT_SELECTOR_NAME) != nullptr);
    ASSERT_EQUAL(mux->GetInputPort(LE_PORT_SELECTOR_NAME)->GetType(), PortType::DIGITAL);
    
    // Verify output ports
    ASSERT_TRUE(mux->GetOutputPort(LE_PORT_OUTPUT_NAME(0).c_str()) != nullptr);
    ASSERT_TRUE(mux->GetOutputPort(LE_PORT_OUTPUT_NAME(1).c_str()) != nullptr);
    
    return true;
}

void RunMuxTests()
{
    std::cout << "\n=== le_Mux Tests ===" << std::endl;
    TestFramework::RunTest("ElementType::MuxDigital - Basic multiplexing", test_Mux_Digital_basic);
    TestFramework::RunTest("le_Mux - Heterogeneous ports (selector)", test_Mux_port_heterogeneous);
}

