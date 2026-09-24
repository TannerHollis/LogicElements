# LogicElements Board Ports Guide

LogicElements is designed to run on **any microcontroller** with zero porting headaches.

---

## The 3-step integration

Adding LogicElements to your microcontroller project takes less than 5 minutes:

### Step 1: Copy Source Files into Your IDE Project
Copy the `src/runtime/` directory into your project. It contains only standard C files (`.c` and `.h`) with **zero external dependencies**:
- `le_types.h`, `le_process_image.h`, `le_opcodes.h`, `le_vm.h`, `le_loader.h`, `le_comms.h`, `le_complex.h`, `le_hal.h`, `le_rt.h`
- `le_process_image.c`, `le_opcodes.c`, `le_vm.c`, `le_loader.c`, `le_comms.c`, `le_rt.c`

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

---

## Runtime capacity & feature switches (matching your `.leconfig`)

The runtime is sized by **build-time macros** (defaults in `src/runtime/include/le_types.h`).
Your port build should pin them to the values the board profile (`*.leconfig`) declares, so
the compiler and the loader agree:

| Runtime macro | Meaning |
| :--- | :--- |
| `LE_RAM_WORKSPACE_BYTES` | The one board-tunable **unified RAM pool** the loader carves into a packed **register arena** (sized from the program's declared register counts) followed by the **state blocks**. A program is rejected at load when `register arena + state image > LE_RAM_WORKSPACE_BYTES`. |
| `LE_MAX_DIGITAL_IN/OUT`, `LE_MAX_BOOL_REGS`, `LE_MAX_FLOATS`, `LE_MAX_INT_REGS`, `LE_MAX_ANALOG_IN` | Per-region ceilings. Must be **≥** the profile's `digital_inputs`, `digital_outputs`, `coils`, `floats`, `analog_inputs`. |
| `LE_NS_PER_ABSTRACT_CYCLE` | Port-calibrated worst-case **nanoseconds per compiler abstract cycle** (the `.leconfig` `ns_per_abstract_cycle`). This is the board's **cost model only**. The loader rejects any program whose `abstract_cycles × ns/cycle` exceeds the program's own declared scan period (`LE_ERR_TIMING_BUDGET`), so an unachievable circuit never loads. |

> **Fixed-rate model:** the **scan rate is circuit-owned** — each circuit declares `scan_rate_hz`
> (e.g. `"scan_rate_hz": 960`) and the compiler embeds it in the `.lebin` timing descriptor; the
> loader applies the period (`1e6/rate` µs) to the VM clock and verifies achievability against the
> board's `ns_per_abstract_cycle`. If a designer's chosen rate is unachievable, the compiler/loader
> reply with the **max achievable rate** so they simply pick a lower one. DSP/phasing/timers all
> derive their `dt` from this enforced period, so features like phasor extraction and filters run
> on a uniform sample grid. `ns_per_abstract_cycle` is a **safety estimate**, not precision — it
> only needs to be within a comfortable factor so pathological circuits are caught before they
> brick a board; calibrate it by measuring your MCU's worst case with a representative program.

Feature switches (each `0` removes the subsystem's state/opcodes — a board can be reduced all
the way down to boolean-only):

| Switch | Gated subsystem |
| :--- | :--- |
| `LE_ENABLE_PROTECTION` | Protection & control relays (phasor, overcurrent, diff87, dist21, …) |
| `LE_ENABLE_COMPLEX` | Complex arithmetic, `%C` registers & complex conversions |
| `LE_ENABLE_ANALOG` | Analog inputs `%AIN` and scaling |
| `LE_ENABLE_SERIAL_BUS` | I2C / SPI blocks |
| `LE_ENABLE_DSP` | DSP / filter blocks |

**Prerequisites are forced on automatically** — a higher layer cannot be built without its
lower layers, so enabling one turns its dependencies ON (the `le_types.h` config block
re-asserts the prerequisites at compile time):

```
LE_ENABLE_PROTECTION  =>  LE_ENABLE_COMPLEX  =>  LE_ENABLE_ANALOG
LE_ENABLE_DSP         =>  LE_ENABLE_ANALOG
```

- Complex phasors / `%C` require analog channels to acquire their signals → **complex implies analog**.
- Protection relays operate on phasors → **protection implies complex** (hence analog).
- DSP filters mix sampled (analog) channels → **dsp implies analog**.

The higher-layer switch is the master control: set `LE_ENABLE_PROTECTION=1`
(`LE_ENABLE_DSP=1`) and the complex/analog prerequisites are compiled in regardless of
what the lower switches say; set it to `0` to drop the whole subtree. Reaching a
**boolean-only** target still means setting *all five* switches to `0`.

Example: the ATmega328P firmware build sets
`-DLE_RAM_WORKSPACE_BYTES=512 -DLE_MAX_DIGITAL_IN=6 -DLE_MAX_DIGITAL_OUT=6
-DLE_MAX_BOOL_REGS=32 -DLE_MAX_FLOATS=16 -DLE_MAX_ANALOG_IN=6 -DLE_ENABLE_PROTECTION=0`
to match `ports/avr/atmega328p.leconfig`. Each reference port documents its exact flag set at
the top of its `le_port.h`.
