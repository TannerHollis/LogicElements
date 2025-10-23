#include "test_framework.hpp"

using namespace LogicElements;

#ifdef LE_ELEMENTS_ANALOG
#ifdef LE_ELEMENTS_ANALOG_COMPLEX

bool test_NegateComplex_basic()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "IN", ElementType::NodeAnalogComplex);
    TestFramework::CreateElement(&engine, "NEG", ElementType::NegateComplex);
    TestFramework::CreateElement(&engine, "OUT", ElementType::NodeAnalogComplex);
    
    TestFramework::ConnectElements(&engine, "IN", LE_PORT_OUTPUT_PREFIX, "NEG", LE_PORT_INPUT_PREFIX);
    TestFramework::ConnectElements(&engine, "NEG", LE_PORT_OUTPUT_PREFIX, "OUT", LE_PORT_INPUT_PREFIX);
    
    NodeAnalogComplex* in = (NodeAnalogComplex*)engine.GetElement("IN");
    NodeAnalogComplex* out = (NodeAnalogComplex*)engine.GetElement("OUT");
    
    Time t = Time::GetTime();
    
    // Test: -(3+4j) = (-3-4j)
    in->SetValue(std::complex<float>(3.0f, 4.0f));
    engine.Update(t);
    std::complex<float> result = out->GetOutput();
    ASSERT_NEAR(result.real(), -3.0f, 0.001f);
    ASSERT_NEAR(result.imag(), -4.0f, 0.001f);
    
    // Test: -(0+0j) = (0+0j)
    in->SetValue(std::complex<float>(0.0f, 0.0f));
    engine.Update(t);
    result = out->GetOutput();
    ASSERT_NEAR(result.real(), 0.0f, 0.001f);
    ASSERT_NEAR(result.imag(), 0.0f, 0.001f);
    
    return true;
}

bool test_NegateComplex_port_names()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "NEG", ElementType::NegateComplex);
    Element* neg = engine.GetElement("NEG");
    
    ASSERT_TRUE(neg->GetInputPort("input") != nullptr);
    ASSERT_TRUE(neg->GetOutputPort(LE_PORT_OUTPUT_PREFIX) != nullptr);
    ASSERT_EQUAL(neg->GetInputPort("input")->GetType(), PortType::COMPLEX);
    
    return true;
}

void RunNegateComplexTests()
{
    std::cout << "\n=== NegateComplex Tests ===" << std::endl;
    TestFramework::RunTest("NegateComplex - Basic negation", test_NegateComplex_basic);
    TestFramework::RunTest("NegateComplex - Port names", test_NegateComplex_port_names);
}

#endif
#endif

