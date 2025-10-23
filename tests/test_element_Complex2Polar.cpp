#include "test_framework.hpp"

using namespace LogicElements;

#ifdef LE_ELEMENTS_ANALOG_COMPLEX

bool test_Complex2Polar_heterogeneous()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "COMPLEX_IN", ElementType::NodeAnalogComplex);
    TestFramework::CreateElement(&engine, "C2P", ElementType::Complex2Polar);
    TestFramework::CreateElement(&engine, "MAG_OUT", ElementType::NodeAnalog);
    TestFramework::CreateElement(&engine, "ANG_OUT", ElementType::NodeAnalog);
    
    TestFramework::ConnectElements(&engine, "COMPLEX_IN", "output", "C2P", "complex");
    TestFramework::ConnectElements(&engine, "C2P", "magnitude", "MAG_OUT", "input");
    TestFramework::ConnectElements(&engine, "C2P", "angle", "ANG_OUT", "input");
    
    NodeAnalogComplex* complex_in = (NodeAnalogComplex*)engine.GetElement("COMPLEX_IN");
    NodeAnalog* mag_out = (NodeAnalog*)engine.GetElement("MAG_OUT");
    NodeAnalog* ang_out = (NodeAnalog*)engine.GetElement("ANG_OUT");
    
    Time t = Time::GetTime();
    
    // HETEROGENEOUS TEST: complex input -> float outputs
    complex_in->SetValue(std::complex<float>(3.0f, 4.0f));
    engine.Update(t);
    
    ASSERT_NEAR(mag_out->GetOutput(), 5.0f, 0.01f);
    ASSERT_NEAR(ang_out->GetOutput(), 53.13f, 0.2f);
    
    return true;
}

bool test_Complex2Polar_port_types()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "C2P", ElementType::Complex2Polar);
    Element* converter = engine.GetElement("C2P");
    
    // Verify HETEROGENEOUS port types!
    ASSERT_EQUAL(converter->GetInputPort("complex")->GetType(), PortType::COMPLEX);
    ASSERT_EQUAL(converter->GetOutputPort("magnitude")->GetType(), PortType::ANALOG);
    ASSERT_EQUAL(converter->GetOutputPort("angle")->GetType(), PortType::ANALOG);
    
    return true;
}

void RunComplex2PolarTests()
{
    std::cout << "\n=== Complex2Polar Tests ===" << std::endl;
    TestFramework::RunTest("Complex2Polar - Heterogeneous (complex->float)", test_Complex2Polar_heterogeneous);
    TestFramework::RunTest("Complex2Polar - Port type validation", test_Complex2Polar_port_types);
}

#endif

