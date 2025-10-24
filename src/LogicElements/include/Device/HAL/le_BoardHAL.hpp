/**
 * @file le_BoardHAL.hpp
 * @brief Hardware Abstraction Layer interface for Board I/O operations
 * 
 * Defines abstract interface for platform-specific hardware operations.
 * Platform implementations (STM32, RPi, Arduino) inherit from this base.
 */

#pragma once

#include <cstdint>

namespace LogicElements {

/**
 * @brief Platform-agnostic GPIO pin representation
 * 
 * Replaces void* with type-safe structure. Platform HALs interpret
 * port/pin numbers according to their hardware conventions.
 */
struct GPIOPin {
    uint32_t port;    ///< Port number/identifier (e.g., 0=GPIOA, 1=GPIOB for STM32)
    uint32_t pin;     ///< Pin number (e.g., GPIO_PIN_0, or BCM pin number)
    void* context;    ///< Optional platform-specific context data
    
    GPIOPin() : port(0), pin(0), context(nullptr) {}
    GPIOPin(uint32_t p, uint32_t pn) : port(p), pin(pn), context(nullptr) {}
};

/**
 * @brief Abstract Hardware Abstraction Layer interface for Board operations
 * 
 * Platform-specific implementations must inherit from this class and
 * implement all pure virtual methods. This enables the Board class to
 * remain platform-agnostic while supporting multiple hardware targets.
 * 
 * Supported platforms: Generic (simulator), STM32, Raspberry Pi, Arduino, etc.
 */
class BoardHAL {
public:
    virtual ~BoardHAL() = default;
    
    // ========================================
    // Platform Lifecycle
    // ========================================
    
    /**
     * @brief Initialize the platform hardware
     * @return true if initialization successful, false otherwise
     */
    virtual bool Init() = 0;
    
    /**
     * @brief Shutdown and cleanup platform hardware
     */
    virtual void Shutdown() = 0;
    
    /**
     * @brief Get platform name for diagnostics
     * @return Platform name string (e.g., "STM32", "Raspberry Pi")
     */
    virtual const char* GetPlatformName() const = 0;
    
    // ========================================
    // Digital I/O Operations
    // ========================================
    
    /**
     * @brief Read digital input pin state
     * @param pin GPIO pin to read
     * @return true if pin is HIGH, false if LOW
     */
    virtual bool ReadDigital(const GPIOPin& pin) = 0;
    
    /**
     * @brief Write digital output pin state
     * @param pin GPIO pin to write
     * @param value true for HIGH, false for LOW
     */
    virtual void WriteDigital(const GPIOPin& pin, bool value) = 0;
    
    /**
     * @brief Configure pin as digital input
     * @param pin GPIO pin to configure
     */
    virtual void ConfigureDigitalInput(const GPIOPin& pin) = 0;
    
    /**
     * @brief Configure pin as digital output
     * @param pin GPIO pin to configure
     */
    virtual void ConfigureDigitalOutput(const GPIOPin& pin) = 0;
    
    // ========================================
    // Analog I/O Operations (Optional)
    // ========================================
    
    /**
     * @brief Read analog input pin value
     * @param pin GPIO pin to read
     * @param value Reference to store read value (0.0 to 1.0 normalized)
     * @return true if read successful, false if not supported/failed
     */
    virtual bool ReadAnalog(const GPIOPin& pin, float& value) = 0;
    
    /**
     * @brief Write analog output pin value
     * @param pin GPIO pin to write
     * @param value Value to write (0.0 to 1.0 normalized)
     * @return true if write successful, false if not supported/failed
     */
    virtual bool WriteAnalog(const GPIOPin& pin, float value) = 0;
    
    /**
     * @brief Configure pin as analog input
     * @param pin GPIO pin to configure
     * @return true if configuration successful, false if not supported
     */
    virtual bool ConfigureAnalogInput(const GPIOPin& pin) = 0;
    
    /**
     * @brief Configure pin as analog output
     * @param pin GPIO pin to configure
     * @return true if configuration successful, false if not supported
     */
    virtual bool ConfigureAnalogOutput(const GPIOPin& pin) = 0;
    
    // ========================================
    // Diagnostics
    // ========================================
    
    /**
     * @brief Log error message (platform-specific logging)
     * @param message Error message to log
     */
    virtual void LogError(const char* message) = 0;
};

} // namespace LogicElements

