# Platform HAL Development Guide

Complete guide for implementing Hardware Abstraction Layer (HAL) for new platforms.

---

## 📚 Table of Contents

- [Overview](#overview)
- [HAL Interface](#hal-interface)
- [Creating New Platform HAL](#creating-new-platform-hal)
- [Supported Platforms](#supported-platforms)
- [Testing Your HAL](#testing-your-hal)
- [Best Practices](#best-practices)

---

## Overview

LogicElements uses a **Hardware Abstraction Layer (HAL)** to support multiple platforms without modifying core logic. The HAL pattern separates:

- **Platform-Agnostic**: `Board` class (unchanged across platforms)
- **Platform-Specific**: HAL implementations (one per platform)

### Benefits

✅ **Add platforms without touching core code**  
✅ **Test without hardware** (Generic HAL simulator)  
✅ **Type-safe** (`GPIOPin` struct instead of `void*`)  
✅ **Discoverable** (all HALs in `Device/HAL/` folder)  
✅ **C++ idiomatic** (virtual functions, polymorphism)  

---

## HAL Interface

All platform HALs must inherit from `BoardHAL` and implement these methods:

### Required Methods

```cpp
class BoardHAL {
public:
    // Platform lifecycle
    virtual bool Init() = 0;
    virtual void Shutdown() = 0;
    virtual const char* GetPlatformName() const = 0;
    
    // Digital I/O (REQUIRED)
    virtual bool ReadDigital(const GPIOPin& pin) = 0;
    virtual void WriteDigital(const GPIOPin& pin, bool value) = 0;
    virtual void ConfigureDigitalInput(const GPIOPin& pin) = 0;
    virtual void ConfigureDigitalOutput(const GPIOPin& pin) = 0;
    
    // Analog I/O (OPTIONAL - return false if unsupported)
    virtual bool ReadAnalog(const GPIOPin& pin, float& value) = 0;
    virtual bool WriteAnalog(const GPIOPin& pin, float value) = 0;
    virtual bool ConfigureAnalogInput(const GPIOPin& pin) = 0;
    virtual bool ConfigureAnalogOutput(const GPIOPin& pin) = 0;
    
    // Diagnostics
    virtual void LogError(const char* message) = 0;
};
```

### GPIOPin Structure

```cpp
struct GPIOPin {
    uint32_t port;    // Port identifier (e.g., 0=GPIOA, 1=GPIOB for STM32)
    uint32_t pin;     // Pin number
    void* context;    // Optional platform-specific data
};
```

**Interpretation is platform-specific:**
- **STM32**: `port` = GPIO bank (A, B, C...), `pin` = GPIO_PIN_x
- **Raspberry Pi**: `port` ignored, `pin` = BCM GPIO number (17, 27, etc.)
- **Arduino**: `port` ignored, `pin` = Arduino pin number (13, A0, etc.)

---

## Creating New Platform HAL

Follow these steps to add a new platform (e.g., ESP32):

### Step 1: Create Header File

`src/LogicElements/include/Device/HAL/le_BoardHAL_ESP32.hpp`

```cpp
#pragma once

#ifdef LE_BOARD_ESP32

#include "Device/HAL/le_BoardHAL.hpp"
// Include platform-specific headers
#include <driver/gpio.h>

namespace LogicElements {

class BoardHAL_ESP32 : public BoardHAL {
public:
    BoardHAL_ESP32();
    virtual ~BoardHAL_ESP32() = default;
    
    // Implement all pure virtual methods
    bool Init() override;
    void Shutdown() override;
    const char* GetPlatformName() const override;
    
    bool ReadDigital(const GPIOPin& pin) override;
    void WriteDigital(const GPIOPin& pin, bool value) override;
    void ConfigureDigitalInput(const GPIOPin& pin) override;
    void ConfigureDigitalOutput(const GPIOPin& pin) override;
    
    bool ReadAnalog(const GPIOPin& pin, float& value) override;
    bool WriteAnalog(const GPIOPin& pin, float value) override;
    bool ConfigureAnalogInput(const GPIOPin& pin) override;
    bool ConfigureAnalogOutput(const GPIOPin& pin) override;
    
    void LogError(const char* message) override;
};

} // namespace LogicElements

#endif // LE_BOARD_ESP32
```

### Step 2: Create Implementation File

`src/LogicElements/src/Device/HAL/le_BoardHAL_ESP32.cpp`

```cpp
#ifdef LE_BOARD_ESP32

#include "Device/HAL/le_BoardHAL_ESP32.hpp"
#include <esp_log.h>

namespace LogicElements {

BoardHAL_ESP32::BoardHAL_ESP32() {}

bool BoardHAL_ESP32::Init() {
    ESP_LOGI("BoardHAL", "ESP32 HAL Initialized");
    return true;
}

void BoardHAL_ESP32::Shutdown() {
    ESP_LOGI("BoardHAL", "ESP32 HAL Shutdown");
}

const char* BoardHAL_ESP32::GetPlatformName() const {
    return "ESP32";
}

bool BoardHAL_ESP32::ReadDigital(const GPIOPin& pin) {
    return gpio_get_level((gpio_num_t)pin.pin) == 1;
}

void BoardHAL_ESP32::WriteDigital(const GPIOPin& pin, bool value) {
    gpio_set_level((gpio_num_t)pin.pin, value ? 1 : 0);
}

void BoardHAL_ESP32::ConfigureDigitalInput(const GPIOPin& pin) {
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << pin.pin);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
    gpio_config(&io_conf);
}

void BoardHAL_ESP32::ConfigureDigitalOutput(const GPIOPin& pin) {
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << pin.pin);
    io_conf.mode = GPIO_MODE_OUTPUT;
    gpio_config(&io_conf);
}

bool BoardHAL_ESP32::ReadAnalog(const GPIOPin& pin, float& value) {
    // ESP32 ADC implementation
    // Return false if not implemented yet
    return false;
}

bool BoardHAL_ESP32::WriteAnalog(const GPIOPin& pin, float value) {
    // ESP32 DAC implementation
    return false;
}

bool BoardHAL_ESP32::ConfigureAnalogInput(const GPIOPin& pin) {
    return false; // Implement ADC configuration
}

bool BoardHAL_ESP32::ConfigureAnalogOutput(const GPIOPin& pin) {
    return false; // Implement DAC configuration
}

void BoardHAL_ESP32::LogError(const char* message) {
    ESP_LOGE("BoardHAL", "%s", message);
}

} // namespace LogicElements

#endif // LE_BOARD_ESP32
```

### Step 3: Update CMakeLists.txt

Add ESP32 to platform selection in `src/LogicElements/CMakeLists.txt`:

```cmake
# Add platform-specific HAL implementation
if(LE_BOARD_PLATFORM STREQUAL "STM32")
    # ... existing ...
elseif(LE_BOARD_PLATFORM STREQUAL "ESP32")
    list(APPEND CPP_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/Device/HAL/le_BoardHAL_ESP32.cpp")
    set(LE_BOARD_ESP32 ON)
    # Link ESP-IDF components if needed
else()
    # ... Generic ...
endif()
```

Also add to platform strings:

```cmake
set_property(CACHE LE_BOARD_PLATFORM PROPERTY STRINGS 
             "Generic" "STM32" "RPI" "Arduino" "ESP32")
```

### Step 4: Update le_Config.hpp.in

Add define in `src/LogicElements/include/Core/le_Config.hpp.in`:

```cpp
#cmakedefine LE_BOARD_ESP32
```

### Step 5: Test Your HAL

```cpp
// In your main.cpp or test
BoardHAL_ESP32 hal;
Board board("TestDevice", "ESP32-DEV", 4, 4, 2, &hal);

board.AddInput(0, "Button", 0, 5, false);  // GPIO5 as input
board.AddOutput(0, "LED", 0, 2, false);    // GPIO2 as output

// ... attach engine and run ...
```

---

## Supported Platforms

### 1. Generic (Simulator)

**Purpose**: Testing and CI/CD without hardware

**Features**:
- ✅ All I/O simulated with internal state
- ✅ Console logging of all operations
- ✅ Perfect for unit tests

**Pin Convention**: Arbitrary port/pin numbers

**Build**:
```bash
cmake -DLE_BOARD_PLATFORM=Generic -S . -B build
```

### 2. STM32

**Purpose**: STM32 microcontrollers (F4, H7, etc.)

**Features**:
- ✅ Digital I/O via STM32 HAL
- ⚠️ Analog I/O stub (user implements ADC/DAC)

**Pin Convention**:
- `port`: 0=GPIOA, 1=GPIOB, 2=GPIOC, etc.
- `pin`: GPIO_PIN_0, GPIO_PIN_1, etc.

**Build**:
```bash
cmake -DLE_BOARD_PLATFORM=STM32 -S . -B build
```

**Requirements**: User must include appropriate STM32 HAL header (`stm32f4xx_hal.h`, etc.)

### 3. Raspberry Pi

**Purpose**: Raspberry Pi (all models)

**Features**:
- ✅ Digital I/O via pigpio
- ⚠️ Analog I/O requires external ADC/DAC

**Pin Convention**:
- `port`: Ignored
- `pin`: BCM GPIO number (e.g., 17, 27, 22)

**Build**:
```bash
cmake -DLE_BOARD_PLATFORM=RPI -S . -B build
sudo ./build/executable  # Requires sudo for GPIO access
```

**Requirements**: `pigpio` library installed

### 4. Arduino

**Purpose**: Arduino boards (Uno, Mega, etc.)

**Features**:
- ✅ Digital I/O via Arduino API
- ✅ Analog input via analogRead
- ✅ PWM output via analogWrite

**Pin Convention**:
- `port`: Ignored
- `pin`: Arduino pin number (13, A0, etc.)

**Build**: Use Arduino IDE or PlatformIO

---

## Testing Your HAL

### Unit Test Example

```cpp
#include "test_framework.hpp"
#include "Device/HAL/le_BoardHAL_Generic.hpp"

bool test_HAL_DigitalIO()
{
    BoardHAL_Generic hal;
    hal.Init();
    
    GPIOPin testPin(0, 5);
    
    // Test write
    hal.WriteDigital(testPin, true);
    
    // Simulate input
    hal.SetSimulatedDigitalInput(0, 5, true);
    
    // Test read
    bool value = hal.ReadDigital(testPin);
    ASSERT_TRUE(value);
    
    hal.Shutdown();
    return true;
}
```

### Integration Test

```cpp
BoardHAL_Generic hal;
Board board("TestBoard", "TEST-001", 2, 2, 1, &hal);

board.AddInput(0, "DI_0", 0, 5, false);
board.AddOutput(0, "DO_0", 1, 3, false);

// Create simple logic
Engine engine("Test");
engine.AddElement("IN", ElementType::NodeDigital);
engine.AddElement("OUT", ElementType::NodeDigital);
engine.CreateConnection("IN", "output", "OUT", "input");

board.AttachEngine(&engine);

// Simulate input
hal.SetSimulatedDigitalInput(0, 5, true);
board.Update(Time::GetTime());

// Check output
bool output = hal.GetSimulatedDigitalOutput(1, 3);
ASSERT_TRUE(output);
```

---

## Best Practices

### 1. Error Handling

Always check return values and log errors:

```cpp
bool BoardHAL_MyPlatform::ReadAnalog(const GPIOPin& pin, float& value) {
    if (pin.pin > MAX_ADC_CHANNELS) {
        LogError("Invalid ADC channel");
        return false;
    }
    
    // ... implementation ...
    
    if (adcError) {
        LogError("ADC read failed");
        return false;
    }
    
    return true;
}
```

### 2. Initialization

Initialize hardware in `Init()`, not constructor:

```cpp
bool BoardHAL_MyPlatform::Init() {
    // Initialize peripherals
    if (!InitGPIO()) {
        LogError("GPIO initialization failed");
        return false;
    }
    
    if (!InitADC()) {
        LogError("ADC initialization failed");
        return false;
    }
    
    printf("[%s] Initialized successfully\n", GetPlatformName());
    return true;
}
```

### 3. Cleanup

Always cleanup in `Shutdown()`:

```cpp
void BoardHAL_MyPlatform::Shutdown() {
    // Disable peripherals
    DisableADC();
    DisableGPIO();
    
    printf("[%s] Shutdown complete\n", GetPlatformName());
}
```

### 4. Platform Detection

Use preprocessor guards:

```cpp
#ifdef LE_BOARD_MYPLATFORM

// ... HAL implementation ...

#endif // LE_BOARD_MYPLATFORM
```

### 5. Optional Features

Return `false` for unsupported operations:

```cpp
bool BoardHAL_MyPlatform::ReadAnalog(const GPIOPin& pin, float& value) {
    // This platform doesn't have ADC
    LogError("Analog I/O not supported on this platform");
    return false;
}
```

---

## Example: Complete Minimal HAL

Here's a minimal working HAL implementation:

```cpp
// le_BoardHAL_MyPlatform.hpp
#pragma once
#ifdef LE_BOARD_MYPLATFORM

#include "Device/HAL/le_BoardHAL.hpp"

namespace LogicElements {

class BoardHAL_MyPlatform : public BoardHAL {
public:
    bool Init() override;
    void Shutdown() override;
    const char* GetPlatformName() const override;
    
    bool ReadDigital(const GPIOPin& pin) override;
    void WriteDigital(const GPIOPin& pin, bool value) override;
    void ConfigureDigitalInput(const GPIOPin& pin) override;
    void ConfigureDigitalOutput(const GPIOPin& pin) override;
    
    bool ReadAnalog(const GPIOPin& pin, float& value) override;
    bool WriteAnalog(const GPIOPin& pin, float value) override;
    bool ConfigureAnalogInput(const GPIOPin& pin) override;
    bool ConfigureAnalogOutput(const GPIOPin& pin) override;
    
    void LogError(const char* message) override;
};

} // namespace LogicElements

#endif // LE_BOARD_MYPLATFORM
```

```cpp
// le_BoardHAL_MyPlatform.cpp
#ifdef LE_BOARD_MYPLATFORM

#include "Device/HAL/le_BoardHAL_MyPlatform.hpp"
#include <my_platform_gpio.h>  // Your platform's GPIO library

namespace LogicElements {

bool BoardHAL_MyPlatform::Init() {
    my_gpio_init();
    printf("[MyPlatform HAL] Initialized\n");
    return true;
}

void BoardHAL_MyPlatform::Shutdown() {
    my_gpio_deinit();
}

const char* BoardHAL_MyPlatform::GetPlatformName() const {
    return "MyPlatform";
}

bool BoardHAL_MyPlatform::ReadDigital(const GPIOPin& pin) {
    return my_gpio_read(pin.pin) == 1;
}

void BoardHAL_MyPlatform::WriteDigital(const GPIOPin& pin, bool value) {
    my_gpio_write(pin.pin, value ? 1 : 0);
}

void BoardHAL_MyPlatform::ConfigureDigitalInput(const GPIOPin& pin) {
    my_gpio_set_direction(pin.pin, GPIO_DIR_INPUT);
}

void BoardHAL_MyPlatform::ConfigureDigitalOutput(const GPIOPin& pin) {
    my_gpio_set_direction(pin.pin, GPIO_DIR_OUTPUT);
}

// Analog operations (return false if not supported)
bool BoardHAL_MyPlatform::ReadAnalog(const GPIOPin& pin, float& value) {
    return false;  // Not supported
}

bool BoardHAL_MyPlatform::WriteAnalog(const GPIOPin& pin, float value) {
    return false;  // Not supported
}

bool BoardHAL_MyPlatform::ConfigureAnalogInput(const GPIOPin& pin) {
    return false;
}

bool BoardHAL_MyPlatform::ConfigureAnalogOutput(const GPIOPin& pin) {
    return false;
}

void BoardHAL_MyPlatform::LogError(const char* message) {
    printf("[MyPlatform HAL ERROR] %s\n", message);
}

} // namespace LogicElements

#endif // LE_BOARD_MYPLATFORM
```

### Step 3: Update Build System

Add to `src/LogicElements/CMakeLists.txt`:

```cmake
elseif(LE_BOARD_PLATFORM STREQUAL "MyPlatform")
    list(APPEND CPP_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/Device/HAL/le_BoardHAL_MyPlatform.cpp")
    set(LE_BOARD_MYPLATFORM ON)
    # Link platform libraries if needed
    # find_package(my_platform_lib REQUIRED)
    # target_link_libraries(Logic_Elements PRIVATE my_platform_lib)
```

Add to platform strings:

```cmake
set_property(CACHE LE_BOARD_PLATFORM PROPERTY STRINGS 
             "Generic" "STM32" "RPI" "Arduino" "MyPlatform")
```

Add define to `le_Config.hpp.in`:

```cpp
#cmakedefine LE_BOARD_MYPLATFORM
```

### Step 4: Build and Test

```bash
cmake -DLE_BOARD_PLATFORM=MyPlatform -S . -B build
cmake --build build
```

---

## Platform-Specific Notes

### STM32

**GPIO Port Mapping**:
```
0 = GPIOA
1 = GPIOB
2 = GPIOC
3 = GPIOD
...
```

**Pin Numbers**: Use STM32 HAL defines (`GPIO_PIN_0` = 0x0001, `GPIO_PIN_1` = 0x0002, etc.)

**ADC/DAC**: User must configure ADC/DAC peripherals and implement in HAL

### Raspberry Pi

**GPIO Numbering**: Use BCM numbering (not physical pin numbers)

**Common Pins**:
- GPIO 17, 27, 22 (general purpose)
- GPIO 2, 3 (I2C)
- GPIO 14, 15 (UART)

**Analog I/O**: Requires external ADC/DAC via SPI (e.g., MCP3008, MCP4725)

**Permissions**: Requires sudo or pigpio daemon

### Arduino

**Pin Numbering**: Use Arduino pin numbers (13 = built-in LED, A0-A5 = analog inputs)

**Analog Input**: 10-bit ADC (0-1023), normalized to 0.0-1.0

**Analog Output**: PWM on pins with ~ symbol (8-bit, 0-255)

**Pin Modes**: Configured automatically by `pinMode()`

---

## FAQ

**Q: Can I use multiple HALs in one program?**  
A: No, only one HAL is compiled per build. Use CMake to select platform.

**Q: What if my platform doesn't have analog I/O?**  
A: Return `false` from analog methods. Board will handle gracefully.

**Q: Can I add custom methods to my HAL?**  
A: Yes! Extend the HAL interface as needed. Cast `BoardHAL*` to your type.

**Q: How do I handle interrupts?**  
A: Use `GPIOPin::context` to store ISR data. Implement in your HAL.

**Q: What about SPI, I2C, UART?**  
A: Extend HAL interface for your needs, or handle in element implementations.

---

## Contributing

When contributing a new platform HAL:

1. Test thoroughly on actual hardware
2. Document pin conventions
3. Include usage example
4. Add to this guide
5. Submit pull request

**Contact**: See main README for contribution guidelines

---

[⬆️ Back to Main README](README.md)

