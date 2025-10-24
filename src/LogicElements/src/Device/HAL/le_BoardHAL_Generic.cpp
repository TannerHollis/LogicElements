/**
 * @file le_BoardHAL_Generic.cpp
 * @brief Generic/Simulator HAL implementation
 */

#include "Device/HAL/le_BoardHAL_Generic.hpp"

namespace LogicElements {

BoardHAL_Generic::BoardHAL_Generic()
{
    // Initialize with default values
}

bool BoardHAL_Generic::Init()
{
    printf("[Generic HAL] Initialized in Simulator Mode\n");
    printf("[Generic HAL] Platform: %s\n", GetPlatformName());
    return true;
}

void BoardHAL_Generic::Shutdown()
{
    printf("[Generic HAL] Shutdown\n");
    digitalInputs.clear();
    digitalOutputs.clear();
    analogInputs.clear();
    analogOutputs.clear();
}

const char* BoardHAL_Generic::GetPlatformName() const
{
    return "Generic/Simulator";
}

// ========================================
// Digital I/O Operations
// ========================================

bool BoardHAL_Generic::ReadDigital(const GPIOPin& pin)
{
    uint64_t key = MakeKey(pin.port, pin.pin);
    
    // Return stored value or default (false)
    if (digitalInputs.find(key) != digitalInputs.end())
    {
        return digitalInputs[key];
    }
    
    return false; // Default to LOW
}

void BoardHAL_Generic::WriteDigital(const GPIOPin& pin, bool value)
{
    uint64_t key = MakeKey(pin.port, pin.pin);
    digitalOutputs[key] = value;
    
    printf("[Generic HAL] Digital Write: Port %u, Pin %u = %s\n", 
           pin.port, pin.pin, value ? "HIGH" : "LOW");
}

void BoardHAL_Generic::ConfigureDigitalInput(const GPIOPin& pin)
{
    printf("[Generic HAL] Configure Digital Input: Port %u, Pin %u\n", 
           pin.port, pin.pin);
}

void BoardHAL_Generic::ConfigureDigitalOutput(const GPIOPin& pin)
{
    printf("[Generic HAL] Configure Digital Output: Port %u, Pin %u\n", 
           pin.port, pin.pin);
}

// ========================================
// Analog I/O Operations
// ========================================

bool BoardHAL_Generic::ReadAnalog(const GPIOPin& pin, float& value)
{
    uint64_t key = MakeKey(pin.port, pin.pin);
    
    // Return stored value or default (0.0)
    if (analogInputs.find(key) != analogInputs.end())
    {
        value = analogInputs[key];
        return true;
    }
    
    value = 0.0f;
    return true; // Generic HAL always succeeds
}

bool BoardHAL_Generic::WriteAnalog(const GPIOPin& pin, float value)
{
    uint64_t key = MakeKey(pin.port, pin.pin);
    analogOutputs[key] = value;
    
    printf("[Generic HAL] Analog Write: Port %u, Pin %u = %.3f\n", 
           pin.port, pin.pin, value);
    
    return true;
}

bool BoardHAL_Generic::ConfigureAnalogInput(const GPIOPin& pin)
{
    printf("[Generic HAL] Configure Analog Input: Port %u, Pin %u\n", 
           pin.port, pin.pin);
    return true;
}

bool BoardHAL_Generic::ConfigureAnalogOutput(const GPIOPin& pin)
{
    printf("[Generic HAL] Configure Analog Output: Port %u, Pin %u\n", 
           pin.port, pin.pin);
    return true;
}

// ========================================
// Diagnostics
// ========================================

void BoardHAL_Generic::LogError(const char* message)
{
    printf("[Generic HAL ERROR] %s\n", message);
}

// ========================================
// Simulator-Specific Methods
// ========================================

void BoardHAL_Generic::SetSimulatedDigitalInput(uint32_t port, uint32_t pin, bool value)
{
    uint64_t key = MakeKey(port, pin);
    digitalInputs[key] = value;
    printf("[Generic HAL] Simulated Input Set: Port %u, Pin %u = %s\n", 
           port, pin, value ? "HIGH" : "LOW");
}

void BoardHAL_Generic::SetSimulatedAnalogInput(uint32_t port, uint32_t pin, float value)
{
    uint64_t key = MakeKey(port, pin);
    analogInputs[key] = value;
    printf("[Generic HAL] Simulated Analog Input Set: Port %u, Pin %u = %.3f\n", 
           port, pin, value);
}

bool BoardHAL_Generic::GetSimulatedDigitalOutput(uint32_t port, uint32_t pin) const
{
    uint64_t key = MakeKey(port, pin);
    auto it = digitalOutputs.find(key);
    return (it != digitalOutputs.end()) ? it->second : false;
}

float BoardHAL_Generic::GetSimulatedAnalogOutput(uint32_t port, uint32_t pin) const
{
    uint64_t key = MakeKey(port, pin);
    auto it = analogOutputs.find(key);
    return (it != analogOutputs.end()) ? it->second : 0.0f;
}

} // namespace LogicElements

