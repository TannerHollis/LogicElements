# Custom Board Implementer's Guide

LogicElements is designed to run on **any bare-metal microcontroller or microprocessor** with zero external dependencies, no dynamic heap allocation (`malloc`), and complete architectural portability.

This document is the **single authoritative reference** for hardware engineers, firmware developers, and board adopters bringing up LogicElements on custom hardware.

---

## Table of Contents
1. [Architecture & Directory Layout](#1-architecture--directory-layout)
2. [5-Minute Fast-Track Bring-Up](#2-5-minute-fast-track-bring-up)
3. [Centralized Configuration (`le_board_config.h` & `le_config.h`)](#3-centralized-configuration-le_board_configh--le_configh)
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
│     ├── le_config.h (Engine defaults)         ├── le_process_image.c        │
│     ├── le_vm.h (Virtual machine)             ├── le_loader.c (Verifier)    │
│     ├── le_process_image.h (Registers %B,%I)  ├── le_storage.c (Multi-slot) │
│     ├── le_storage.h (Slot management)        ├── le_nvram.c (Wear-leveling)│
│     ├── le_nvram.h (Persistent memory)        ├── le_ser.c (Event logging)  │
│     ├── le_hal.h (Hardware table interface)   ├── le_cli.c (Terminal CLI)   │
│     ├── le_ringbuf.h (FIFO buffers)           ├── le_comms.c (UART packets) │
│     └── le_comms.h (Binary protocol & pump)   └── le_ringbuf.c              │
└──────────────────────────────────────┬──────────────────────────────────────┘
                                       │ via le_hal_t function table
┌──────────────────────────────────────▼──────────────────────────────────────┐
│                       BOARD-SPECIFIC PORT IMPLEMENTATION                    │
│                                                                             │
│   include/                                  src/                            │
│     ├── le_board_config.h (Hardware limits)   ├── le_board_config.c (Flash) │
│     └── le_port.h (5-function scan contract)  └── le_port.c (HAL drivers)   │
│   profiles/                                 demos/                          │
│     └── <board>.leconfig (Profile JSON)       └── test_circuit.h (ROM demo) │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Files to Add to Your Microcontroller Project
1. Add all core runtime files from `src/runtime/include/` and `src/runtime/src/` to your build system (CMake, Make, Keil, IAR, STM32CubeIDE, MCUXpresso, VS Code, ESP-IDF, etc.).
2. Copy the reference files from `ports/template/` into your project:
   - `include/le_board_config.h` & `src/le_board_config.c`: Board hardware geometry, flash sectors, and capacity limits.
   - `include/le_port.h` & `src/le_port.c`: HAL callback drivers and scan lifecycle synchronization.

### Understanding `le_port.h` vs. `le_port.c` (Public Scan Loop vs. Private HAL Callbacks)
A common question when porting LogicElements is:
*Why does `le_port.c` define dozens of functions (`port_gpio_read`, `port_adc_read`, `port_storage_read`, `port_nvram_read`, `port_uart_write`), but `le_port.h` only declares `get_my_mcu_hal()`, `le_port_init()`, `le_port_read_inputs()`, `le_port_write_outputs()`, and `le_port_deenergize_outputs()`?*

- **`le_port.h` is the Public Board Interface for `main.c`:**
  It exposes only the high-level scan-loop lifecycle functions needed by your microcontroller's `main()` loop to initialize hardware and synchronize physical I/O at the start and end of each PLC scan cycle.
- **`le_port.c` contains Private Driver Callbacks for `le_hal_t`:**
  All peripheral callbacks (`port_gpio_*`, `port_storage_*`, `port_nvram_*`, `port_uart_*`, `port_i2c_*`, `port_spi_*`) are declared `static`. They are **never called directly by application code or `main.c`**. Instead, they are packed into the `s_my_mcu_hal` struct and invoked polymorphically by the runtime VM, storage manager, comms, and CLI via the global `g_le_hal` pointer. Keeping them `static` guarantees process image consistency, avoids namespace pollution, and keeps the port interface clean.

---

## 2. 5-Minute Fast-Track Bring-Up

To get running immediately before writing flash or peripheral drivers, use the **In-Memory RAM Mirror** defaults.

### Minimal `main.c`
```c
#include "le_vm.h"
#include "le_loader.h"
#include "le_storage.h"
#include "le_comms.h"
#include "le_cli.h"
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

    /* 4. LogicElements Virtual Machine & Storage */
    le_vm_t vm;
    le_vm_init(&vm);
    le_storage_t storage;
    le_storage_init(&storage);

    /* 5. Communications & Interactive CLI */
    le_comms_t comms;
    le_comms_init(&comms, &vm, &storage);
    le_cli_t cli;
    le_cli_init(&cli, &vm, &storage);

    /* 6. Dual-Boot Autostart: Boot from Slot 0 or fallback to embedded demo */
    #include "test_circuit.h" // Produced by: le compile circuit.json --header -o test_circuit.h
    const char* boot_source = NULL;
    le_storage_autostart_or_demo(&vm, &storage, 0, test_circuit_program, sizeof(test_circuit_program), &boot_source);

    /* 7. Deterministic PLC Execution Loop */
    while (1)
    {
        uint32_t now_us = (uint32_t)hal->get_time_us();

        /* Non-blocking fixed-rate scan gate (drift-free): */
        if (le_vm_scan_due(&vm, now_us)) {
            /* a. Fresh sample of physical pins into Process Image inputs (%DIN, %AIN) */
            le_port_read_inputs(&vm.image);

            /* b. Step virtual machine logic at exact boundary */
            le_vm_step_us(&vm, now_us);

            /* c. Immediate output actuation (%DOUT, %AOUT) */
            le_port_write_outputs(&vm.image);
        }

        /* d. Background work runs during slack time (Binary protocol & Interactive CLI) */
        le_comms_cli_pump(&comms, &cli, hal);
    }
}
```

---

## 3. Centralized Configuration (`le_board_config.h` & `le_config.h`)

All default compile-time limits, memory pools, core topologies, and feature flags are centralized in [`src/runtime/include/le_config.h`](file:///c:/Users/tanne/OneDrive/Documents/GitHub/LogicElements/src/runtime/include/le_config.h).

Each target board overrides these defaults by providing its own `include/le_board_config.h`:
1. Passing compiler `-D` flags in CMake / Makefile.
2. Defining `-DLE_CONFIG_FILE="le_board_config.h"`.
3. Customizing the reference template from `ports/template/le_board_config.h`.

### Master Configuration Domains

The master configuration is organized into 8 distinct architectural domains:

#### 1. Processor Core & Execution Topology
- **`LE_CORE_MODE`**: `LE_CORE_SINGLE` (0, default), `LE_CORE_AMP` (1, asymmetric multiprocessing e.g. RP2040/RP2350 dual-core), `LE_CORE_SMP` (2, RTOS multithreading).
- **`LE_AMP_WATCHDOG_TIMEOUT_US`**: Inter-core scan completion watchdog timeout in microseconds (default `50000`).
- **`LE_EXEC_MODE`**: `LE_EXEC_IN_PLACE` (0, execute directly from flash ROM), `LE_EXEC_IN_RAM` (1, shadow program into SRAM).
- **`LE_DISPATCH_MODE`**: `LE_DISPATCH_AUTO` (0), `LE_DISPATCH_SWITCH` (1), `LE_DISPATCH_DIRECT` (2, computed gotos).

#### 2. Memory Hierarchy & RAM Partitioning
- **`LE_RAM_WORKSPACE_BYTES`**: Unified dynamic RAM workspace (default `2048` bytes). The loader dynamically packs all internal registers (%B, %I, %F, %C) and element state blocks (timers, relays, filters) within this pool. No static per-type register limits!
- **`LE_STORAGE_PARTITION_MODE`**: Slot RAM buffer strategy:
  - `LE_STORAGE_PARTITION_ZERO_RAM` (0): 0 bytes RAM. Slots read directly from Flash XIP.
  - `LE_STORAGE_PARTITION_SINGLE_CACHE` (1): 1 slot buffer in RAM (phantom upload buffer).
  - `LE_STORAGE_PARTITION_FULL_RAM` (2): All slots mirrored in RAM (simulation / desktop).
- **`LE_STORAGE_BACKEND`**: `LE_STORAGE_FLASH` (0, on-chip/SPI flash), `LE_STORAGE_RAM` (1, RAM mirror).

#### 3. Program Storage & Slots
- **`LE_MAX_CONFIG_SLOTS`**: Number of user-visible program bytecode slots (default `3`).
- **`LE_SLOT_SIZE_BYTES`**: Maximum compiled `.lebin` binary program size per slot (default `2048`).

#### 4. Operational NVRAM & Flash Wear-Leveling
- **`LE_NVRAM_BACKEND`**: `LE_NVRAM_FLASH` (0, wear-leveling journal), `LE_NVRAM_EXTERNAL` (1, direct SPI FRAM/MRAM), `LE_NVRAM_RAM` (2, RAM mirror).
- **`LE_NVRAM_TOTAL_BYTES`**: Total physical NVRAM capacity (default `16384` bytes).
- **`LE_NVRAM_BLOCK_SIZE`**: Flash erase sector size (default `4096` bytes).
- **`LE_NVRAM_PAGE_SIZE`**: Flash/EEPROM programming page size (default `256` bytes).

> [!NOTE]
> **Circuit-Driven Dynamic NVRAM Allocation**: Modern LogicElements does not impose rigid compile-time partition limits for retentive registers, Sequence of Events (SER) logs, or oscillography event records. Instead, the loaded `.lebin` circuit declares its exact retentive register count, SER depth, and oscillography event records. The runtime dynamically packs them into `LE_NVRAM_TOTAL_BYTES` at load time, recording dynamic offsets in the NVRAM manifest.

#### 5. Physical Hardware I/O Capacity
- **`LE_MAX_DIGITAL_IN`**: Maximum physical digital input pins (`%DIN`, default `64`).
- **`LE_MAX_DIGITAL_OUT`**: Maximum physical digital output pins (`%DOUT`, default `64`).
- **`LE_MAX_ANALOG_IN`**: Maximum physical ADC channels (`%AIN`, default `16`).
- **`LE_MAX_ANALOG_OUT`**: Maximum physical DAC/PWM channels (`%AOUT`, default `16`).

#### 6. Subsystem Feature Switches & Automatic Cascades
Disable subsystems not needed on constrained MCUs to strip unused opcodes and save Flash/RAM (see the **[Runtime VM Size and Feature Breakdown](ARCHITECTURE.md#runtime-vm-size-and-feature-breakdown)** for exact Flash and static RAM costs per feature):
```c
#define LE_ENABLE_PROTECTION    1  // P&C relays (OVERCURRENT, DIST_21, DIFF_87)
#define LE_ENABLE_COMPLEX       1  // Complex arithmetic, phasors & %C registers
#define LE_ENABLE_ANALOG        1  // Analog channels (%AIN, %AOUT)
#define LE_ENABLE_DSP           1  // DSP filters (DFT, BIQUAD, RMS, MEDIAN)
#define LE_ENABLE_SERIAL_BUS    1  // Hardware I2C / SPI opcodes
#define LE_ENABLE_CRYPTO        0  // Ed25519 / HMAC program verification
#define LE_ENABLE_FPU           1  // Hardware floating-point unit
```
*Dependency cascades are automatic*: If `LE_ENABLE_PROTECTION` is `1`, `LE_ENABLE_COMPLEX` and `LE_ENABLE_ANALOG` are automatically enabled.

#### 7. Math & Numerical Stability
- **`LE_DIV_BY_ZERO_POLICY`**: `LE_DIV_POLICY_SATURATE` (0, default), `LE_DIV_POLICY_ZERO` (1), `LE_DIV_POLICY_NAN` (2).
- **`LE_FLUSH_NAN_TO_ZERO`**: Sanitize NaN/Inf inputs to 0.0 (default `1`).
- **`LE_MAX_RAW_SAMPLES`**: Raw sample buffer for high-rate acquisition / DFT (default `64`).
- **`LE_MOVING_AVG_MAX_WINDOW`**, **`LE_RMS_MAX_WINDOW`**, **`LE_MAX_MEDIAN_WINDOW`**, **`LE_MAX_LUT_POINTS`**.

#### 8. Timing Model & Communications
- **`LE_NS_PER_ABSTRACT_CYCLE`**: Port-calibrated worst-case execution time (nanoseconds) per abstract cycle (default `600`).
- **`LE_DEFAULT_SCAN_DT_SEC`**: Fallback scan delta-time in seconds (default `0.001f` = 1 kHz).
- **`LE_COMMS_MAX_PAYLOAD`**: Maximum packet payload size (default `128` bytes).
- **`LE_COMMS_SYNC_BYTE`**: Framing synchronization byte (default `0xAA`).
- **`LE_COMMS_CLI_RX_CHUNK_SIZE`**: Batch reception chunk size for `le_comms_cli_pump` (default `32` bytes).
- **`LE_RINGBUF_DEFAULT_SIZE`**: Default capacity for `le_ringbuf_t` FIFO buffers (default `256` bytes).

---

## 4. Target Board Profile Specification (`.leconfig` JSON)

The `.leconfig` file tells the compiler and desktop design tools the exact limits, pin aliases, and capabilities of your board so designers can never generate invalid logic.

Place `<your_board>.leconfig` in `profiles/<your_board>.leconfig` (or `ports/<your_board>/`):

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
    "nvram_page_size": 256
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

## 6. Program Slot Storage (`le_board_config.c` & Flash HAL)

LogicElements stores compiled `.lebin` programs in non-volatile flash partitions called **Configuration Slots**.

### The Phantom Staging Slot Rule
To prevent network drops or bad serial transmissions from bricking a live controller, the storage manager requires:
$$\text{Physical Flash Slots Reserved} = \text{LE\_MAX\_CONFIG\_SLOTS} + 1$$
The extra partition at index `LE_SLOT_PHANTOM` (`0xFF` / `LE_MAX_CONFIG_SLOTS`) is the **phantom scratch slot**. Incoming programs stream into the phantom slot first. Only when the full program is received and passes full IEEE 802.3 CRC32 verification is it atomically committed to the target user slot.

### Declarative Sector Descriptors (`le_board_config.c`)
Rather than forcing board implementers to write manual slot offset math and sector erase boundary detection inside their HAL drivers, the layout is defined declaratively in `src/le_board_config.c`:

```c
#include "le_board_config.h"
#include "le_storage.h"

const le_storage_sector_t g_board_storage_sectors[] = {
    {
        .slot_id      = 0,
        .medium       = LE_MEDIUM_INTERNAL_FLASH,
        .phys_addr    = 0x08060000UL,
        .size_bytes   = LE_SLOT_SIZE_BYTES,
        .erase_size   = 4096,
        .page_size    = 256,
        .hw_sector_id = 5,
    },
    {
        .slot_id      = 1,
        .medium       = LE_MEDIUM_INTERNAL_FLASH,
        .phys_addr    = 0x08064000UL,
        .size_bytes   = LE_SLOT_SIZE_BYTES,
        .erase_size   = 4096,
        .page_size    = 256,
        .hw_sector_id = 6,
    },
    {
        .slot_id      = LE_SLOT_PHANTOM,
        .medium       = LE_MEDIUM_INTERNAL_FLASH,
        .phys_addr    = 0x08068000UL,
        .size_bytes   = LE_SLOT_SIZE_BYTES,
        .erase_size   = 4096,
        .page_size    = 256,
        .hw_sector_id = 7,
    }
};

const size_t g_board_storage_sector_count = sizeof(g_board_storage_sectors) / sizeof(g_board_storage_sectors[0]);
```

### Implementing Storage HAL Callbacks
With sector descriptors defined in `src/le_board_config.c`, the runtime engine handles sector lookup, automatic pre-erase before flashing, page chunking, and `0xFF` trailing padding.

The board implementer only provides clean, low-level flash hardware primitives in `le_port.c`:

```c
static bool port_storage_read(const le_storage_sector_t* sector, uint32_t offset_in_sector, uint8_t* buf, size_t len)
{
    uint32_t addr = (sector ? sector->phys_addr : 0) + offset_in_sector;
    return bsp_flash_read(addr, buf, len);
}

static bool port_storage_erase_sector(const le_storage_sector_t* sector)
{
    uint32_t addr = sector ? sector->phys_addr : 0;
    return bsp_flash_erase_sector(addr);
}

static bool port_storage_write_page(const le_storage_sector_t* sector, uint32_t offset_in_sector, const uint8_t* buf, size_t len)
{
    uint32_t addr = (sector ? sector->phys_addr : 0) + offset_in_sector;
    return bsp_flash_page_program(addr, buf, len);
}

static const uint8_t* port_storage_get_xip_ptr(const le_storage_sector_t* sector, uint32_t offset_in_sector)
{
    if (!sector || sector->medium != LE_MEDIUM_INTERNAL_FLASH) return NULL;
    return (const uint8_t*)(sector->phys_addr + offset_in_sector);
}
```

Registered in `le_hal_t`:
```c
const le_hal_t s_my_mcu_hal = {
    // ...
    .storage_read         = port_storage_read,
    .storage_erase_sector = port_storage_erase_sector,
    .storage_write_page   = port_storage_write_page,
    .storage_get_xip_ptr  = port_storage_get_xip_ptr,
    // ...
};
```

---

## 7. Operational Non-Volatile Memory (NVRAM / Flash Wear Leveling)

LogicElements manages three kinds of non-volatile operational data using **circuit-driven dynamic NVRAM allocation**:
1. **Retentive Process Image Registers** (`%B`, `%I`, `%F`, `%C`):
   Setpoints, counters, totals, and calibration factors. Retentive registers are isolated per configuration slot so switching between Slot 0 and Slot 1 never overwrites or corrupts another program's setpoints.
2. **Sequence of Events Recorder (SER Log)**:
   Circular FIFO logging physical breaker trips and contact transitions with microsecond timestamps.
3. **Event Recorder (Oscillography)**:
   Waveform fault capture buffers saving analog pre-fault and post-fault samples.

The loaded `.lebin` program designates how many retentive registers, SER entries, and oscillography events it requires. At program load time, the runtime dynamically allocates and partitions these regions within `LE_NVRAM_TOTAL_BYTES`, recording dynamic offsets in the NVRAM manifest.

### Declarative NVRAM Configuration (`le_board_config.c`)
Just like slot storage, define the NVRAM wear-leveling flash pool declaratively in `src/le_board_config.c`:

```c
#include "le_board_config.h"
#include "le_storage.h"

const le_storage_sector_t g_board_nvram_sector = {
    .slot_id      = 0,
    .medium       = LE_MEDIUM_INTERNAL_FLASH,
    .phys_addr    = 0x08070000UL,       /* Base address of reserved flash pool */
    .size_bytes   = LE_NVRAM_TOTAL_BYTES, /* Total pool size (e.g. 16 KB) */
    .erase_size   = 4096,               /* Sector erase size */
    .page_size    = 256,                /* Flash page programming size */
    .hw_sector_id = 8,
};
```

### Auto-Mounting NVRAM Wear Leveling
> [!IMPORTANT]
> **Storage Medium vs. NVRAM Medium Separation**
> In `le_hal_t`, the callbacks for **Program Slot Storage** (`storage_*`) and **Operational NVRAM** (`nvram_*`) are kept strictly independent and separate.
> On real embedded hardware, these two media often have completely different physical backends:
> - **Storage (`storage_*`)** holds compiled `.lebin` circuit bytecode. It is typically internal Flash or high-capacity external QSPI NOR Flash, erased in sectors and written in pages.
> - **NVRAM (`nvram_*`)** stores live setpoints, counter totals, SER fault event logs, and oscillography. It is written frequently and may be an external SPI FRAM (zero erase, byte-writable, trillion-cycle endurance), I2C EEPROM, battery-backed SRAM, or a dedicated wear-leveled flash pool.
>
> Never attempt to combine or conflate them in `le_hal_t`; keeping them decoupled ensures your board can freely mix any storage medium with any NVRAM medium.

When using on-chip Flash with wear leveling, `le_nvram_init_flash(NULL)` automatically discovers the wear-leveling sector from `g_board_nvram_sector` and binds `nvram_erase_sector`, `nvram_write_page`, and `nvram_read` from `g_le_hal`.

Simply call:
```c
void le_port_init(void) {
    /* Auto-mounts wear-leveling journal using g_board_nvram_sector and HAL storage callbacks */
    le_nvram_init_flash(NULL);
}
```

The runtime will automatically construct and mount the wear-leveling driver using your board's `g_board_nvram_sector` configuration.

#### Flash Wear Leveling & Health Tracking Algorithm
The wear leveling engine writes a 16-byte header to each sector tracking its physical erase cycles (`erase_count`). The runtime automatically computes:
$$\text{Wear Percentage} = \min\left(100.0\%,\ \frac{\max(\text{sector\_erase\_counts})}{\text{rated\_erase\_cycles}} \times 100.0\%\right)$$
$$\text{Health Percentage} = 100.0\% - \text{Wear Percentage}$$

- **Real-Time Runtime Diagnostics**: In the interactive CLI, querying **`status`** prints the active sector index, sequence number, erase count, wear %, health %, and remaining slack bytes.
- **Hardware Profile Configuration**: Querying **`info`** and **`caps`** reports the storage backend, sector geometry, and endurance limit.
- **C API**: Firmware code can call `le_nvram_get_stats(&stats)` to query `le_nvram_stats_t` programmatically.

---

### Option B: External Byte-Addressable NVRAM (SPI FRAM, MRAM, EEPROM)
If using an external SPI/I2C FRAM (e.g. MB85RS64, FM25V02, battery SRAM):

```c
/* In le_port.c - internal static hardware callbacks */
static bool port_nvram_read(uint32_t offset, uint8_t* buf, size_t len) {
    return bsp_spi_fram_read(offset, buf, len);
}

static bool port_nvram_write(uint32_t offset, const uint8_t* buf, size_t len) {
    return bsp_spi_fram_write(offset, buf, len);
}

static bool port_nvram_write_page(uint32_t phys_addr, const uint8_t* buf, size_t len) {
    return port_nvram_write(phys_addr, buf, len);
}

static bool port_nvram_erase_sector(uint32_t sector_idx, uint32_t phys_addr) {
    (void)sector_idx; (void)phys_addr;
    /* FRAM supports direct overwrite without erase cycles */
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
    .nvram_read         = port_nvram_read,
    .nvram_write        = port_nvram_write,
    .nvram_write_page   = port_nvram_write_page,
    .nvram_erase_sector = port_nvram_erase_sector,
    // ...
};
```

---

## 8. Timing Model & Scan Rate Verification

LogicElements enforces **deterministic timing**.

### The Circuit-Owned Scan Rate Rule
The scan rate is declared by the circuit designer in their schematic (e.g. `"scan_rate_hz": 1000` = 1 ms period, or `960` Hz for discrete power Fourier tracking).

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

### Three Execution Timing Paradigms for Board Implementers

Board developers can pace the execution cycle using any of three paradigms:

#### Paradigm A: Non-Blocking Cooperative Superloop (`le_vm_scan_due`) — **Recommended**
Ideal for bare-metal systems with interactive CLI/UART communications. The scan runs only when the microsecond boundary arrives; all slack time is used for communications, background compaction, or `__WFI()` (Wait For Interrupt):

```c
while (1) {
    uint32_t now_us = (uint32_t)hal->get_time_us();

    // Guard: Only run when the exact scan boundary arrives
    if (le_vm_scan_due(&vm, now_us)) {
        le_port_read_inputs(&vm.image);
        le_vm_step_us(&vm, now_us);
        le_port_write_outputs(&vm.image);
    }

    // Low-priority background tasks run freely during slack time
    le_comms_cli_pump(&comms, &cli, hal);
}
```

#### Paradigm B: Dedicated Hardware Timer / RTOS Interrupt (`set_timer_rate`)
For sub-millisecond protective relays (e.g. 960 Hz / 2400 Hz), jitter must be near zero. Implement `hal->set_timer_rate(rate_hz, period_us)` in your HAL table. Whenever a new program is loaded, the runtime automatically invokes this hook so you can reconfigure hardware timer ARR / reload registers:

```c
// In le_port.c:
static bool port_set_timer_rate(uint32_t rate_hz, uint32_t period_us)
{
    // Reconfigure hardware timer reload register:
    bsp_hardware_timer_set_frequency(TIM2, rate_hz);
    return true;
}

// In the Hardware Timer ISR / High-Priority RTOS Task:
void TIM2_IRQHandler(void)
{
    TIM2->SR = ~TIM_SR_UIF; // Clear interrupt flag
    uint32_t tick_us = (uint32_t)hal->get_time_us();

    le_port_read_inputs(&vm.image);
    le_vm_step_us(&vm, tick_us);
    le_port_write_outputs(&vm.image);
}
```

#### Paradigm C: Simple Blocking Spin-Wait (`le_vm_run_scan`)
If background communications aren't needed or you want a simple synchronous loop:
```c
while (1) {
    le_port_read_inputs(&vm.image);
    le_vm_run_scan(&vm); // Spin-waits until exact period_us boundary, then executes
    le_port_write_outputs(&vm.image);
}
```

---

## 9. Live Interactive CLI & Program Uploads over UART

LogicElements provides a unified background pump and an interactive command-line interface alongside a framed binary communications protocol over UART.

### Unified Ingestion via `le_comms_cli_pump`
Rather than requiring handwritten byte polling loops in application code, call `le_comms_cli_pump(&comms, &cli, hal)` during scan slack time:

```c
/* In your main execution loop: */
le_comms_cli_pump(&comms, &cli, hal);
```

The pump reads chunks up to `LE_COMMS_CLI_RX_CHUNK_SIZE` (default 32 bytes) from `hal->uart_read()`, automatically routes `0xAA` framed binary packets to `le_comms_process_byte()`, and delivers ASCII terminal characters to `le_cli_process_char()`.

### Buffered UART Drivers with `le_ringbuf_t`
In `le_port.c`, you can back `hal->uart_read` and `hal->uart_write` with the runtime's lock-free single-producer single-consumer circular buffer (`le_ringbuf_t`):

```c
#include "le_ringbuf.h"

static le_ringbuf_t s_uart_rx_ring;
static uint8_t      s_uart_rx_storage[LE_RINGBUF_DEFAULT_SIZE];

void bsp_uart_rx_isr(void)
{
    while (UART_HAS_RX_BYTE()) {
        uint8_t byte = UART_READ_BYTE();
        le_ringbuf_push(&s_uart_rx_ring, byte);
    }
}

static size_t port_uart_read(uint8_t* buf, size_t max_len)
{
    return le_ringbuf_pop_buf(&s_uart_rx_ring, buf, max_len);
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
- `swap <slot>` — Bump-free online swap to slot preserving live state.
- `start` / `stop` — Start or halt virtual machine execution.
- `regs` — Display live internal registers (`%B`, `%I`, `%F`, `%C`).
- `upload <slot> [xmodem]` — Upload new compiled `.lebin` program using XMODEM-CRC into the phantom staging slot.
- `upload <slot> hex` — Upload new compiled `.lebin` program by pasting a hex string.

---

## 10. Exposing Board Peripherals via Declarative Custom Nodes

If your board has unique hardware peripherals (e.g. RP2350 PIO stepper motor, CAN transceiver, hardware cryptographic accelerator, or OLED display), you can expose them as visual blocks in circuit diagrams without writing boilerplate switch statements or handwriting JSON metadata.

### 1. Declare the Node in `src/le_board_config.c`
Define `g_board_custom_nodes[]` in `src/le_board_config.c`:

```c
#include "le_custom_node.h"

static le_status_t relay_driver_handler(const uint16_t* args, int in_count, int out_count, le_process_image_t* img)
{
    bool energize = le_custom_get_bool(img, args, 0);
    bsp_set_physical_relay(energize);
    bool feedback = bsp_read_aux_contact();
    le_custom_set_bool(img, args, in_count, 0, feedback);
    return LE_OK;
}

const le_custom_node_t g_board_custom_nodes[] = {
    {
        .func_id      = 1,
        .type_id      = "BOARD_RELAY_DRIVER",
        .display_name = "Relay Driver",
        .category     = "Actuators",
        .description  = "Physical relay output with aux contact feedback",
        .inputs       = (const le_custom_port_t[]){ { "Energize", LE_PORT_TYPE_BOOL, 0.0f } },
        .in_count     = 1,
        .outputs      = (const le_custom_port_t[]){ { "TripFeedback", LE_PORT_TYPE_BOOL, 0.0f } },
        .out_count    = 1,
        .handler      = relay_driver_handler,
    }
};

const size_t g_board_custom_node_count = sizeof(g_board_custom_nodes) / sizeof(g_board_custom_nodes[0]);
```

### 2. Automatic HAL Delegation
In `le_port.c`, you can leave `.ext_call` and `.get_custom_nodes_json` as `NULL` in `s_my_mcu_hal`:
```c
const le_hal_t s_my_mcu_hal = {
    // ...
    .ext_call              = NULL, /* Defaults automatically to le_custom_nodes_dispatch */
    .get_custom_nodes_json = NULL, /* Defaults automatically to le_custom_nodes_get_json */
};
```
The runtime automatically:
1. Validates input and output argument counts against `.in_count` and `.out_count`.
2. Resolves normalized function IDs (e.g. `1` vs `0x81`).
3. Dispatches to `.handler`.
4. Serializes all custom nodes to JSON for the IDE and UART communications.

### 3. Automatic Target Profile Export (`caps` / `.leconfig`)
You do not need to handcraft a separate `<board>.leconfig` JSON file.
- Connect over serial and type `caps` (or `profile` / `export-config`).
- The runtime streams the complete `.leconfig` JSON specification directly from the compiled firmware metadata.
- Or call `le_config_generate_profile_json(buf, sizeof(buf))` from an automated test or export script.

---

## 11. Board Bring-Up Verification Checklist

Use this checklist to verify your port before deploying to production:

- [ ] **Compilation**: Core runtime (`src/runtime/`) and your port compile with 0 warnings.
- [ ] **RAM Workspace**: `LE_RAM_WORKSPACE_BYTES` is sized comfortably within MCU SRAM.
- [ ] **Pin Aliases**: All inputs in `INPUT_MAP` and outputs in `OUTPUT_MAP` match your `.leconfig`.
- [ ] **Safety Interlock**: Stopping the VM or calling `le_port_deenergize_outputs()` zeroes all physical outputs.
- [ ] **Program Storage**: Flash sector erase and programming work for all $(N+1)$ slots.
- [ ] **Phantom Slot**: Uploading a corrupted binary into the phantom slot is rejected with bad CRC without touching the active program.
- [ ] **Dynamic NVRAM**: Circuit-driven retentive registers, SER depth, and oscillography dynamically fit within `LE_NVRAM_TOTAL_BYTES` without overflow.
- [ ] **Flash Wear Leveling (if using Flash)**: Circular sector rollover functions cleanly across power cycles.
- [ ] **Timing Achievability**: Loading a test program at 1 kHz confirms estimated execution time fits within 1000 µs.
- [ ] **Communications Pump**: `le_comms_cli_pump()` cleanly routes packets and terminal keystrokes during slack time.
- [ ] **CLI Inspection**: Running `caps` over UART prints valid JSON matching your `.leconfig`.
