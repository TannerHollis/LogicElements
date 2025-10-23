#include "test_framework.hpp"

using namespace LogicElements;

#ifdef LE_ELEMENTS_ANALOG

bool test_Analog3PWinding_port_names()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "WINDING", ElementType::Analog3PWinding, 32);
    Element* winding = engine.GetElement("WINDING");
    
    // Verify raw input ports
    ASSERT_TRUE(winding->GetInputPort("raw_a") != nullptr);
    ASSERT_TRUE(winding->GetInputPort("raw_b") != nullptr);
    ASSERT_TRUE(winding->GetInputPort("raw_c") != nullptr);
    
#ifdef LE_ELEMENTS_ANALOG_COMPLEX
    // Complex version
    ASSERT_TRUE(winding->GetInputPort("reference") != nullptr);
    ASSERT_TRUE(winding->GetOutputPort("phase_a") != nullptr);
    ASSERT_TRUE(winding->GetOutputPort("phase_b") != nullptr);
    ASSERT_TRUE(winding->GetOutputPort("phase_c") != nullptr);
    ASSERT_TRUE(winding->GetOutputPort("seq_0") != nullptr);
    ASSERT_TRUE(winding->GetOutputPort("seq_1") != nullptr);
    ASSERT_TRUE(winding->GetOutputPort("seq_2") != nullptr);
#else
    // Float version
    ASSERT_TRUE(winding->GetInputPort("reference_real") != nullptr);
    ASSERT_TRUE(winding->GetInputPort("reference_imag") != nullptr);
    ASSERT_TRUE(winding->GetOutputPort("phase_a_real") != nullptr);
    ASSERT_TRUE(winding->GetOutputPort("phase_a_imag") != nullptr);
    ASSERT_TRUE(winding->GetOutputPort("seq_0_real") != nullptr);
    ASSERT_TRUE(winding->GetOutputPort("seq_0_imag") != nullptr);
#endif
    
    return true;
}

bool test_Analog3PWinding_factory()
{
    Engine engine("TestEngine");
    
    TestFramework::CreateElement(&engine, "WINDING", ElementType::Analog3PWinding, 32);
    Element* winding = engine.GetElement("WINDING");
    
    ASSERT_TRUE(winding != nullptr);
    ASSERT_EQUAL(Element::GetType(winding), ElementType::Analog3PWinding);
    
    // Verify 3-phase has all phase outputs
    ASSERT_TRUE(winding->GetOutputPortCount() >= 6); // At least 6 outputs (3 phases + 3 sequences)
    
    return true;
}

void RunAnalog3PWindingTests()
{
    std::cout << "\n=== Analog3PWinding Tests ===" << std::endl;
    TestFramework::RunTest("Analog3PWinding - Port names", test_Analog3PWinding_port_names);
    TestFramework::RunTest("Analog3PWinding - Factory creation", test_Analog3PWinding_factory);
}

#endif

