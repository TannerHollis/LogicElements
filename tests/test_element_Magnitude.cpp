#include "test_framework.hpp"

using namespace LogicElements;

#ifdef LE_ELEMENTS_ANALOG
#ifdef LE_ELEMENTS_ANALOG_COMPLEX

bool test_Magnitude_basic()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "IN", ElementType::NodeAnalogComplex);
    TestFramework::CreateElement(&engine, "MAG", ElementType::Magnitude);
    TestFramework::CreateElement(&engine, "OUT", ElementType::NodeAnalog);
    
    TestFramework::ConnectElements(&engine, "IN", LE_PORT_OUTPUT_PREFIX, "MAG", LE_PORT_INPUT_PREFIX);
    TestFramework::ConnectElements(&engine, "MAG", LE_PORT_OUTPUT_PREFIX, "OUT", LE_PORT_INPUT_PREFIX);
    
    NodeAnalogComplex* in = (NodeAnalogComplex*)engine.GetElement("IN");
    NodeAnalog* out = (NodeAnalog*)engine.GetElement("OUT");
    
    Time t = Time::GetTime();
    
    // Test: |3+4j| = 5.0
    in->SetValue(std::complex<float>(3.0f, 4.0f));
    engine.Update(t);
    ASSERT_NEAR(out->GetOutput(), 5.0f, 0.001f);
    
    // Test: |1+0j| = 1.0
    in->SetValue(std::complex<float>(1.0f, 0.0f));
    engine.Update(t);
    ASSERT_NEAR(out->GetOutput(), 1.0f, 0.001f);
    
    // Test: |0+0j| = 0.0
    in->SetValue(std::complex<float>(0.0f, 0.0f));
    engine.Update(t);
    ASSERT_NEAR(out->GetOutput(), 0.0f, 0.001f);
    
    return true;
}

bool test_Magnitude_heterogeneous()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "MAG", ElementType::Magnitude);
    Element* mag = engine.GetElement("MAG");
    
    // Verify HETEROGENEOUS port types!
    ASSERT_EQUAL(mag->GetInputPort("input")->GetType(), PortType::COMPLEX);  // complex input
    ASSERT_EQUAL(mag->GetOutputPort(LE_PORT_OUTPUT_PREFIX)->GetType(), PortType::ANALOG); // float output
    
    // This demonstrates mixing COMPLEX and ANALOG on same element!
    return true;
}

void RunMagnitudeTests()
{
    std::cout << "\n=== Magnitude Tests ===" << std::endl;
    TestFramework::RunTest("Magnitude - Basic calculation", test_Magnitude_basic);
    TestFramework::RunTest("Magnitude - Heterogeneous ports", test_Magnitude_heterogeneous);
}

#endif
#endif

