#include "test_framework.hpp"

using namespace LogicElements;

#ifdef LE_ELEMENTS_ANALOG_COMPLEX

bool test_Rect2Complex_heterogeneous()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "REAL_IN", ElementType::NodeAnalog);
    TestFramework::CreateElement(&engine, "IMAG_IN", ElementType::NodeAnalog);
    TestFramework::CreateElement(&engine, "R2C", ElementType::Rect2Complex);
    TestFramework::CreateElement(&engine, "COMPLEX_OUT", ElementType::NodeAnalogComplex);
    
    TestFramework::ConnectElements(&engine, "REAL_IN", "output", "R2C", "real");
    TestFramework::ConnectElements(&engine, "IMAG_IN", "output", "R2C", "imaginary");
    TestFramework::ConnectElements(&engine, "R2C", "complex", "COMPLEX_OUT", "input");
    
    NodeAnalog* real_in = (NodeAnalog*)engine.GetElement("REAL_IN");
    NodeAnalog* imag_in = (NodeAnalog*)engine.GetElement("IMAG_IN");
    NodeAnalogComplex* complex_out = (NodeAnalogComplex*)engine.GetElement("COMPLEX_OUT");
    
    Time t = Time::GetTime();
    
    // HETEROGENEOUS TEST: float inputs -> complex output
    real_in->SetValue(3.0f);
    imag_in->SetValue(4.0f);
    engine.Update(t);
    
    std::complex<float> result = complex_out->GetOutput();
    ASSERT_NEAR(result.real(), 3.0f, 0.001f);
    ASSERT_NEAR(result.imag(), 4.0f, 0.001f);
    
    return true;
}

bool test_Rect2Complex_port_types()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "R2C", ElementType::Rect2Complex);
    Element* converter = engine.GetElement("R2C");
    
    // Verify HETEROGENEOUS port types!
    ASSERT_EQUAL(converter->GetInputPort("real")->GetType(), PortType::ANALOG);
    ASSERT_EQUAL(converter->GetInputPort("imaginary")->GetType(), PortType::ANALOG);
    ASSERT_EQUAL(converter->GetOutputPort("complex")->GetType(), PortType::COMPLEX);
    
    return true;
}

void RunRect2ComplexTests()
{
    std::cout << "\n=== Rect2Complex Tests ===" << std::endl;
    TestFramework::RunTest("Rect2Complex - Heterogeneous (float->complex)", test_Rect2Complex_heterogeneous);
    TestFramework::RunTest("Rect2Complex - Port type validation", test_Rect2Complex_port_types);
}

#endif

