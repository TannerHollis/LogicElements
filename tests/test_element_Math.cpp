#include "test_framework.hpp"

using namespace LogicElements;

#ifdef LE_ELEMENTS_MATH

bool test_Math_basic_expression()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "X0", ElementType::NodeAnalog);
    TestFramework::CreateElement(&engine, "X1", ElementType::NodeAnalog);
    
    // Create Math element with expression
    Engine::ElementTypeDef mathComp("MATH", ElementType::Math);
    mathComp.args[0].uArg = 2; // 2 inputs
    Engine::CopyAndClampString("x0 + x1 * 2", mathComp.args[1].sArg, LE_ELEMENT_ARGUMENT_LENGTH);
    engine.AddElement(&mathComp);
    
    TestFramework::CreateElement(&engine, "RESULT", ElementType::NodeAnalog);
    
    TestFramework::ConnectElements(&engine, "X0", "output", "MATH", "x0");
    TestFramework::ConnectElements(&engine, "X1", "output", "MATH", "x1");
    TestFramework::ConnectElements(&engine, "MATH", "output", "RESULT", "input");
    
    NodeAnalog* x0 = (NodeAnalog*)engine.GetElement("X0");
    NodeAnalog* x1 = (NodeAnalog*)engine.GetElement("X1");
    NodeAnalog* result = (NodeAnalog*)engine.GetElement("RESULT");
    
    Time t = Time::GetTime();
    
    // Test: x0=10, x1=5 -> 10 + 5*2 = 20
    x0->SetValue(10.0f);
    x1->SetValue(5.0f);
    engine.Update(t);
    
    ASSERT_NEAR(result->GetOutput(), 20.0f, 0.001f);
    
    return true;
}

bool test_Math_complex_expression()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "X0", ElementType::NodeAnalog);
    TestFramework::CreateElement(&engine, "X1", ElementType::NodeAnalog);
    TestFramework::CreateElement(&engine, "X2", ElementType::NodeAnalog);
    
    Engine::ElementTypeDef mathComp("MATH", ElementType::Math);
    mathComp.args[0].uArg = 3; // 3 inputs
    Engine::CopyAndClampString("(x0 + x1) * x2", mathComp.args[1].sArg, LE_ELEMENT_ARGUMENT_LENGTH);
    engine.AddElement(&mathComp);
    
    TestFramework::CreateElement(&engine, "RESULT", ElementType::NodeAnalog);
    
    TestFramework::ConnectElements(&engine, "X0", "output", "MATH", "x0");
    TestFramework::ConnectElements(&engine, "X1", "output", "MATH", "x1");
    TestFramework::ConnectElements(&engine, "X2", "output", "MATH", "x2");
    TestFramework::ConnectElements(&engine, "MATH", "output", "RESULT", "input");
    
    NodeAnalog* x0 = (NodeAnalog*)engine.GetElement("X0");
    NodeAnalog* x1 = (NodeAnalog*)engine.GetElement("X1");
    NodeAnalog* x2 = (NodeAnalog*)engine.GetElement("X2");
    NodeAnalog* result = (NodeAnalog*)engine.GetElement("RESULT");
    
    Time t = Time::GetTime();
    
    // Test: (3 + 4) * 5 = 35
    x0->SetValue(3.0f);
    x1->SetValue(4.0f);
    x2->SetValue(5.0f);
    engine.Update(t);
    
    ASSERT_NEAR(result->GetOutput(), 35.0f, 0.001f);
    
    return true;
}

bool test_Math_port_names()
{
    Engine engine("TestEngine");
    
    Engine::ElementTypeDef mathComp("MATH", ElementType::Math);
    mathComp.args[0].uArg = 2;
    Engine::CopyAndClampString("x0 + x1", mathComp.args[1].sArg, LE_ELEMENT_ARGUMENT_LENGTH);
    engine.AddElement(&mathComp);
    
    Element* math = engine.GetElement("MATH");
    
    ASSERT_TRUE(math->GetInputPort("x0") != nullptr);
    ASSERT_TRUE(math->GetInputPort("x1") != nullptr);
    ASSERT_TRUE(math->GetOutputPort("output") != nullptr);
    
    return true;
}

void RunMathTests()
{
    std::cout << "\n=== Math Tests ===" << std::endl;
    TestFramework::RunTest("Math - Basic expression", test_Math_basic_expression);
    TestFramework::RunTest("Math - Complex expression", test_Math_complex_expression);
    TestFramework::RunTest("Math - Port names", test_Math_port_names);
}

#endif

