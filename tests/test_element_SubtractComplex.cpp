#include "test_framework.hpp"

using namespace LogicElements;

#ifdef LE_ELEMENTS_ANALOG
#ifdef LE_ELEMENTS_ANALOG_COMPLEX

bool test_SubtractComplex_basic()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "IN0", ElementType::NodeAnalogComplex);
    TestFramework::CreateElement(&engine, "IN1", ElementType::NodeAnalogComplex);
    TestFramework::CreateElement(&engine, "SUB", ElementType::SubtractComplex);
    TestFramework::CreateElement(&engine, "OUT", ElementType::NodeAnalogComplex);
    
    TestFramework::ConnectElements(&engine, "IN0", "output", "SUB", "input_0");
    TestFramework::ConnectElements(&engine, "IN1", "output", "SUB", "input_1");
    TestFramework::ConnectElements(&engine, "SUB", "output", "OUT", "input");
    
    NodeAnalogComplex* in0 = (NodeAnalogComplex*)engine.GetElement("IN0");
    NodeAnalogComplex* in1 = (NodeAnalogComplex*)engine.GetElement("IN1");
    NodeAnalogComplex* out = (NodeAnalogComplex*)engine.GetElement("OUT");
    
    Time t = Time::GetTime();
    
    // Test: (5+7j) - (2+3j) = (3+4j)
    in0->SetValue(std::complex<float>(5.0f, 7.0f));
    in1->SetValue(std::complex<float>(2.0f, 3.0f));
    engine.Update(t);
    std::complex<float> result = out->GetOutput();
    ASSERT_NEAR(result.real(), 3.0f, 0.001f);
    ASSERT_NEAR(result.imag(), 4.0f, 0.001f);
    
    // Test: (1+1j) - (1+1j) = (0+0j)
    in0->SetValue(std::complex<float>(1.0f, 1.0f));
    in1->SetValue(std::complex<float>(1.0f, 1.0f));
    engine.Update(t);
    result = out->GetOutput();
    ASSERT_NEAR(result.real(), 0.0f, 0.001f);
    ASSERT_NEAR(result.imag(), 0.0f, 0.001f);
    
    return true;
}

bool test_SubtractComplex_port_names()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "SUB", ElementType::SubtractComplex);
    Element* sub = engine.GetElement("SUB");
    
    ASSERT_TRUE(sub->GetInputPort("input_0") != nullptr);
    ASSERT_TRUE(sub->GetInputPort("input_1") != nullptr);
    ASSERT_TRUE(sub->GetOutputPort("output") != nullptr);
    ASSERT_EQUAL(sub->GetInputPort("input_0")->GetType(), PortType::COMPLEX);
    
    return true;
}

void RunSubtractComplexTests()
{
    std::cout << "\n=== SubtractComplex Tests ===" << std::endl;
    TestFramework::RunTest("SubtractComplex - Basic subtraction", test_SubtractComplex_basic);
    TestFramework::RunTest("SubtractComplex - Port names", test_SubtractComplex_port_names);
}

#endif
#endif

