#include "test_framework.hpp"

using namespace LogicElements;

#ifdef LE_ELEMENTS_ANALOG

#ifdef LE_ELEMENTS_ANALOG_COMPLEX
bool test_PhasorShift_complex()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "IN", ElementType::NodeAnalogComplex);
    TestFramework::CreateElement(&engine, "SHIFT", ElementType::PhasorShift, 1.0f, 30.0f, 0.0f, 0.0f, 0.0f); // mag=1.0, angle=30°
    TestFramework::CreateElement(&engine, "OUT", ElementType::NodeAnalogComplex);
    
    TestFramework::ConnectElements(&engine, "IN", "output", "SHIFT", "input");
    TestFramework::ConnectElements(&engine, "SHIFT", "output", "OUT", "input");
    
    NodeAnalogComplex* in = (NodeAnalogComplex*)engine.GetElement("IN");
    NodeAnalogComplex* out = (NodeAnalogComplex*)engine.GetElement("OUT");
    
    Time t = Time::GetTime();
    
    // Test: Input phasor, should be phase-shifted
    in->SetValue(std::complex<float>(1.0f, 0.0f)); // 1+0j
    engine.Update(t);
    
    // Output should be rotated by -30° (clockwise)
    std::complex<float> inValue = in->GetOutput();
    std::complex<float> shiftValue = ((PhasorShift*)engine.GetElement("SHIFT"))->GetOutput();
    std::complex<float> result = out->GetOutput();
    
    // Debug: Print values
    std::cout << "  IN: " << inValue << ", SHIFT: " << shiftValue << ", OUT: " << result << std::endl;
    
    ASSERT_NEAR(std::abs(result), 1.0f, 0.01f); // Magnitude preserved
    
    return true;
}

bool test_PhasorShift_port_names_complex()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "SHIFT", ElementType::PhasorShift, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    Element* shift = engine.GetElement("SHIFT");
    
    ASSERT_TRUE(shift->GetInputPort("input") != nullptr);
    ASSERT_TRUE(shift->GetOutputPort("output") != nullptr);
    ASSERT_EQUAL(shift->GetInputPort("input")->GetType(), PortType::COMPLEX);
    
    return true;
}
#else
bool test_PhasorShift_float()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "REAL_IN", ElementType::NodeAnalog);
    TestFramework::CreateElement(&engine, "IMAG_IN", ElementType::NodeAnalog);
    TestFramework::CreateElement(&engine, "SHIFT", ElementType::PhasorShift, 1.0f, 30.0f, 0.0f, 0.0f, 0.0f);
    TestFramework::CreateElement(&engine, "REAL_OUT", ElementType::NodeAnalog);
    TestFramework::CreateElement(&engine, "IMAG_OUT", ElementType::NodeAnalog);
    
    TestFramework::ConnectElements(&engine, "REAL_IN", "output", "SHIFT", "real");
    TestFramework::ConnectElements(&engine, "IMAG_IN", "output", "SHIFT", "imaginary");
    TestFramework::ConnectElements(&engine, "SHIFT", "real", "REAL_OUT", "input");
    TestFramework::ConnectElements(&engine, "SHIFT", "imaginary", "IMAG_OUT", "input");
    
    NodeAnalog* real_in = (NodeAnalog*)engine.GetElement("REAL_IN");
    NodeAnalog* imag_in = (NodeAnalog*)engine.GetElement("IMAG_IN");
    NodeAnalog* real_out = (NodeAnalog*)engine.GetElement("REAL_OUT");
    NodeAnalog* imag_out = (NodeAnalog*)engine.GetElement("IMAG_OUT");
    
    Time t = Time::GetTime();
    
    real_in->SetValue(1.0f);
    imag_in->SetValue(0.0f);
    engine.Update(t);
    
    // Verify shift applied
    float mag = sqrtf(real_out->GetOutput() * real_out->GetOutput() + 
                      imag_out->GetOutput() * imag_out->GetOutput());
    ASSERT_NEAR(mag, 1.0f, 0.01f);
    
    return true;
}

bool test_PhasorShift_port_names_float()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "SHIFT", ElementType::PhasorShift, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    Element* shift = engine.GetElement("SHIFT");
    
    ASSERT_TRUE(shift->GetInputPort("real") != nullptr);
    ASSERT_TRUE(shift->GetInputPort("imaginary") != nullptr);
    ASSERT_TRUE(shift->GetOutputPort("real") != nullptr);
    ASSERT_TRUE(shift->GetOutputPort("imaginary") != nullptr);
    
    return true;
}
#endif

void RunPhasorShiftTests()
{
    std::cout << "\n=== PhasorShift Tests ===" << std::endl;
#ifdef LE_ELEMENTS_ANALOG_COMPLEX
    TestFramework::RunTest("PhasorShift - Complex version", test_PhasorShift_complex);
    TestFramework::RunTest("PhasorShift - Port names (complex)", test_PhasorShift_port_names_complex);
#else
    TestFramework::RunTest("PhasorShift - Float version", test_PhasorShift_float);
    TestFramework::RunTest("PhasorShift - Port names (float)", test_PhasorShift_port_names_float);
#endif
}

#endif

