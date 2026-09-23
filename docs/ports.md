# LogicElements Board Ports Guide

LogicElements is designed to run on **any microcontroller** with zero porting headaches.

---

## The 3-step integration

Adding LogicElements to your microcontroller project takes less than 5 minutes:

### Step 1: Copy Source Files into Your IDE Project
Copy the `src/runtime/` directory into your project. It contains only standard C files (`.c` and `.h`) with **zero external dependencies**:
- `le_types.h`, `le_process_image.h`, `le_opcodes.h`, `le_vm.h`, `le_loader.h`, `le_comms.h`, `le_complex.h`
- `le_process_image.c`, `le_opcodes.c`, `le_vm.c`, `le_loader.c`, `le_comms.c`

### Step 2: Choose Your Board Port
Select the port for your microcontroller family from `ports/` and add it to your project:
- **STM32** (`ports/stm32/le_port_stm32.c`) - STM32CubeIDE / Keil / IAR using STM32Cube HAL
- **Raspberry Pi Pico / RP2040** (`ports/rp2040/le_port_rp2040.c`) - Pico C/C++ SDK
- **Microchip AVR / Arduino** (`ports/avr/le_port_avr.c`) - ATmega328P / Arduino Uno / Nano / Mega
- **Custom MCU** (`ports/template/le_port.c`) - Blank template for any other architecture (ESP32, PIC, NXP, etc.)

> **Note**: For instructions on enabling live UART `.lebin` upload and flash storage, see the [Platform HAL & UART Upload Guide](../PLATFORM_GUIDE.md).

### Step 3: Call the Scan Loop in `main()`
In your application's `main.c`:

```c
#include "le_vm.h"
#include "le_loader.h"
#include "le_port.h"
#include "le_hal.h"

// Embed your compiled circuit (generated with le_compiler.py --header)
#include "my_circuit.h" 

int main(void)
{
    /* 1. Hardware setup */
    System_Init();
    const le_hal_t* hal = get_my_mcu_hal();
    hal->init();
    le_hal_set(hal);

    /* 2. LogicElements engine setup */
    le_vm_t vm;
    le_vm_init(&vm);
    le_loader_load(&vm, my_circuit_program, sizeof(my_circuit_program));

    /* 3. Deterministic PLC Scan Loop */
    while (1)
    {
        le_port_read_inputs(&vm.image);         // Read hardware pins -> %I
        le_vm_step(&vm, hal->get_time_ms()); // Execute logic
        le_port_write_outputs(&vm.image);        // Flush %Q -> hardware outputs

        Delay_ms(1); // 1 kHz scan frequency
    }
}
```

---

## Customizing pin mappings

Every port file includes a clean, isolated **Pin Mapping Table** at the top. You never have to modify the core engine to remap pins:

### Example: STM32 Pin Table
```c
static const stm32_pin_t INPUT_PINS[] = {
    { GPIOA, GPIO_PIN_0 },  // %I[0] -> Emergency Stop Button
    { GPIOC, GPIO_PIN_13 }, // %I[1] -> Start Button
};

static const stm32_pin_t OUTPUT_PINS[] = {
    { GPIOB, GPIO_PIN_0 },  // %Q[0] -> Motor Contactor Relay
    { GPIOB, GPIO_PIN_7 },  // %Q[1] -> Running Pilot Light
};
```

### Example: Raspberry Pi Pico (RP2040) Pin Table
```c
static const uint8_t INPUT_PINS[]  = { 14, 15, 16 }; // %I[0], %I[1], %I[2] on GP14, GP15, GP16
static const uint8_t OUTPUT_PINS[] = { 25, 18, 19 }; // %Q[0] on Onboard LED, %Q[1], %Q[2] on Relays
```
