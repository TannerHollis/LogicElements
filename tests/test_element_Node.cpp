#include "test_framework.hpp"

using namespace LogicElements;

bool test_Node_Digital_basic()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "IN", ElementType::NodeDigital);
    TestFramework::CreateElement(&engine, "OUT", ElementType::NodeDigital);
    
    TestFramework::ConnectElements(&engine, "IN", "output", "OUT", "input");
    
    NodeDigital* in = (NodeDigital*)engine.GetElement("IN");
    NodeDigital* out = (NodeDigital*)engine.GetElement("OUT");
    
    Time t = Time::GetTime();
    
    in->SetValue(false);
    engine.Update(t);
    ASSERT_FALSE(out->GetOutput());
    
    in->SetValue(true);
    engine.Update(t);
    ASSERT_TRUE(out->GetOutput());
    
    return true;
}

bool test_Node_Digital_override()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "NODE", ElementType::NodeDigital);
    NodeDigital* node = (NodeDigital*)engine.GetElement("NODE");
    
    Time t = Time::GetTime();
    
    node->SetValue(false);
    engine.Update(t);
    ASSERT_FALSE(node->GetOutput());
    
    // Override
    node->OverrideValue(true, 1.0f);
    ASSERT_TRUE(node->IsOverridden());
    engine.Update(t);
    ASSERT_TRUE(node->GetOutput());
    
    return true;
}

bool test_Node_Digital_port_names()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "NODE", ElementType::NodeDigital);
    Element* node = engine.GetElement("NODE");
    
    ASSERT_TRUE(node->GetInputPort("input") != nullptr);
    ASSERT_TRUE(node->GetOutputPort("output") != nullptr);
    
    return true;
}

#ifdef LE_ELEMENTS_ANALOG
bool test_Node_Analog_basic()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "IN", ElementType::NodeAnalog);
    TestFramework::CreateElement(&engine, "OUT", ElementType::NodeAnalog);
    
    TestFramework::ConnectElements(&engine, "IN", "output", "OUT", "input");
    
    NodeAnalog* in = (NodeAnalog*)engine.GetElement("IN");
    NodeAnalog* out = (NodeAnalog*)engine.GetElement("OUT");
    
    Time t = Time::GetTime();
    
    in->SetValue(123.45f);
    engine.Update(t);
    ASSERT_NEAR(out->GetOutput(), 123.45f, 0.001f);
    
    return true;
}

#ifdef LE_ELEMENTS_ANALOG_COMPLEX
bool test_Node_AnalogComplex_basic()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "IN", ElementType::NodeAnalogComplex);
    TestFramework::CreateElement(&engine, "OUT", ElementType::NodeAnalogComplex);
    
    TestFramework::ConnectElements(&engine, "IN", "output", "OUT", "input");
    
    NodeAnalogComplex* in = (NodeAnalogComplex*)engine.GetElement("IN");
    NodeAnalogComplex* out = (NodeAnalogComplex*)engine.GetElement("OUT");
    
    Time t = Time::GetTime();
    
    in->SetValue(std::complex<float>(3.0f, 4.0f));
    engine.Update(t);
    
    std::complex<float> result = out->GetOutput();
    ASSERT_NEAR(result.real(), 3.0f, 0.001f);
    ASSERT_NEAR(result.imag(), 4.0f, 0.001f);
    
    return true;
}
#endif
#endif

void RunNodeTests()
{
    std::cout << "\n=== le_Node Tests ===" << std::endl;
    TestFramework::RunTest("ElementType::NodeDigital - Basic passthrough", test_Node_Digital_basic);
    TestFramework::RunTest("ElementType::NodeDigital - Override", test_Node_Digital_override);
    TestFramework::RunTest("ElementType::NodeDigital - Port names", test_Node_Digital_port_names);
#ifdef LE_ELEMENTS_ANALOG
    TestFramework::RunTest("ElementType::NodeAnalog - Basic passthrough", test_Node_Analog_basic);
#ifdef LE_ELEMENTS_ANALOG_COMPLEX
    TestFramework::RunTest("NodeAnalogComplex - Basic passthrough", test_Node_AnalogComplex_basic);
#endif
#endif
}

