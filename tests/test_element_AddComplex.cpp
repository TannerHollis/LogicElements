#include "test_framework.hpp"

using namespace LogicElements;

#ifdef LE_ELEMENTS_ANALOG
#ifdef LE_ELEMENTS_ANALOG_COMPLEX

bool test_AddComplex_basic()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "IN0", ElementType::NodeAnalogComplex);
    TestFramework::CreateElement(&engine, "IN1", ElementType::NodeAnalogComplex);
    TestFramework::CreateElement(&engine, "ADD", ElementType::AddComplex);
    TestFramework::CreateElement(&engine, "OUT", ElementType::NodeAnalogComplex);
    
    TestFramework::ConnectElements(&engine, "IN0", LE_PORT_OUTPUT_PREFIX, "ADD", LE_PORT_INPUT_NAME(0));
    TestFramework::ConnectElements(&engine, "IN1", LE_PORT_OUTPUT_PREFIX, "ADD", LE_PORT_INPUT_NAME(1));
    TestFramework::ConnectElements(&engine, "ADD", LE_PORT_OUTPUT_PREFIX, "OUT", LE_PORT_INPUT_PREFIX);
    
    NodeAnalogComplex* in0 = (NodeAnalogComplex*)engine.GetElement("IN0");
    NodeAnalogComplex* in1 = (NodeAnalogComplex*)engine.GetElement("IN1");
    NodeAnalogComplex* out = (NodeAnalogComplex*)engine.GetElement("OUT");
    
    Time t = Time::GetTime();
    
    // Test: (1+2j) + (3+4j) = (4+6j)
    in0->SetValue(std::complex<float>(1.0f, 2.0f));
    in1->SetValue(std::complex<float>(3.0f, 4.0f));
    engine.Update(t);
    std::complex<float> result = out->GetOutput();
    ASSERT_NEAR(result.real(), 4.0f, 0.001f);
    ASSERT_NEAR(result.imag(), 6.0f, 0.001f);
    
    // Test: (5+0j) + (0+3j) = (5+3j)
    in0->SetValue(std::complex<float>(5.0f, 0.0f));
    in1->SetValue(std::complex<float>(0.0f, 3.0f));
    engine.Update(t);
    result = out->GetOutput();
    ASSERT_NEAR(result.real(), 5.0f, 0.001f);
    ASSERT_NEAR(result.imag(), 3.0f, 0.001f);
    
    return true;
}

bool test_AddComplex_port_names()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "ADD", ElementType::AddComplex);
    Element* add = engine.GetElement("ADD");
    
    ASSERT_TRUE(add->GetInputPort(LE_PORT_INPUT_NAME(0).c_str()) != nullptr);
    ASSERT_TRUE(add->GetInputPort(LE_PORT_INPUT_NAME(1).c_str()) != nullptr);
    ASSERT_TRUE(add->GetOutputPort(LE_PORT_OUTPUT_PREFIX) != nullptr);
    ASSERT_EQUAL(add->GetInputPort(LE_PORT_INPUT_NAME(0).c_str())->GetType(), PortType::COMPLEX);
    
    return true;
}

void RunAddComplexTests()
{
    std::cout << "\n=== AddComplex Tests ===" << std::endl;
    TestFramework::RunTest("AddComplex - Basic addition", test_AddComplex_basic);
    TestFramework::RunTest("AddComplex - Port names", test_AddComplex_port_names);
}

#endif
#endif

