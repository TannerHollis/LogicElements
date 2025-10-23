#include "test_framework.hpp"

using namespace LogicElements;

#ifdef LE_ELEMENTS_ANALOG_COMPLEX

bool test_Polar2Complex_heterogeneous()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "MAG_IN", ElementType::NodeAnalog);
    TestFramework::CreateElement(&engine, "ANG_IN", ElementType::NodeAnalog);
    TestFramework::CreateElement(&engine, "P2C", ElementType::Polar2Complex);
    TestFramework::CreateElement(&engine, "COMPLEX_OUT", ElementType::NodeAnalogComplex);
    
    TestFramework::ConnectElements(&engine, "MAG_IN", "output", "P2C", "magnitude");
    TestFramework::ConnectElements(&engine, "ANG_IN", "output", "P2C", "angle");
    TestFramework::ConnectElements(&engine, "P2C", "complex", "COMPLEX_OUT", "input");
    
    NodeAnalog* mag_in = (NodeAnalog*)engine.GetElement("MAG_IN");
    NodeAnalog* ang_in = (NodeAnalog*)engine.GetElement("ANG_IN");
    NodeAnalogComplex* complex_out = (NodeAnalogComplex*)engine.GetElement("COMPLEX_OUT");
    
    Time t = Time::GetTime();
    
    // HETEROGENEOUS TEST: float inputs -> complex output
    mag_in->SetValue(5.0f);
    ang_in->SetValue(53.13f);
    engine.Update(t);
    
    std::complex<float> result = complex_out->GetOutput();
    ASSERT_NEAR(result.real(), 3.0f, 0.01f);
    ASSERT_NEAR(result.imag(), 4.0f, 0.01f);
    
    return true;
}

bool test_Polar2Complex_port_types()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "P2C", ElementType::Polar2Complex);
    Element* converter = engine.GetElement("P2C");
    
    // Verify HETEROGENEOUS port types!
    ASSERT_EQUAL(converter->GetInputPort("magnitude")->GetType(), PortType::ANALOG);
    ASSERT_EQUAL(converter->GetInputPort("angle")->GetType(), PortType::ANALOG);
    ASSERT_EQUAL(converter->GetOutputPort("complex")->GetType(), PortType::COMPLEX);
    
    return true;
}

void RunPolar2ComplexTests()
{
    std::cout << "\n=== Polar2Complex Tests ===" << std::endl;
    TestFramework::RunTest("Polar2Complex - Heterogeneous (float->complex)", test_Polar2Complex_heterogeneous);
    TestFramework::RunTest("Polar2Complex - Port type validation", test_Polar2Complex_port_types);
}

#endif

