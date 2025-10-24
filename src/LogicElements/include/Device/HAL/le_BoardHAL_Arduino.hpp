/**
 * @file le_BoardHAL_Arduino.hpp
 * @brief Arduino Hardware Abstraction Layer
 * 
 * Platform-specific implementation for Arduino boards.
 * Only compiled when LE_BOARD_ARDUINO is defined.
 */

#pragma once

#ifdef LE_BOARD_ARDUINO

#include "Device/HAL/le_BoardHAL.hpp"

namespace LogicElements {

/**
 * @brief Arduino HAL implementation
 * 
 * Uses Arduino API (digitalWrite, digitalRead, analogRead, analogWrite).
 * Pin numbers use Arduino pin numbering (e.g., 13 for LED pin).
 * Port number is ignored (Arduino uses flat pin numbering).
 * 
 * Supports:
 * - Digital I/O (all pins)
 * - Analog Input (A0-A5 or more depending on board)
 * - Analog Output via PWM (pins with ~ symbol)
 */
class BoardHAL_Arduino : public BoardHAL {
public:
    BoardHAL_Arduino();
    virtual ~BoardHAL_Arduino() = default;
    
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
};

} // namespace LogicElements

#endif // LE_BOARD_ARDUINO

