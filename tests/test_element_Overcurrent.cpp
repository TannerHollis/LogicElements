#include "test_framework.hpp"

using namespace LogicElements;

#ifdef LE_ELEMENTS_ANALOG

bool test_Overcurrent_heterogeneous()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "CURRENT", ElementType::NodeAnalog);
    
    // Create Overcurrent element
    Engine::ElementTypeDef ocComp("OC", ElementType::Overcurrent);
    Engine::CopyAndClampString("DT", ocComp.args[0].sArg, LE_ELEMENT_ARGUMENT_LENGTH);
    ocComp.args[1].fArg = 100.0f; // pickup
    ocComp.args[2].fArg = 1.0f;   // time dial
    ocComp.args[3].fArg = 0.0f;   // time adder
    ocComp.args[4].bArg = false;  // em reset
    engine.AddElement(&ocComp);
    
    TestFramework::CreateElement(&engine, "TRIP", ElementType::NodeDigital);
    
    TestFramework::ConnectElements(&engine, "CURRENT", LE_PORT_OUTPUT_PREFIX, "OC", "current");
    TestFramework::ConnectElements(&engine, "OC", "trip", "TRIP", LE_PORT_INPUT_PREFIX);
    
    NodeAnalog* current = (NodeAnalog*)engine.GetElement("CURRENT");
    NodeDigital* trip = (NodeDigital*)engine.GetElement("TRIP");
    
    Time t = Time::GetTime();
    
    // HETEROGENEOUS TEST: float input -> bool output
    current->SetValue(50.0f); // Below 100A pickup
    engine.Update(t);
    ASSERT_FALSE(trip->GetOutput());
    
    return true;
}

bool test_Overcurrent_port_types()
{
    Engine engine("TestEngine");
    
    Engine::ElementTypeDef ocComp("OC", ElementType::Overcurrent);
    Engine::CopyAndClampString("DT", ocComp.args[0].sArg, LE_ELEMENT_ARGUMENT_LENGTH);
    ocComp.args[1].fArg = 100.0f;
    ocComp.args[2].fArg = 1.0f;
    ocComp.args[3].fArg = 0.0f;
    ocComp.args[4].bArg = false;
    engine.AddElement(&ocComp);
    
    Element* oc = engine.GetElement("OC");
    
    // Verify HETEROGENEOUS port types!
    ASSERT_EQUAL(oc->GetInputPort("current")->GetType(), PortType::ANALOG);   // float
    ASSERT_EQUAL(oc->GetOutputPort("trip")->GetType(), PortType::DIGITAL);    // bool
    
    // This demonstrates mixing ANALOG and DIGITAL on same element!
    return true;
}

bool test_Overcurrent_port_names()
{
    Engine engine("TestEngine");
    
    Engine::ElementTypeDef ocComp("OC", ElementType::Overcurrent);
    Engine::CopyAndClampString("C1", ocComp.args[0].sArg, LE_ELEMENT_ARGUMENT_LENGTH);
    ocComp.args[1].fArg = 100.0f;
    ocComp.args[2].fArg = 1.0f;
    ocComp.args[3].fArg = 0.0f;
    ocComp.args[4].bArg = false;
    engine.AddElement(&ocComp);
    
    Element* oc = engine.GetElement("OC");
    
    ASSERT_TRUE(oc->GetInputPort("current") != nullptr);
    ASSERT_TRUE(oc->GetOutputPort("trip") != nullptr);
    
    return true;
}

void RunOvercurrentTests()
{
    std::cout << "\n=== Overcurrent Tests ===" << std::endl;
    TestFramework::RunTest("Overcurrent - Heterogeneous (float->bool)", test_Overcurrent_heterogeneous);
    TestFramework::RunTest("Overcurrent - Port type validation", test_Overcurrent_port_types);
    TestFramework::RunTest("Overcurrent - Port names", test_Overcurrent_port_names);
}

#endif

