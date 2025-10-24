/**
 * @file le_BoardHAL_Generic.hpp
 * @brief Generic/Simulator Hardware Abstraction Layer
 * 
 * Provides a simulation layer for testing without hardware.
 * Logs all I/O operations and maintains internal state.
 */

#pragma once

#include "Device/HAL/le_BoardHAL.hpp"

#include <map>
#include <cstdio>

namespace LogicElements {

/**
 * @brief Generic/Simulator HAL implementation for testing
 * 
 * This HAL provides:
 * - Simulated I/O with internal state storage
 * - Console logging of all operations
 * - No hardware dependencies
 * - Perfect for unit tests and CI/CD
 */
class BoardHAL_Generic : public BoardHAL {
public:
    BoardHAL_Generic();
    virtual ~BoardHAL_Generic() = default;
    
    // Platform lifecycle
    bool Init() override;
    void Shutdown() override;
    const char* GetPlatformName() const override;
    
    // Digital I/O operations
    bool ReadDigital(const GPIOPin& pin) override;
    void WriteDigital(const GPIOPin& pin, bool value) override;
    void ConfigureDigitalInput(const GPIOPin& pin) override;
    void ConfigureDigitalOutput(const GPIOPin& pin) override;
    
    // Analog I/O operations
    bool ReadAnalog(const GPIOPin& pin, float& value) override;
    bool WriteAnalog(const GPIOPin& pin, float value) override;
    bool ConfigureAnalogInput(const GPIOPin& pin) override;
    bool ConfigureAnalogOutput(const GPIOPin& pin) override;
    
    // Diagnostics
    void LogError(const char* message) override;
    
    // Simulator-specific methods for testing
    void SetSimulatedDigitalInput(uint32_t port, uint32_t pin, bool value);
    void SetSimulatedAnalogInput(uint32_t port, uint32_t pin, float value);
    bool GetSimulatedDigitalOutput(uint32_t port, uint32_t pin) const;
    float GetSimulatedAnalogOutput(uint32_t port, uint32_t pin) const;

private:
    // Internal state storage for simulation
    std::map<uint64_t, bool> digitalInputs;   ///< Simulated digital input states
    std::map<uint64_t, bool> digitalOutputs;  ///< Simulated digital output states
    std::map<uint64_t, float> analogInputs;   ///< Simulated analog input values
    std::map<uint64_t, float> analogOutputs;  ///< Simulated analog output values
    
    // Helper to create unique key from port/pin
    inline uint64_t MakeKey(uint32_t port, uint32_t pin) const {
        return ((uint64_t)port << 32) | pin;
    }
};

} // namespace LogicElements

