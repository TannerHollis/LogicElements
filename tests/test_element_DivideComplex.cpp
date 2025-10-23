#include "test_framework.hpp"

using namespace LogicElements;

#ifdef LE_ELEMENTS_ANALOG
#ifdef LE_ELEMENTS_ANALOG_COMPLEX

bool test_DivideComplex_basic()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "IN0", ElementType::NodeAnalogComplex);
    TestFramework::CreateElement(&engine, "IN1", ElementType::NodeAnalogComplex);
    TestFramework::CreateElement(&engine, "DIV", ElementType::DivideComplex);
    TestFramework::CreateElement(&engine, "OUT", ElementType::NodeAnalogComplex);
    
    TestFramework::ConnectElements(&engine, "IN0", LE_PORT_OUTPUT_PREFIX, "DIV", LE_PORT_INPUT_NAME(0));
    TestFramework::ConnectElements(&engine, "IN1", LE_PORT_OUTPUT_PREFIX, "DIV", LE_PORT_INPUT_NAME(1));
    TestFramework::ConnectElements(&engine, "DIV", LE_PORT_OUTPUT_PREFIX, "OUT", LE_PORT_INPUT_PREFIX);
    
    NodeAnalogComplex* in0 = (NodeAnalogComplex*)engine.GetElement("IN0");
    NodeAnalogComplex* in1 = (NodeAnalogComplex*)engine.GetElement("IN1");
    NodeAnalogComplex* out = (NodeAnalogComplex*)engine.GetElement("OUT");
    
    Time t = Time::GetTime();
    
    // Test: (10+0j) / (2+0j) = (5+0j)
    in0->SetValue(std::complex<float>(10.0f, 0.0f));
    in1->SetValue(std::complex<float>(2.0f, 0.0f));
    engine.Update(t);
    std::complex<float> result = out->GetOutput();
    ASSERT_NEAR(result.real(), 5.0f, 0.001f);
    ASSERT_NEAR(result.imag(), 0.0f, 0.001f);
    
    // Test: Divide by zero protection (returns 0+0j)
    in0->SetValue(std::complex<float>(10.0f, 10.0f));
    in1->SetValue(std::complex<float>(0.0f, 0.0f));
    engine.Update(t);
    result = out->GetOutput();
    ASSERT_NEAR(result.real(), 0.0f, 0.001f);
    ASSERT_NEAR(result.imag(), 0.0f, 0.001f);
    
    return true;
}

bool test_DivideComplex_port_names()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "DIV", ElementType::DivideComplex);
    Element* div = engine.GetElement("DIV");
    
    ASSERT_TRUE(div->GetInputPort(LE_PORT_INPUT_NAME(0).c_str()) != nullptr);
    ASSERT_TRUE(div->GetInputPort(LE_PORT_INPUT_NAME(1).c_str()) != nullptr);
    ASSERT_TRUE(div->GetOutputPort(LE_PORT_OUTPUT_PREFIX) != nullptr);
    ASSERT_EQUAL(div->GetInputPort(LE_PORT_INPUT_NAME(0).c_str())->GetType(), PortType::COMPLEX);
    
    return true;
}

void RunDivideComplexTests()
{
    std::cout << "\n=== DivideComplex Tests ===" << std::endl;
    TestFramework::RunTest("DivideComplex - Basic division", test_DivideComplex_basic);
    TestFramework::RunTest("DivideComplex - Port names", test_DivideComplex_port_names);
}

#endif
#endif

