#include "test_framework.hpp"

using namespace LogicElements;

#ifdef LE_ELEMENTS_ANALOG

bool test_Polar2Rect_basic()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "MAG", ElementType::NodeAnalog);
    TestFramework::CreateElement(&engine, "ANG", ElementType::NodeAnalog);
    TestFramework::CreateElement(&engine, "P2R", ElementType::Polar2Rect);
    TestFramework::CreateElement(&engine, "REAL", ElementType::NodeAnalog);
    TestFramework::CreateElement(&engine, "IMAG", ElementType::NodeAnalog);
    
    TestFramework::ConnectElements(&engine, "MAG", "output", "P2R", "magnitude");
    TestFramework::ConnectElements(&engine, "ANG", "output", "P2R", "angle");
    TestFramework::ConnectElements(&engine, "P2R", "real", "REAL", "input");
    TestFramework::ConnectElements(&engine, "P2R", "imaginary", "IMAG", "input");
    
    NodeAnalog* mag = (NodeAnalog*)engine.GetElement("MAG");
    NodeAnalog* ang = (NodeAnalog*)engine.GetElement("ANG");
    NodeAnalog* real = (NodeAnalog*)engine.GetElement("REAL");
    NodeAnalog* imag = (NodeAnalog*)engine.GetElement("IMAG");
    
    Time t = Time::GetTime();
    
    // Test: magnitude=5, angle=53.13° -> 3 + 4j
    mag->SetValue(5.0f);
    ang->SetValue(53.13f);
    engine.Update(t);
    
    ASSERT_NEAR(real->GetOutput(), 3.0f, 0.01f);
    ASSERT_NEAR(imag->GetOutput(), 4.0f, 0.01f);
    
    return true;
}

bool test_Polar2Rect_port_names()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "P2R", ElementType::Polar2Rect);
    Element* converter = engine.GetElement("P2R");
    
    ASSERT_TRUE(converter->GetInputPort("magnitude") != nullptr);
    ASSERT_TRUE(converter->GetInputPort("angle") != nullptr);
    ASSERT_TRUE(converter->GetOutputPort("real") != nullptr);
    ASSERT_TRUE(converter->GetOutputPort("imaginary") != nullptr);
    
    return true;
}

void RunPolar2RectTests()
{
    std::cout << "\n=== Polar2Rect Tests ===" << std::endl;
    TestFramework::RunTest("Polar2Rect - Basic conversion", test_Polar2Rect_basic);
    TestFramework::RunTest("Polar2Rect - Port names", test_Polar2Rect_port_names);
}

#endif

