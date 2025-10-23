#include "test_framework.hpp"

using namespace LogicElements;

#ifdef LE_ELEMENTS_ANALOG
#ifdef LE_ELEMENTS_ANALOG_COMPLEX

bool test_MultiplyComplex_basic()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "IN0", ElementType::NodeAnalogComplex);
    TestFramework::CreateElement(&engine, "IN1", ElementType::NodeAnalogComplex);
    TestFramework::CreateElement(&engine, "MUL", ElementType::MultiplyComplex);
    TestFramework::CreateElement(&engine, "OUT", ElementType::NodeAnalogComplex);
    
    TestFramework::ConnectElements(&engine, "IN0", LE_PORT_OUTPUT_PREFIX, "MUL", LE_PORT_INPUT_NAME(0));
    TestFramework::ConnectElements(&engine, "IN1", LE_PORT_OUTPUT_PREFIX, "MUL", LE_PORT_INPUT_NAME(1));
    TestFramework::ConnectElements(&engine, "MUL", LE_PORT_OUTPUT_PREFIX, "OUT", LE_PORT_INPUT_PREFIX);
    
    NodeAnalogComplex* in0 = (NodeAnalogComplex*)engine.GetElement("IN0");
    NodeAnalogComplex* in1 = (NodeAnalogComplex*)engine.GetElement("IN1");
    NodeAnalogComplex* out = (NodeAnalogComplex*)engine.GetElement("OUT");
    
    Time t = Time::GetTime();
    
    // Test: (2+0j) * (3+0j) = (6+0j)
    in0->SetValue(std::complex<float>(2.0f, 0.0f));
    in1->SetValue(std::complex<float>(3.0f, 0.0f));
    engine.Update(t);
    std::complex<float> result = out->GetOutput();
    ASSERT_NEAR(result.real(), 6.0f, 0.001f);
    ASSERT_NEAR(result.imag(), 0.0f, 0.001f);
    
    // Test: (1+1j) * (1-1j) = (2+0j) (complex conjugate multiplication)
    in0->SetValue(std::complex<float>(1.0f, 1.0f));
    in1->SetValue(std::complex<float>(1.0f, -1.0f));
    engine.Update(t);
    result = out->GetOutput();
    ASSERT_NEAR(result.real(), 2.0f, 0.001f);
    ASSERT_NEAR(result.imag(), 0.0f, 0.001f);
    
    return true;
}

bool test_MultiplyComplex_port_names()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "MUL", ElementType::MultiplyComplex);
    Element* mul = engine.GetElement("MUL");
    
    ASSERT_TRUE(mul->GetInputPort(LE_PORT_INPUT_NAME(0).c_str()) != nullptr);
    ASSERT_TRUE(mul->GetInputPort(LE_PORT_INPUT_NAME(1).c_str()) != nullptr);
    ASSERT_TRUE(mul->GetOutputPort(LE_PORT_OUTPUT_PREFIX) != nullptr);
    ASSERT_EQUAL(mul->GetInputPort(LE_PORT_INPUT_NAME(0).c_str())->GetType(), PortType::COMPLEX);
    
    return true;
}

void RunMultiplyComplexTests()
{
    std::cout << "\n=== MultiplyComplex Tests ===" << std::endl;
    TestFramework::RunTest("MultiplyComplex - Basic multiplication", test_MultiplyComplex_basic);
    TestFramework::RunTest("MultiplyComplex - Port names", test_MultiplyComplex_port_names);
}

#endif
#endif

