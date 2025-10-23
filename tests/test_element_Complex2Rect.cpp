#include "test_framework.hpp"

using namespace LogicElements;

#ifdef LE_ELEMENTS_ANALOG_COMPLEX

bool test_Complex2Rect_heterogeneous()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "COMPLEX_IN", ElementType::NodeAnalogComplex);
    TestFramework::CreateElement(&engine, "C2R", ElementType::Complex2Rect);
    TestFramework::CreateElement(&engine, "REAL_OUT", ElementType::NodeAnalog);
    TestFramework::CreateElement(&engine, "IMAG_OUT", ElementType::NodeAnalog);
    
    TestFramework::ConnectElements(&engine, "COMPLEX_IN", LE_PORT_OUTPUT_PREFIX, "C2R", "complex");
    TestFramework::ConnectElements(&engine, "C2R", "real", "REAL_OUT", LE_PORT_INPUT_PREFIX);
    TestFramework::ConnectElements(&engine, "C2R", "imaginary", "IMAG_OUT", LE_PORT_INPUT_PREFIX);
    
    NodeAnalogComplex* complex_in = (NodeAnalogComplex*)engine.GetElement("COMPLEX_IN");
    NodeAnalog* real_out = (NodeAnalog*)engine.GetElement("REAL_OUT");
    NodeAnalog* imag_out = (NodeAnalog*)engine.GetElement("IMAG_OUT");
    
    Time t = Time::GetTime();
    
    // HETEROGENEOUS TEST: complex input -> float outputs
    complex_in->SetValue(std::complex<float>(3.0f, 4.0f));
    engine.Update(t);
    
    ASSERT_NEAR(real_out->GetOutput(), 3.0f, 0.001f);
    ASSERT_NEAR(imag_out->GetOutput(), 4.0f, 0.001f);
    
    return true;
}

bool test_Complex2Rect_port_types()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "C2R", ElementType::Complex2Rect);
    Element* converter = engine.GetElement("C2R");
    
    // Verify HETEROGENEOUS port types!
    ASSERT_EQUAL(converter->GetInputPort("complex")->GetType(), PortType::COMPLEX);
    ASSERT_EQUAL(converter->GetOutputPort("real")->GetType(), PortType::ANALOG);
    ASSERT_EQUAL(converter->GetOutputPort("imaginary")->GetType(), PortType::ANALOG);
    
    // This element mixes COMPLEX and ANALOG types!
    return true;
}

void RunComplex2RectTests()
{
    std::cout << "\n=== Complex2Rect Tests ===" << std::endl;
    TestFramework::RunTest("Complex2Rect - Heterogeneous (complex->float)", test_Complex2Rect_heterogeneous);
    TestFramework::RunTest("Complex2Rect - Port type validation", test_Complex2Rect_port_types);
}

#endif

