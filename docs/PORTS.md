# Custom Board Implementer's Guide

LogicElements is designed to run on **any bare-metal microcontroller or microprocessor** with zero external dependencies, no dynamic heap allocation (`malloc`), and complete architectural portability.

This document is the **single authoritative reference** for hardware engineers, firmware developers, and board adopters bringing up LogicElements on custom hardware.

---

## Table of Contents
1. [Architecture & Directory Layout](#1-architecture--directory-layout)
2. [5-Minute Fast-Track Bring-Up](#2-5-minute-fast-track-bring-up)
3. [Centralized Board Configuration (`le_board_config.h`)](#3-centralized-board-configuration-le_board_configh)
4. [Target Board Profile Specification (`.leconfig` JSON)](#4-target-board-profile-specification-leconfig-json)
5. [Physical Hardware I/O Integration](#5-physical-hardware-io-integration)
6. [Program Slot Storage (`storage_read` & `storage_write`)](#6-program-slot-storage-storage_read--storage_write)
7. [Operational Non-Volatile Memory (NVRAM / Flash Wear Leveling)](#7-operational-non-volatile-memory-nvram--flash-wear-leveling)
8. [Timing Model & Scan Rate Verification](#8-timing-model--scan-rate-verification)
9. [Live Interactive CLI & Program Uploads over UART](#9-live-interactive-cli--program-uploads-over-uart)
10. [Exposing Board Peripherals via Custom Nodes](#10-exposing-board-peripherals-via-custom-nodes)
11. [Board Bring-Up Verification Checklist](#11-board-bring-up-verification-checklist)

---

## 1. Architecture & Directory Layout

LogicElements strictly separates platform-independent virtual machine logic from microcontroller hardware peripherals.

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           APPLICATION / USER CODE                           │
│     Compiled Circuit Bytecode (.lebin) uploaded dynamically or embedded      │
└──────────────────────────────────────┬──────────────────────────────────────┘
                                       │
┌──────────────────────────────────────▼──────────────────────────────────────┐
│                    PLATFORM-INDEPENDENT RUNTIME ENGINE                      │
│                                                                             │
│   src/runtime/include/                      src/runtime/src/                │
│     ├── le_types.h (Core types & regions)     ├── le_vm.c (Instruction VM) │
│     ├── le_board_config.h (Master config)    ├── le_process_image.c        │
│     ├── le_vm.h (Virtual machine)             ├── le_loader.c (Verifier)    │
│     ├── le_process_image.h (Registers %B,%I)  ├── le_storage.c (Multi-slot) │
│     ├── le_storage.h (Slot management)        ├── le_nvram.c (Wear-leveling)│
│     ├── le_nvram.h (Persistent memory)        ├── le_ser.c (Event logging)  │
│     ├── le_hal.h (Hardware table interface)   ├── le_cli.c (Terminal CLI)   │
│     └── le_comms.h (Binary protocol)          └── le_comms.c (UART packets) │
└──────────────────────────────────────┬──────────────────────────────────────┘
                                       │ via le_hal_t function table
┌──────────────────────────────────────▼──────────────────────────────────────┐
│                       BOARD-SPECIFIC PORT IMPLEMENTATION                    │
│                                                                             │
│   ports/<your_board>/                                                       │
│     ├── le_port.h         Board constants & scan helper declarations        │
│     ├── le_port.c         HAL callbacks & hardware driver bindings          │
│     └── <board>.leconfig  Machine-readable board profile JSON               │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Files to Add to Your Microcontroller Project
1. Copy all source files from `src/runtime/include/` and `src/runtime/src/` into your IDE project (Keil, IAR, STM32CubeIDE, MCUXpresso, VS Code, ESP-IDF, etc.).
2. Copy `ports/template/le_port.h` and `ports/template/le_port.c` to your project and rename them to match your hardware (e.g. `ports/my_board/`).

---

## 2. 5-Minute Fast-Track Bring-Up

To get running immediately before writing flash or peripheral drivers, use the **In-Memory RAM Mirror** defaults.

### Minimal `main.c`
```c
#include "le_vm.h"
#include "le_loader.h"
#include "le_port.h"
#include "le_hal.h"

int main(void)
{
    /* 1. Microcontroller Hardware Init */
    bsp_system_init();

    /* 2. Register Board HAL Table */
    const le_hal_t* hal = get_my_mcu_hal();
    if (hal->init) hal->init();
    le_hal_set(hal);

    /* 3. Initialize Scan Helpers & NVRAM */
    le_port_init();

    /* 4. LogicElements Virtual Machine */
    le_vm_t vm;
    le_vm_init(&vm);

    /* Embed a pre-compiled circuit for initial testing (or load from slot 0) */
    #include "test_circuit.h" // Produced by: le_compile circuit.json --header -o test_circuit.h
    le_loader_load(&vm, test_circuit_program, sizeof(test_circuit_program));

    /* 5. Deterministic PLC Execution Loop */
    while (1)
    {
        uint32_t now_ms = hal->get_time_ms();

        /* a. Sample physical pins into Process Image inputs (%DIN, %AIN) */
        le_port_read_inputs(&vm.image);

        /* b. Step virtual machine logic */
        le_vm_step(&vm, now_ms);

        /* c. Flush Process Image outputs (%DOUT, %AOUT) to physical pins */
        le_port_write_outputs(&vm.image);

        /* d. Optional: process background UART communications */
        // le_port_process_uart();

        bsp_delay_ms(1); // Target scan period
    }
}
```

---

## 3. Centralized Board Configuration (`le_board_config.h`)

All compile-time limits, memory pools, and feature flags are centralized in [`src/runtime/include/le_board_config.h`](file:///c:/Users/tanne/OneDrive/Documents/GitHub/LogicElements/src/runtime/include/le_board_config.h).

Target boards override defaults by passing compiler `-D` flags or defining `-DLE_BOARD_CONFIG_FILE="path/to/my_board_config.h"`.

### Key Parameters & Their Roles

| Macro | Default | Description |
| :--- | :--- | :--- |
| **`LE_MAX_DIGITAL_IN`** | `64` | Maximum physical digital input pins (%DIN) on the board. |
| **`LE_MAX_DIGITAL_OUT`** | `64` | Maximum physical digital output pins (%DOUT) on the board. |
| **`LE_MAX_ANALOG_IN`** | `16` | Maximum physical ADC channels (%AIN) on the board. |
| **`LE_MAX_ANALOG_OUT`** | `16` | Maximum physical DAC / PWM channels (%AOUT) on the board. |
| **`LE_RAM_WORKSPACE_BYTES`** | `2048` | **Unified RAM Pool**. The loader dynamically packs all internal registers (%B, %I, %F, %C) and element state blocks (timers, relays, filters) within this pool. |
| **`LE_MAX_CONFIG_SLOTS`** | `3` | Number of user-visible program bytecode slots stored in flash. |
| **`LE_SLOT_SIZE_BYTES`** | `2048` | Maximum compiled `.lebin` binary program size per slot. |
| **`LE_NVRAM_TOTAL_BYTES`** | `16384` | Total logical NVRAM capacity (16 KB default). |
| **`LE_NVRAM_BLOCK_SIZE`** | `4096` | Physical erase block / sector size for Flash (typically 4096 bytes). |
| **`LE_NVRAM_PAGE_SIZE`** | `256` | Physical write programming page size for Flash / EEPROM (typically 256 bytes). |
| **`LE_NVRAM_REGS_BYTES`** | `1024` | **Retentive register capacity per configuration slot**. |
| **`LE_NVRAM_SER_BYTES`** | `3072` | Hardware-wide circular Sequence of Events (SER) log buffer size. |
| **`LE_NVRAM_EVENT_REC_BYTES`** | Dynamic | Event Recorder / Oscillography waveform capture buffer size. |
| **`LE_NS_PER_ABSTRACT_CYCLE`** | `600` | Port-calibrated worst-case execution time (nanoseconds) per compiler abstract cycle. |

### Note on Internal Registers (No Static Limits!)
Internal process image registers (`%B` boolean, `%I` integer, `%F` float, `%C` complex) **do not have arbitrary static limits** (e.g. `LE_MAX_BOOL_REGS` has been removed). Register memory is packed dynamically at load time inside `LE_RAM_WORKSPACE_BYTES`. The only constraint is:
$$\text{Packed Register Arena} + \text{State Blocks} \le \text{LE\_RAM\_WORKSPACE\_BYTES}$$

### Feature Switches & Automatic Cascades
Disable subsystems not needed on smaller MCUs to save code and RAM:
```c
#define LE_ENABLE_PROTECTION    1  // P&C relays (OVERCURRENT, DIST_21, DIFF_87)
#define LE_ENABLE_COMPLEX       1  // Complex arithmetic, phasors & %C registers
#define LE_ENABLE_ANALOG        1  // Analog channels (%AIN, %AOUT)
#define LE_ENABLE_SERIAL_BUS    1  // I2C and SPI block opcodes
#define LE_ENABLE_DSP           1  // DSP filters (LPF, BIQUAD, RMS, MEDIAN)
```
*Dependency cascades are automatic*: If `LE_ENABLE_PROTECTION` is `1`, `LE_ENABLE_COMPLEX` and `LE_ENABLE_ANALOG` are automatically forced to `1`.

---

## 4. Target Board Profile Specification (`.leconfig` JSON)

The `.leconfig` file tells the compiler and desktop design tools the exact limits, pin aliases, and capabilities of your board so designers can never generate invalid logic.

Place `<your_board>.leconfig` in `ports/<your_board>/`:

```json
{
  "device": {
    "name": "My Custom Industrial Controller",
    "firmware_version": "1.0",
    "protocol_version": 3
  },
  "limits": {
    "digital_inputs": 16,
    "digital_outputs": 16,
    "analog_inputs": 4,
    "analog_outputs": 2,
    "workspace_bytes": 16384,
    "config_slots": 3,
    "slot_size_bytes": 4096,
    "ns_per_abstract_cycle": 350,
    "nvram_total_bytes": 16384,
    "nvram_block_size": 4096,
    "nvram_page_size": 256,
    "nvram_regs_bytes": 1024,
    "nvram_ser_bytes": 3072,
    "nvram_event_rec_bytes": 10176
  },
  "features": {
    "protection": true,
    "complex": true,
    "analog": true,
    "dsp": true,
    "serial_bus": true
  },
  "pin_map": {
    "inputs": {
      "%I0": {"alias": "ESTOP", "pin": "PA0", "desc": "Emergency Stop (NC)"},
      "%I1": {"alias": "START_PB", "pin": "PA1", "desc": "Start Pushbutton (NO)"}
    },
    "outputs": {
      "%Q0": {"alias": "MOTOR_CONTACTOR", "pin": "PB0", "desc": "Main Motor Contactor"},
      "%Q1": {"alias": "RUN_LAMP", "pin": "PB1", "desc": "Running Pilot Lamp"}
    },
    "analog": {
      "%AIN0": {"alias": "LINE_CURRENT", "pin": "PC0", "desc": "CT Analog Input (0-3.3V)"}
    }
  },
  "custom_nodes": []
}
```

> **Tip:** You can dump this JSON directly from your board at runtime by connecting over UART and running the CLI command:
> ```bash
> caps
> ```

---

## 5. Physical Hardware I/O Integration

In `le_port.c`, create pin tables mapping process-image indices to microcontroller pins:

```c
typedef struct {
    GPIO_TypeDef* port;
    uint16_t      pin;
} bsp_gpio_pin_t;

static const bsp_gpio_pin_t INPUT_MAP[] = {
    { GPIOA, GPIO_PIN_0 }, // %DIN 0
    { GPIOA, GPIO_PIN_1 }, // %DIN 1
};
#define NUM_INPUTS (sizeof(INPUT_MAP) / sizeof(INPUT_MAP[0]))

static const bsp_gpio_pin_t OUTPUT_MAP[] = {
    { GPIOB, GPIO_PIN_0 }, // %DOUT 0
    { GPIOB, GPIO_PIN_1 }, // %DOUT 1
};
#define NUM_OUTPUTS (sizeof(OUTPUT_MAP) / sizeof(OUTPUT_MAP[0]))
```

### Implementing Scan Helpers
```c
void le_port_read_inputs(le_process_image_t* img)
{
    if (!img) return;

    /* 1. Sample Digital Inputs (%DIN) */
    for (uint8_t i = 0; i < NUM_INPUTS; i++) {
        bool state = (HAL_GPIO_ReadPin(INPUT_MAP[i].port, INPUT_MAP[i].pin) == GPIO_PIN_SET);
        le_process_image_set_bool(img, LE_ADDR_MAKE_DIN(i), state);
    }

    /* 2. Sample Analog Inputs (%AIN) */
    #if LE_ENABLE_ANALOG
    for (uint16_t ch = 0; ch < img->ain_count; ch++) {
        float volts = bsp_adc_read_voltage(ch); // e.g. 0.0 to 3.3V
        le_pi_set_ain(img, ch, volts);
    }
    #endif
}

void le_port_write_outputs(const le_process_image_t* img)
{
    if (!img) return;

    /* 1. Flush Digital Outputs (%DOUT) */
    for (uint8_t i = 0; i < NUM_OUTPUTS; i++) {
        bool state = le_process_image_get_bool(img, LE_ADDR_MAKE_DOUT(i));
        HAL_GPIO_WritePin(OUTPUT_MAP[i].port, OUTPUT_MAP[i].pin, state ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }

    /* 2. Flush Analog Outputs (%AOUT) to DAC / PWM channels */
    #if LE_ENABLE_ANALOG
    for (uint16_t ch = 0; ch < img->aout_count; ch++) {
        float val = le_pi_get_aout(img, ch);
        bsp_dac_write_voltage(ch, val);
    }
    #endif
}

void le_port_deenergize_outputs(void)
{
    /* Safety Interlock: de-energize all physical outputs when VM stops */
    for (uint8_t i = 0; i < NUM_OUTPUTS; i++) {
        HAL_GPIO_WritePin(OUTPUT_MAP[i].port, OUTPUT_MAP[i].pin, GPIO_PIN_RESET);
    }
    #if LE_ENABLE_ANALOG
    bsp_dac_deenergize_all();
    #endif
}
```

---

## 6. Program Slot Storage (`storage_read` & `storage_write`)

LogicElements stores compiled `.lebin` programs in non-volatile flash partitions called **Configuration Slots**.

### The Phantom Staging Slot Rule
To prevent network drops or bad serial transmissions from bricking a live controller, the storage manager requires:
$$\text{Physical Flash Slots Reserved} = \text{LE\_MAX\_CONFIG\_SLOTS} + 1$$
The extra partition at index `LE_MAX_CONFIG_SLOTS` is the **phantom scratch slot**. Incoming programs stream into the phantom slot first. Only when the full program is received and passes full IEEE 802.3 CRC32 verification is it atomically committed to the target user slot.

### Total Flash Required for Program Bytecode:
$$\text{Storage Total Bytes} = (\text{LE\_MAX\_CONFIG\_SLOTS} + 1) \times \text{LE\_SLOT\_SIZE\_BYTES}$$
*(Example: 3 slots of 4096 B each require $(3 + 1) \times 4096 = 16{,}384\text{ B}$ of flash).*

### Implementing HAL Callbacks
In `le_port.c`:
```c
#define FLASH_STORAGE_BASE  0x08060000UL // Physical flash partition start

bool le_port_storage_read(uint32_t offset, uint8_t* buf, size_t len)
{
    if (offset + len > LE_PORT_STORAGE_TOTAL_BYTES) return false;
    memcpy(buf, (const void*)(FLASH_STORAGE_BASE + offset), len);
    return true;
}

bool le_port_storage_write(uint32_t offset, const uint8_t* buf, size_t len)
{
    if (offset + len > LE_PORT_STORAGE_TOTAL_BYTES) return false;
    uint32_t target_addr = FLASH_STORAGE_BASE + offset;

    // Erase sector if write aligns with sector boundary
    if ((offset % 4096) == 0) {
        bsp_flash_erase_sector(target_addr);
    }
    return bsp_flash_program(target_addr, buf, len);
}
```

---

## 7. Operational Non-Volatile Memory (NVRAM / Flash Wear Leveling)

LogicElements manages three kinds of non-volatile operational data:
1. **Retentive Process Image Registers** (`%B`, `%I`, `%F`, `%C`):
   Setpoints, counters, totals, and calibration factors.
   **Crucial Architectural Rule:** Retentive registers are **isolated per configuration slot** (`LE_NVRAM_REGS_BYTES` per slot). Loading or switching between Slot 0 and Slot 1 will **never** overwrite or corrupt each other's setpoints.
2. **Sequence of Events Recorder (SER Log)**:
   Circular FIFO logging physical breaker trips and contact transitions with microsecond timestamps (hardware-wide, `LE_NVRAM_SER_BYTES`).
3. **Event Recorder (Oscillography)**:
   Waveform fault capture buffers saving analog pre-fault and post-fault samples (hardware-wide, `LE_NVRAM_EVENT_REC_BYTES`).

You can implement NVRAM in one of two ways:

---

### Option A: Plain Extra Flash Memory (Built-In Wear Leveling)
If your MCU has spare flash sectors (e.g. the last 16 KB or 32 KB of on-chip flash), use the runtime's **built-in log-structured journal**.

The runtime handles all sector rotation, wear leveling, sequence tracking, and compaction. **The board implementer only provides 3 raw hardware operations**:

```c
static bool my_flash_erase(uint32_t sector_idx, uint32_t phys_addr) {
    (void)sector_idx;
    return bsp_flash_erase_sector(phys_addr);       // MCU SDK sector erase (e.g. 4096 B)
}

static bool my_flash_program(uint32_t phys_addr, const uint8_t* data, size_t len) {
    return bsp_flash_page_program(phys_addr, data, len); // MCU SDK page program (e.g. 256 B)
}

static bool my_flash_read(uint32_t phys_addr, uint8_t* data, size_t len) {
    return bsp_flash_read(phys_addr, data, len);   // Direct memcpy or SPI read
}

/* In le_port_init(): */
static const le_flash_wl_driver_t s_flash_wl_driver = {
    .flash_base_offset = 0x08070000UL,       // Base address of reserved flash pool
    .sector_size       = LE_NVRAM_BLOCK_SIZE, // 4096 bytes
    .page_size         = LE_NVRAM_PAGE_SIZE,  // 256 bytes
    .sector_count      = 4,                   // e.g. 4 sectors = 16 KB rotating pool
    .erase_sector      = my_flash_erase,
    .program           = my_flash_program,
    .read              = my_flash_read,
};

void le_port_init(void) {
    le_nvram_init_flash(&s_flash_wl_driver);
}
```

In the `le_hal_t` table:
```c
const le_hal_t s_my_mcu_hal = {
    // ...
    .nvram_read  = (bool (*)(uint32_t, uint8_t*, size_t))le_nvram_read,
    .nvram_write = (bool (*)(uint32_t, const uint8_t*, size_t))le_nvram_write,
    .nvram_erase = (bool (*)(uint32_t, size_t))le_nvram_erase,
    // ...
};
```

---

### Option B: External Byte-Addressable NVRAM (SPI FRAM, MRAM, EEPROM)
If using an external SPI/I2C FRAM (e.g. MB85RS64, FM25V02, battery SRAM):

```c
bool le_port_nvram_read(uint32_t offset, void* buf, size_t len) {
    return bsp_spi_fram_read(offset, buf, len);
}

bool le_port_nvram_write(uint32_t offset, const void* buf, size_t len) {
    return bsp_spi_fram_write(offset, buf, len);
}

bool le_port_nvram_erase(uint32_t offset, size_t len) {
    uint8_t zeros[64] = {0};
    // ... zero bytes on device ...
    return true;
}

void le_port_init(void) {
    bsp_spi_fram_init();
    le_nvram_init_direct(); // Automatically pre-loads cache from external chip on boot
}
```

In the `le_hal_t` table:
```c
const le_hal_t s_my_mcu_hal = {
    // ...
    .nvram_read  = (bool (*)(uint32_t, uint8_t*, size_t))le_port_nvram_read,
    .nvram_write = (bool (*)(uint32_t, const uint8_t*, size_t))le_port_nvram_write,
    .nvram_erase = (bool (*)(uint32_t, size_t))le_port_nvram_erase,
    // ...
};
```

---

## 8. Timing Model & Scan Rate Verification

LogicElements enforces **deterministic timing**.

### The Circuit-Owned Scan Rate Rule
The scan rate is declared by the circuit designer in their schematic (e.g. `"scan_rate_hz": 1000` = 1 ms period).

The board only provides a **calibrated cost model**:
- **`LE_NS_PER_ABSTRACT_CYCLE`**: The estimated worst-case execution time in nanoseconds per compiler instruction cycle.
  - Fast Cortex-M7 @ 480 MHz: `~100 ns`
  - Cortex-M33 @ 150 MHz: `~250 ns`
  - Cortex-M4 @ 84 MHz: `~450 ns`
  - Cortex-M0+ / slow Flash: `~600 - 800 ns`

At program load time, `le_loader` calculates:
$$\text{Estimated Scan Time} = \text{Abstract Cycles} \times \text{LE\_NS\_PER\_ABSTRACT\_CYCLE} \times 1.25\text{ (Safety Margin)}$$
If the estimated scan time exceeds the declared scan period ($1 / \text{scan\_rate\_hz}$), the loader **rejects the program with `LE_ERR_TIMING_BUDGET`**. A circuit that would cause timing jitter or watchdog timeouts is never allowed to run.

---

## 9. Live Interactive CLI & Program Uploads over UART

LogicElements includes an interactive command-line interface and a framed binary communications protocol over UART.

### Feeding Characters from Your UART RX ISR / DMA Ring Buffer
In your main loop or communications task:
```c
void le_port_process_uart(le_cli_t* cli, le_comms_t* comms)
{
    while (bsp_uart_has_data()) {
        uint8_t byte = bsp_uart_read_byte();

        // 1. Check for framed binary packet header (0xAA)
        if (le_comms_is_packet_byte(byte)) {
            le_comms_process_byte(comms, byte);
        } else {
            // 2. Otherwise pass to interactive human CLI
            le_cli_process_char(cli, (char)byte);
        }
    }
}
```

### CLI Commands Available to Field Engineers
When connected via PuTTY, TeraTerm, or `le term`:
- `help` — Show available console commands.
- `info` — Display firmware version, platform name, physical I/O pin counts, RAM workspace.
- `status` — Show VM run/stop state, active configuration slot, cycle count, scan period, and timing margin.
- `caps` — Dump machine-readable `.leconfig` JSON profile for desktop IDE tools.
- `slots` — List all stored configuration slots with instruction counts, byte sizes, and CRC32 status.
- `select <slot>` — Select active program slot and load its retentive register setpoints.
- `start` / `stop` — Start or halt virtual machine execution.
- `regs` — Display live internal registers (`%B`, `%I`, `%F`, `%C`).
- `upload <slot> <size>` — Upload new compiled `.lebin` program using XMODEM-CRC or hex stream into the phantom staging slot.

---

## 10. Exposing Board Peripherals via Custom Nodes

If your board has unique hardware peripherals (e.g. RP2350 PIO stepper motor, CAN transceiver, hardware cryptographic accelerator, or OLED display), you can expose them as visual blocks in circuit diagrams.

1. Declare the node in your board's `.leconfig`:
```json
"custom_nodes": [
  {
    "type_id": "BOARD_RELAY_DRIVER",
    "display_name": "Relay Driver",
    "category": "Actuators",
    "function_id": 1,
    "inputs": [{"name": "Energize", "type": "bool"}],
    "outputs": [{"name": "TripFeedback", "type": "bool"}]
  }
]
```

2. Implement `hal->ext_call` in `le_port.c`:
```c
static le_status_t port_ext_call(uint8_t func_id, const uint16_t* args,
                                 int in_count, int out_count, le_process_image_t* img)
{
    if (func_id == 1) { // BOARD_RELAY_DRIVER
        bool energize = le_process_image_get_bool(img, args[0]);
        bsp_set_physical_relay(energize);
        bool feedback = bsp_read_aux_contact();
        le_process_image_set_bool(img, args[1], feedback);
        return LE_OK;
    }
    return LE_ERR_OPCODE_UNKNOWN;
}
```

---

## 11. Board Bring-Up Verification Checklist

Use this checklist to verify your port before deploying to production:

- [ ] **Compilation**: Core runtime (`src/runtime/`) and your port compile with 0 warnings.
- [ ] **RAM Workspace**: `LE_RAM_WORKSPACE_BYTES` is sized comfortably within MCU SRAM.
- [ ] **Pin Aliases**: All inputs in `INPUT_MAP` and outputs in `OUTPUT_MAP` match your `.leconfig`.
- [ ] **Safety Interlock**: Stopping the VM or calling `le_port_deenergize_outputs()` zeroes all physical outputs.
- [ ] **Program Storage**: Flash sector erase and programming work for all $(N+1)$ slots.
- [ ] **Phantom Slot**: Uploading a corrupted binary into the phantom slot is rejected with bad CRC without touching the active program.
- [ ] **NVRAM Partitioning**: Saving setpoints in Slot 0 does not overwrite or corrupt Slot 1 setpoints.
- [ ] **Flash Wear Leveling (if using Flash)**: Circular sector rollover functions cleanly across power cycles.
- [ ] **Timing Achievability**: Loading a test program at 1 kHz confirms estimated execution time fits within 1000 µs.
- [ ] **CLI Inspection**: Running `caps` over UART prints valid JSON matching your `.leconfig`.
