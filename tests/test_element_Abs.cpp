#include "test_framework.hpp"

using namespace LogicElements;

#ifdef LE_ELEMENTS_ANALOG

bool test_Abs_basic()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "IN", ElementType::NodeAnalog);
    TestFramework::CreateElement(&engine, "ABS", ElementType::Abs);
    TestFramework::CreateElement(&engine, "OUT", ElementType::NodeAnalog);
    
    TestFramework::ConnectElements(&engine, "IN", "output", "ABS", "input");
    TestFramework::ConnectElements(&engine, "ABS", "output", "OUT", "input");
    
    NodeAnalog* in = (NodeAnalog*)engine.GetElement("IN");
    NodeAnalog* out = (NodeAnalog*)engine.GetElement("OUT");
    
    Time t = Time::GetTime();
    
    // Test: |5.0| = 5.0
    in->SetValue(5.0f);
    engine.Update(t);
    ASSERT_NEAR(out->GetOutput(), 5.0f, 0.001f);
    
    // Test: |-3.0| = 3.0
    in->SetValue(-3.0f);
    engine.Update(t);
    ASSERT_NEAR(out->GetOutput(), 3.0f, 0.001f);
    
    // Test: |0.0| = 0.0
    in->SetValue(0.0f);
    engine.Update(t);
    ASSERT_NEAR(out->GetOutput(), 0.0f, 0.001f);
    
    return true;
}

bool test_Abs_port_names()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "ABS", ElementType::Abs);
    Element* abs = engine.GetElement("ABS");
    
    ASSERT_TRUE(abs->GetInputPort("input") != nullptr);
    ASSERT_TRUE(abs->GetOutputPort("output") != nullptr);
    ASSERT_EQUAL(abs->GetInputPort("input")->GetType(), PortType::ANALOG);
    
    return true;
}

void RunAbsTests()
{
    std::cout << "\n=== Abs Tests ===" << std::endl;
    TestFramework::RunTest("Abs - Basic absolute value", test_Abs_basic);
    TestFramework::RunTest("Abs - Port names", test_Abs_port_names);
}

#endif

