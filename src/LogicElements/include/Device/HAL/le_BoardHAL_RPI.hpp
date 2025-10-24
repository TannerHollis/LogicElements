/**
 * @file le_BoardHAL_RPI.hpp
 * @brief Raspberry Pi Hardware Abstraction Layer
 * 
 * Platform-specific implementation for Raspberry Pi using pigpio library.
 * Only compiled when LE_BOARD_RPI is defined.
 */

#pragma once

#ifdef LE_BOARD_RPI

#include "Device/HAL/le_BoardHAL.hpp"

namespace LogicElements {

/**
 * @brief Raspberry Pi HAL implementation
 * 
 * Uses pigpio library for GPIO operations.
 * Pin numbers use BCM GPIO numbering (e.g., GPIO 17, GPIO 27, etc.)
 * Port number is ignored (Raspberry Pi doesn't have ports)
 * 
 * Requirements:
 * - pigpio library installed
 * - Run with sudo or configure pigpio daemon
 */
class BoardHAL_RPI : public BoardHAL {
public:
    BoardHAL_RPI();
    virtual ~BoardHAL_RPI();
    
    // Platform lifecycle
    bool Init() override;
    void Shutdown() override;
    const char* GetPlatformName() const override;
    
    // Digital I/O operations
    bool ReadDigital(const GPIOPin& pin) override;
    void WriteDigital(const GPIOPin& pin, bool value) override;
    void ConfigureDigitalInput(const GPIOPin& pin) override;
    void ConfigureDigitalOutput(const GPIOPin& pin) override;
    
    // Analog I/O operations (limited support via SPI ADC/DAC)
    bool ReadAnalog(const GPIOPin& pin, float& value) override;
    bool WriteAnalog(const GPIOPin& pin, float value) override;
    bool ConfigureAnalogInput(const GPIOPin& pin) override;
    bool ConfigureAnalogOutput(const GPIOPin& pin) override;
    
    // Diagnostics
    void LogError(const char* message) override;

private:
    bool initialized;
};

} // namespace LogicElements

#endif // LE_BOARD_RPI

