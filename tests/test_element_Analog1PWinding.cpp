#include "test_framework.hpp"

using namespace LogicElements;

#ifdef LE_ELEMENTS_ANALOG

bool test_Analog1PWinding_port_names()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "WINDING", ElementType::Analog1PWinding, 32); // 32 samples/cycle
    Element* winding = engine.GetElement("WINDING");
    
    // Verify input ports
    ASSERT_TRUE(winding->GetInputPort("raw") != nullptr);
    
#ifdef LE_ELEMENTS_ANALOG_COMPLEX
    ASSERT_TRUE(winding->GetInputPort("reference") != nullptr);
    ASSERT_TRUE(winding->GetOutputPort(LE_PORT_OUTPUT_PREFIX) != nullptr);
    ASSERT_EQUAL(winding->GetInputPort("reference")->GetType(), PortType::COMPLEX);
    ASSERT_EQUAL(winding->GetOutputPort(LE_PORT_OUTPUT_PREFIX)->GetType(), PortType::COMPLEX);
#else
    ASSERT_TRUE(winding->GetInputPort("reference_real") != nullptr);
    ASSERT_TRUE(winding->GetInputPort("reference_imag") != nullptr);
    ASSERT_TRUE(winding->GetOutputPort("real") != nullptr);
    ASSERT_TRUE(winding->GetOutputPort("imaginary") != nullptr);
    ASSERT_EQUAL(winding->GetInputPort("raw")->GetType(), PortType::ANALOG);
    ASSERT_EQUAL(winding->GetOutputPort("real")->GetType(), PortType::ANALOG);
#endif
    
    return true;
}

bool test_Analog1PWinding_factory()
{
    Engine engine("TestEngine");
    
    // Verify element can be created via factory
    TestFramework::CreateElement(&engine, "WINDING", ElementType::Analog1PWinding, 32);
    Element* winding = engine.GetElement("WINDING");
    
    ASSERT_TRUE(winding != nullptr);
    ASSERT_EQUAL(Element::GetType(winding), ElementType::Analog1PWinding);
    
    return true;
}

void RunAnalog1PWindingTests()
{
    std::cout << "\n=== Analog1PWinding Tests ===" << std::endl;
    TestFramework::RunTest("Analog1PWinding - Port names", test_Analog1PWinding_port_names);
    TestFramework::RunTest("Analog1PWinding - Factory creation", test_Analog1PWinding_factory);
}

#endif

