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
    TestFramework::ConnectElements(&engine, "SIG0_0", "output", "MUX", "input_0_0");
    TestFramework::ConnectElements(&engine, "SIG0_1", "output", "MUX", "input_0_1");
    TestFramework::ConnectElements(&engine, "SIG1_0", "output", "MUX", "input_1_0");
    TestFramework::ConnectElements(&engine, "SIG1_1", "output", "MUX", "input_1_1");
    TestFramework::ConnectElements(&engine, "SEL", "output", "MUX", "selector");
    TestFramework::ConnectElements(&engine, "MUX", "output_0", "OUT0", "input");
    TestFramework::ConnectElements(&engine, "MUX", "output_1", "OUT1", "input");
    
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
    auto port = mux->GetInputPort("input_0_0");
    std::cout << "  Port input_0_0: " << (port ? "exists" : "NULL") << std::endl;
    ASSERT_TRUE(port != nullptr);
    ASSERT_EQUAL(port->GetType(), PortType::DIGITAL);
    
    // Verify selector port (also DIGITAL but semantically different!)
    ASSERT_TRUE(mux->GetInputPort("selector") != nullptr);
    ASSERT_EQUAL(mux->GetInputPort("selector")->GetType(), PortType::DIGITAL);
    
    // Verify output ports
    ASSERT_TRUE(mux->GetOutputPort("output_0") != nullptr);
    ASSERT_TRUE(mux->GetOutputPort("output_1") != nullptr);
    
    return true;
}

void RunMuxTests()
{
    std::cout << "\n=== le_Mux Tests ===" << std::endl;
    TestFramework::RunTest("ElementType::MuxDigital - Basic multiplexing", test_Mux_Digital_basic);
    TestFramework::RunTest("le_Mux - Heterogeneous ports (selector)", test_Mux_port_heterogeneous);
}

