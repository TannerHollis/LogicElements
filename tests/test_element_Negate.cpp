#include "test_framework.hpp"

using namespace LogicElements;

#ifdef LE_ELEMENTS_ANALOG

bool test_Negate_basic()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "IN", ElementType::NodeAnalog);
    TestFramework::CreateElement(&engine, "NEG", ElementType::Negate);
    TestFramework::CreateElement(&engine, "OUT", ElementType::NodeAnalog);
    
    TestFramework::ConnectElements(&engine, "IN", "output", "NEG", "input");
    TestFramework::ConnectElements(&engine, "NEG", "output", "OUT", "input");
    
    NodeAnalog* in = (NodeAnalog*)engine.GetElement("IN");
    NodeAnalog* out = (NodeAnalog*)engine.GetElement("OUT");
    
    Time t = Time::GetTime();
    
    // Test: -(5.0) = -5.0
    in->SetValue(5.0f);
    engine.Update(t);
    ASSERT_NEAR(out->GetOutput(), -5.0f, 0.001f);
    
    // Test: -(-3.0) = 3.0
    in->SetValue(-3.0f);
    engine.Update(t);
    ASSERT_NEAR(out->GetOutput(), 3.0f, 0.001f);
    
    // Test: -(0.0) = 0.0
    in->SetValue(0.0f);
    engine.Update(t);
    ASSERT_NEAR(out->GetOutput(), 0.0f, 0.001f);
    
    return true;
}

bool test_Negate_port_names()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "NEG", ElementType::Negate);
    Element* neg = engine.GetElement("NEG");
    
    ASSERT_TRUE(neg->GetInputPort("input") != nullptr);
    ASSERT_TRUE(neg->GetOutputPort("output") != nullptr);
    ASSERT_EQUAL(neg->GetInputPort("input")->GetType(), PortType::ANALOG);
    
    return true;
}

void RunNegateTests()
{
    std::cout << "\n=== Negate Tests ===" << std::endl;
    TestFramework::RunTest("Negate - Basic negation", test_Negate_basic);
    TestFramework::RunTest("Negate - Port names", test_Negate_port_names);
}

#endif

