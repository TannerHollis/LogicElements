/**
 * @file le_BoardHAL_STM32.hpp
 * @brief STM32 Hardware Abstraction Layer
 * 
 * Platform-specific implementation for STM32 microcontrollers using STM32 HAL.
 * Only compiled when LE_BOARD_STM32 is defined.
 */

#pragma once

#ifdef LE_BOARD_STM32

#include "Device/HAL/le_BoardHAL.hpp"

// Include STM32 HAL headers (user must ensure correct variant is included in their project)
// Example: #include "stm32f4xx_hal.h" or #include "stm32h7xx_hal.h"
// This should be included in the user's main.h or project configuration

namespace LogicElements {

/**
 * @brief STM32 HAL implementation
 * 
 * Maps GPIOPin to STM32 HAL GPIO operations.
 * Port numbers map to GPIO ports (0=GPIOA, 1=GPIOB, etc.)
 * Pin numbers use STM32 GPIO_PIN_x defines
 */
class BoardHAL_STM32 : public BoardHAL {
public:
    BoardHAL_STM32();
    virtual ~BoardHAL_STM32() = default;
    
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

private:
    /**
     * @brief Helper to convert port number to GPIO_TypeDef pointer
     * @param port Port number (0=GPIOA, 1=GPIOB, etc.)
     * @return Pointer to GPIO_TypeDef or nullptr if invalid
     */
    void* GetGPIOPort(uint32_t port) const;
};

} // namespace LogicElements

#endif // LE_BOARD_STM32

