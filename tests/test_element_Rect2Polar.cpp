#include "test_framework.hpp"

using namespace LogicElements;

#ifdef LE_ELEMENTS_ANALOG

bool test_Rect2Polar_basic()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "REAL", ElementType::NodeAnalog);
    TestFramework::CreateElement(&engine, "IMAG", ElementType::NodeAnalog);
    TestFramework::CreateElement(&engine, "R2P", ElementType::Rect2Polar);
    TestFramework::CreateElement(&engine, "MAG", ElementType::NodeAnalog);
    TestFramework::CreateElement(&engine, "ANG", ElementType::NodeAnalog);
    
    TestFramework::ConnectElements(&engine, "REAL", LE_PORT_OUTPUT_PREFIX, "R2P", "real");
    TestFramework::ConnectElements(&engine, "IMAG", LE_PORT_OUTPUT_PREFIX, "R2P", "imaginary");
    TestFramework::ConnectElements(&engine, "R2P", "magnitude", "MAG", LE_PORT_INPUT_PREFIX);
    TestFramework::ConnectElements(&engine, "R2P", "angle", "ANG", LE_PORT_INPUT_PREFIX);
    
    NodeAnalog* real = (NodeAnalog*)engine.GetElement("REAL");
    NodeAnalog* imag = (NodeAnalog*)engine.GetElement("IMAG");
    NodeAnalog* mag = (NodeAnalog*)engine.GetElement("MAG");
    NodeAnalog* ang = (NodeAnalog*)engine.GetElement("ANG");
    
    Time t = Time::GetTime();
    
    // Test: 3 + 4j -> magnitude=5, angle=53.13°
    real->SetValue(3.0f);
    imag->SetValue(4.0f);
    engine.Update(t);
    
    ASSERT_NEAR(mag->GetOutput(), 5.0f, 0.01f);
    ASSERT_NEAR(ang->GetOutput(), 53.13f, 0.2f);
    
    // Test: 1 + 0j -> magnitude=1, angle=0°
    real->SetValue(1.0f);
    imag->SetValue(0.0f);
    engine.Update(t);
    
    ASSERT_NEAR(mag->GetOutput(), 1.0f, 0.01f);
    ASSERT_NEAR(ang->GetOutput(), 0.0f, 0.1f);
    
    return true;
}

bool test_Rect2Polar_port_names()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "R2P", ElementType::Rect2Polar);
    Element* converter = engine.GetElement("R2P");
    
    // Verify port names
    ASSERT_TRUE(converter->GetInputPort("real") != nullptr);
    ASSERT_TRUE(converter->GetInputPort("imaginary") != nullptr);
    ASSERT_TRUE(converter->GetOutputPort("magnitude") != nullptr);
    ASSERT_TRUE(converter->GetOutputPort("angle") != nullptr);
    
    // Verify port types (all ANALOG)
    ASSERT_EQUAL(converter->GetInputPort("real")->GetType(), PortType::ANALOG);
    ASSERT_EQUAL(converter->GetOutputPort("magnitude")->GetType(), PortType::ANALOG);
    
    return true;
}

void RunRect2PolarTests()
{
    std::cout << "\n=== Rect2Polar Tests ===" << std::endl;
    TestFramework::RunTest("Rect2Polar - Basic conversion", test_Rect2Polar_basic);
    TestFramework::RunTest("Rect2Polar - Port names", test_Rect2Polar_port_names);
}

#endif

