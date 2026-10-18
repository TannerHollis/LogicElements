# Platform Hardware Abstraction Layer (HAL) and UART upload guide

LogicElements v2 uses a decoupled, lightweight C function pointer table (`le_hal_t`) to interface with any microcontroller hardware without touching core logic.

This guide covers everything an adopter needs to build or port a microcontroller board that supports **on-the-fly UART configuration upload of compiled `.lebin` programs**, multi-slot non-volatile flash storage, custom hardware node execution, and live interactive terminal control.

---

## Why on-the-fly UART upload

In industrial control and substation automation, taking down equipment to connect a JTAG or SWD programmer (ST-Link, J-Link, Picoprobe) is costly and impractical. 

LogicElements turns any microcontroller into a field-programmable PLC where:
- **Zero JTAG or SWD required**: Field engineers upload compiled `.lebin` binary circuits over standard USB-UART, RS-232, RS-485, or wireless serial (Bluetooth/Wi-Fi bridge).
- **Zero microcontroller recompilation**: Modifying ladder logic, timers, interlocks, or protection thresholds never requires recompiling or reflashing the firmware C codebase.
- **Multi-slot configuration management**: Store multiple independent `.lebin` programs in flash partitions (e.g. *Recipe A*, *Recipe B*, *Safe Mode*) and switch between them dynamically.
- **Fail-safe and verified**: Every uploaded binary is validated with an IEEE 802.3 CRC32 checksum, 32-byte binary header, and hardware bounds verification before being committed to non-volatile memory or loaded into the virtual machine.

---

## Architecture: How UART upload operates

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                            HOST / DESKTOP SIDE                              │
│                                                                             │
│   Circuit Schematic (.json)                                                 │
│               │                                                             │
│               ▼                                                             │
│   le_compile / le_compiler.py -b my_board.leconfig                          │
│               │                                                             │
│               ▼                                                             │
│   Compiled .lebin (58 bytes per 4 gates, CRC32 verified)                    │
│               │                                                             │
│               ├──────────────────────────┬──────────────────────────────┐   │
│               ▼                          ▼                              ▼   │
│       Serial Terminal CLI           Python Tools                  Desktop UI│
│      (PuTTY / TeraTerm)          (le_board.py)                    (Custom)  │
│   [XMODEM-CRC or Hex Paste]    [Framed UART Packets]          [Binary API]  │
└───────────────┼──────────────────────────┼──────────────────────────────┼───┘
                │                          │                              │
                └──────────────────────────┼──────────────────────────────┘
                                           │ UART Serial (115200 8N1)
                                           ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                        MICROCONTROLLER RUNTIME (C)                          │
│                                                                             │
│   Hardware UART RX (Interrupt / DMA Ring Buffer)                            │
│               │                                                             │
│               ▼                                                             │
│   ┌──────────────────────────────────────────────────────────────────────┐  │
│   │ Ingestion Engine (le_comms_cli_pump):                                │  │
│   │   • le_cli_process_char()    -> Interactive CLI & XMODEM-CRC engine  │  │
│   │   • le_comms_process_byte()  -> Framed Packet Parser (0xAA SYNC)     │  │
│   └──────────────────────────────────┬───────────────────────────────────┘  │
│                                      │                                      │
│                                      ▼                                      │
│   ┌──────────────────────────────────────────────────────────────────────┐  │
│   │ Non-Volatile Storage Manager (le_storage.c):                         │  │
│   │   • Validates chunk offset and slot boundary                         │  │
│   │   • Calls g_le_hal->storage_write() -> Internal Flash / EEPROM / FRAM│  │
│   │   • Verifies slot integrity & CRC32                                  │  │
│   └──────────────────────────────────┬───────────────────────────────────┘  │
│                                      │                                      │
│                                      ▼                                      │
│   ┌──────────────────────────────────────────────────────────────────────┐  │
│   │ Virtual Machine Activation (le_vm.c):                                │  │
│   │   • le_storage_activate_slot() -> Points VM to Flash (XIP) or RAM   │  │
│   │   • Initializes Process Image (%I, %Q, %M, %R, Timers)              │  │
│   │   • Resumes deterministic PLC execution loop (le_vm_step)            │  │
│   └──────────────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## The `le_hal_t` interface

Defined in [`src/runtime/include/le_hal.h`](file:///c:/Users/tanne/OneDrive/Documents/GitHub/LogicElements/src/runtime/include/le_hal.h), the HAL connects LogicElements to physical hardware via function pointers.

For boards supporting UART upload and custom extensions, the following structure members interface with your platform:

```c
typedef struct {
    /* Platform lifecycle */
    bool        (*init)(void);
    void        (*shutdown)(void);
    const char* (*get_platform_name)(void);

    /* GPIO Digital I/O */
    bool        (*gpio_read)(uint8_t pin);
    void        (*gpio_write)(uint8_t pin, bool value);

    /* Analog I/O */
    float       (*adc_read)(uint8_t channel);
    uint32_t    (*adc_read_raw)(uint8_t channel);
    void        (*dac_write)(uint8_t channel, float value);

    /* System time */
    uint32_t    (*get_time_ms)(void);
    uint64_t    (*get_time_us)(void);

    /* ------------------------------------------------------------------ */
    /* UART communications (required for UART upload)                     */
    /* ------------------------------------------------------------------ */
    size_t      (*uart_available)(void);
    size_t      (*uart_read)(uint8_t* buf, size_t max_len);
    size_t      (*uart_write)(const uint8_t* buf, size_t len);

    /* ------------------------------------------------------------------ */
    /* Non-volatile configuration slot storage (.lebin program slots)     */
    /* ------------------------------------------------------------------ */
    bool        (*storage_read)(const struct le_storage_sector_s* sector, uint32_t offset_in_sector, uint8_t* buf, size_t len);
    bool        (*storage_erase_sector)(const struct le_storage_sector_s* sector);
    bool        (*storage_write_page)(const struct le_storage_sector_s* sector, uint32_t offset_in_sector, const uint8_t* buf, size_t len);
    const uint8_t* (*storage_get_xip_ptr)(const struct le_storage_sector_s* sector, uint32_t offset_in_sector);

    /* ------------------------------------------------------------------ */
    /* Dedicated Non-volatile RAM (NV registers, SER log, Event recording)*/
    /* ------------------------------------------------------------------ */
    bool        (*nvram_read)(uint32_t offset, uint8_t* buf, size_t len);
    bool        (*nvram_write)(uint32_t offset, const uint8_t* buf, size_t len);
    bool        (*nvram_write_page)(uint32_t phys_addr, const uint8_t* buf, size_t len);
    bool        (*nvram_erase_sector)(uint32_t sector_idx, uint32_t phys_addr);

#if LE_ENABLE_SERIAL_BUS
    /* I2C and SPI master bus callbacks */
    bool        (*i2c_write)(uint8_t addr_7bit, const uint8_t* tx_data, size_t len);
    bool        (*i2c_read)(uint8_t addr_7bit, uint8_t* rx_data, size_t len);
    bool        (*i2c_write_read)(uint8_t addr_7bit, const uint8_t* tx_data, size_t tx_len, uint8_t* rx_data, size_t rx_len);
    bool        (*spi_transfer)(uint8_t cs_pin, const uint8_t* tx_data, uint8_t* rx_data, size_t len);
#endif

    /* Board custom node and external hardware callbacks */
    le_status_t (*ext_call)(uint8_t func_id, const uint16_t* args, int in_count, int out_count, le_process_image_t* img);
    const char* (*get_custom_nodes_json)(void);
} le_hal_t;
```

---

## Step 1: Implement non-volatile storage (`storage_*` callbacks)

LogicElements manages program slot storage via declarative sector descriptors defined in `src/le_board_config.c`:
- `LE_MAX_CONFIG_SLOTS`: Number of selectable user slots (e.g. `2` or `3`).
- `LE_SLOT_SIZE_BYTES`: Capacity allocated per slot (e.g. `4096` to `2097152` bytes).
- A hidden **phantom staging slot** (`slot_id = LE_SLOT_PHANTOM` or index `LE_MAX_CONFIG_SLOTS`) is reserved to buffer and verify incoming uploads without mutating live operational bytecode.

The runtime storage manager handles slot lookup, page-by-page chunking, padding, and CRC32 verification. The board implementer only provides clean low-level hardware primitives in `le_port.c`:

### Recipe A: STM32 on-chip flash memory
```c
#include "stm32f4xx_hal.h"
#include "le_storage.h"

static bool stm32_storage_read(const le_storage_sector_t* sector, uint32_t offset_in_sector, uint8_t* buf, size_t len)
{
    uint32_t src_addr = (sector ? sector->phys_addr : 0) + offset_in_sector;
    memcpy(buf, (const void*)src_addr, len);
    return true;
}

static bool stm32_storage_erase_sector(const le_storage_sector_t* sector)
{
    if (!sector || sector->erase_size == 0) return true;

    HAL_FLASH_Unlock();
    FLASH_EraseInitTypeDef erase_init;
    erase_init.TypeErase    = FLASH_TYPEERASE_SECTORS;
    erase_init.Sector       = sector->hw_sector_id;
    erase_init.NbSectors    = 1;
    erase_init.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    uint32_t sector_error = 0;
    HAL_StatusTypeDef status = HAL_FLASHEx_Erase(&erase_init, &sector_error);
    HAL_FLASH_Lock();
    return (status == HAL_OK);
}

static bool stm32_storage_write_page(const le_storage_sector_t* sector, uint32_t offset_in_sector, const uint8_t* buf, size_t len)
{
    uint32_t dest_addr = (sector ? sector->phys_addr : 0) + offset_in_sector;

    HAL_FLASH_Unlock();
    for (size_t i = 0; i < len; i++) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, dest_addr + i, buf[i]) != HAL_OK) {
            HAL_FLASH_Lock();
            return false;
        }
    }
    HAL_FLASH_Lock();
    return true;
}

static const uint8_t* stm32_storage_get_xip_ptr(const le_storage_sector_t* sector, uint32_t offset_in_sector)
{
    if (!sector) return NULL;
    return (const uint8_t*)(sector->phys_addr + offset_in_sector);
}
```

### Recipe B: Raspberry Pi Pico (RP2040 / RP2350 QSPI flash)
```c
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "le_storage.h"

static bool pico_storage_read(const le_storage_sector_t* sector, uint32_t offset_in_sector, uint8_t* buf, size_t len)
{
    uint32_t abs_addr = (sector ? sector->phys_addr : 0) + offset_in_sector;
    const uint8_t* flash_target = (const uint8_t*)(XIP_BASE + abs_addr);
    memcpy(buf, flash_target, len);
    return true;
}

static bool pico_storage_erase_sector(const le_storage_sector_t* sector)
{
    if (!sector || sector->erase_size == 0) return true;
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(sector->phys_addr, sector->erase_size);
    restore_interrupts(ints);
    return true;
}

static bool pico_storage_write_page(const le_storage_sector_t* sector, uint32_t offset_in_sector, const uint8_t* buf, size_t len)
{
    uint32_t target_offset = (sector ? sector->phys_addr : 0) + offset_in_sector;
    uint32_t ints = save_and_disable_interrupts();
    flash_range_program(target_offset, buf, len);
    restore_interrupts(ints);
    return true;
}

static const uint8_t* pico_storage_get_xip_ptr(const le_storage_sector_t* sector, uint32_t offset_in_sector)
{
    if (!sector) return NULL;
    return (const uint8_t*)(XIP_BASE + sector->phys_addr + offset_in_sector);
}
```

### Recipe C: Static RAM mirror (bring-up & simulation)
```c
static uint8_t s_ram_storage[LE_SLOT_SIZE_BYTES * LE_PHYSICAL_SLOT_COUNT];

static bool ram_storage_read(const le_storage_sector_t* sector, uint32_t offset_in_sector, uint8_t* buf, size_t len)
{
    uint32_t offset = (sector ? sector->phys_addr : 0) + offset_in_sector;
    if (offset + len <= sizeof(s_ram_storage)) {
        memcpy(buf, &s_ram_storage[offset], len);
        return true;
    }
    return false;
}

static bool ram_storage_erase_sector(const le_storage_sector_t* sector)
{
    if (!sector) return false;
    if (sector->phys_addr + sector->size_bytes <= sizeof(s_ram_storage)) {
        memset(&s_ram_storage[sector->phys_addr], 0xFF, sector->size_bytes);
        return true;
    }
    return false;
}

static bool ram_storage_write_page(const le_storage_sector_t* sector, uint32_t offset_in_sector, const uint8_t* buf, size_t len)
{
    uint32_t offset = (sector ? sector->phys_addr : 0) + offset_in_sector;
    if (offset + len <= sizeof(s_ram_storage)) {
        memcpy(&s_ram_storage[offset], buf, len);
        return true;
    }
    return false;
}

static const uint8_t* ram_storage_get_xip_ptr(const le_storage_sector_t* sector, uint32_t offset_in_sector)
{
    uint32_t offset = (sector ? sector->phys_addr : 0) + offset_in_sector;
    if (offset < sizeof(s_ram_storage)) return &s_ram_storage[offset];
    return NULL;
}
```

---

## Step 1B: Implement Non-Volatile Runtime Memory (NVRAM) & Wear Leveling

LogicElements strictly decouples **static program configuration slots** (`storage_read` / `storage_write` for `.lebin` program code) from **operational non-volatile runtime memory** (`nvram_read` / `nvram_write`).

Operational NVRAM persists:
1. **Retentive Registers**: User-configured subsets of boolean `%B`, integer `%I`, float `%F`, and complex `%C` registers that survive power loss.
2. **Sequence of Events (SER)**: High-resolution timestamped digital transitions logged to a circular journal.
3. **Event Recorder**: Pre-trigger and post-trigger waveform sample captures during electrical protection trips or fault events.

### Circuit-Driven Dynamic NVRAM Allocation

LogicElements defines the total physical NVRAM pool capacity via `LE_NVRAM_TOTAL_BYTES` (default 16 KB) in `include/le_board_config.h` and [`le_nvram.h`](file:///c:/Users/tanne/OneDrive/Documents/GitHub/LogicElements/src/runtime/include/le_nvram.h).

Rather than hardcoding static partition offsets at compile time, the loaded `.lebin` circuit designates its required retentive registers, SER history depth, and oscillography event records. The runtime validates that the circuit's total requirements fit within `LE_NVRAM_TOTAL_BYTES`, dynamically calculates region offsets, and writes them into the 64-byte NVRAM manifest (`le_nvram_manifest_t`) at offset `0x0000`:

| Region Component | Offset Determination | Description |
| :--- | :--- | :--- |
| **Manifest Header** | Offset `0x0000` (64 B) | Magic `0x4D564E4C` (`LNVM`), schema version, dynamic component lengths and offsets |
| **Retentive Registers** | Offset from manifest | Slot-isolated snapshot of active retentive registers (CRC32 validated) |
| **SER Event Journal** | Offset from manifest | Circular Sequence of Events buffer tracking contact and breaker trips |
| **Event Recorder** | Offset from manifest | Analog oscillography pre-fault and post-fault sample capture buffers |

The runtime accesses this region exclusively through `g_le_hal->nvram_read`, `g_le_hal->nvram_write`, `g_le_hal->nvram_write_page`, and `g_le_hal->nvram_erase_sector`.

### Choosing your NVRAM Backend

Depending on your board's hardware design, choose one of two options:

#### Option 1: Direct Byte-Addressable NVRAM (External SPI FRAM, EEPROM, Battery-Backed SRAM)
Industrial PLCs frequently include an SPI FRAM (e.g., Cypress/Infineon FM25V02 or Fujitsu MB85RS64) or an I2C/SPI EEPROM. FRAM supports trillions of write cycles and byte-level writing with zero erase latency.

In this setup, implement `port_nvram_read` and `port_nvram_write` as internal `static` callbacks in `le_port.c` by directly calling your SPI/I2C driver:

```c
/* In le_port.c */
static bool port_nvram_read(uint32_t offset, uint8_t* buf, size_t len)
{
    if (!buf || (offset + len > LE_NVRAM_TOTAL_BYTES)) return false;
    return bsp_spi_fram_read(offset, buf, len);
}

static bool port_nvram_write(uint32_t offset, const uint8_t* buf, size_t len)
{
    if (!buf || (offset + len > LE_NVRAM_TOTAL_BYTES)) return false;
    return bsp_spi_fram_write(offset, buf, len);
}

static bool port_nvram_write_page(uint32_t phys_addr, const uint8_t* buf, size_t len)
{
    return port_nvram_write(phys_addr, buf, len);
}

static bool port_nvram_erase_sector(uint32_t sector_idx, uint32_t phys_addr)
{
    (void)sector_idx; (void)phys_addr;
    /* FRAM supports direct byte writes without sector erase cycles */
    return true;
}

/* Initialize runtime in direct mode */
void board_nvram_init(void)
{
    bsp_spi_fram_init();
    le_nvram_init_direct();
}
```

In your `s_my_mcu_hal` table:
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

#### Option 2: Flash with Built-In Runtime Wear Leveling (Zero User Algorithm Required)
When using on-chip MCU Flash (e.g., STM32, RP2040, RP2350, ESP32), raw flash sectors wear out quickly (typically rated for 10,000 to 100,000 erase cycles). 

**The board implementer does not need to design or write a wear-leveling algorithm.** The LogicElements runtime includes a built-in log-structured wear-leveling journal:
- Sectors rotate cyclically using monotonic sequence IDs and erase counters.
- Writes are appended sequentially with CRC32 records without erasing sectors prematurely.
- Automatic compaction reclaims space by gathering the latest active data into the next clean sector.

The board implementer only provides raw sector erase, page program, and memory read callbacks in an `le_flash_wl_driver_t` descriptor:

```c
#include "le_nvram.h"
#include "hardware/flash.h"
#include "hardware/sync.h"

#define PICO_NVRAM_FLASH_BASE   (1472 * 1024) /* 64 KB reserved below config slots */
#define PICO_NVRAM_SECTOR_SIZE  LE_NVRAM_BLOCK_SIZE /* 4096 */
#define PICO_NVRAM_PAGE_SIZE    LE_NVRAM_PAGE_SIZE  /* 256 */
#define PICO_NVRAM_SECTOR_COUNT 4                   /* 4 sectors = 16 KB pool */

static bool pico_wl_erase(uint32_t sector_idx, uint32_t phys_addr)
{
    (void)sector_idx;
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(phys_addr, PICO_NVRAM_SECTOR_SIZE);
    restore_interrupts(ints);
    return true;
}

static bool pico_wl_program(uint32_t phys_addr, const uint8_t* data, size_t len)
{
    uint32_t ints = save_and_disable_interrupts();
    flash_range_program(phys_addr, data, len);
    restore_interrupts(ints);
    return true;
}

static bool pico_wl_read(uint32_t phys_addr, uint8_t* data, size_t len)
{
    const uint8_t* flash_ptr = (const uint8_t*)(XIP_BASE + phys_addr);
    memcpy(data, flash_ptr, len);
    return true;
}

static const le_flash_wl_driver_t s_pico_wl_driver = {
    .flash_base_offset = PICO_NVRAM_FLASH_BASE,
    .sector_size       = PICO_NVRAM_SECTOR_SIZE,
    .page_size         = PICO_NVRAM_PAGE_SIZE,
    .sector_count      = PICO_NVRAM_SECTOR_COUNT,
    .erase_sector      = pico_wl_erase,
    .program           = pico_wl_program,
    .read              = pico_wl_read,
};

/* Port NVRAM functions delegate to the runtime wear-leveling engine */
static bool port_nvram_read(uint32_t offset, uint8_t* buf, size_t len)
{
    return le_nvram_read(offset, buf, len);
}

static bool port_nvram_write(uint32_t offset, const uint8_t* buf, size_t len)
{
    return le_nvram_write(offset, buf, len);
}

/* Call once at boot */
void board_nvram_init(void)
{
    le_nvram_init_flash(&s_pico_wl_driver);
}
```

Bind `port_nvram_read`, `port_nvram_write`, `pico_wl_program`, and `pico_wl_erase` to `s_my_mcu_hal.nvram_read`, `s_my_mcu_hal.nvram_write`, `s_my_mcu_hal.nvram_write_page`, and `s_my_mcu_hal.nvram_erase_sector` so the runtime engine automatically manages persistent memory.


---

## Step 2: Implement asynchronous UART reception


PLC execution must never stall or block waiting for UART bytes. Always use an **Interrupt-Driven Ring Buffer** (or DMA with idle-line detection):

```c
#define UART_RX_RING_SIZE 256

static volatile uint8_t  s_rx_ring[UART_RX_RING_SIZE];
static volatile uint16_t s_rx_head = 0;
static volatile uint16_t s_rx_tail = 0;

/* Call this from your MCU UART RX Interrupt Service Routine */
void mcu_uart_rx_isr(uint8_t byte)
{
    uint16_t next = (s_rx_head + 1) % UART_RX_RING_SIZE;
    if (next != s_rx_tail) {
        s_rx_ring[s_rx_head] = byte;
        s_rx_head = next;
    }
    /* If full, byte is safely dropped without locking up the bus */
}

/* HAL: Number of available unread bytes */
static size_t hal_uart_available(void)
{
    return (s_rx_head >= s_rx_tail) ? 
           (s_rx_head - s_rx_tail) : 
           (UART_RX_RING_SIZE - s_rx_tail + s_rx_head);
}

/* HAL: Non-blocking read into destination buffer */
static size_t hal_uart_read(uint8_t* buf, size_t max_len)
{
    size_t count = 0;
    while (count < max_len && s_rx_tail != s_rx_head) {
        buf[count++] = s_rx_ring[s_rx_tail];
        s_rx_tail = (s_rx_tail + 1) % UART_RX_RING_SIZE;
    }
    return count;
}

/* HAL: Transmit bytes over UART */
static size_t hal_uart_write(const uint8_t* buf, size_t len)
{
    if (buf && len > 0) {
        /* Replace with vendor transmit: HAL_UART_Transmit(&huart2, buf, len, 100); */
        vendor_uart_transmit(buf, len);
    }
    return len;
}
```

---

## Step 3: Integrate the runtime and upload engines

LogicElements offers two complementary methods for uploading `.lebin` files over UART. Both can coexist on the same serial port:

### Option 1: Interactive terminal CLI (`le_cli`)
Allows technicians to use any standard serial terminal (such as PuTTY, TeraTerm, Minicom, or screen at 115200 8N1) without specialized desktop software.

#### Supported CLI commands
| Command | Description | Example |
| :--- | :--- | :--- |
| `help` | Print command cheat sheet | `help` |
| `info` | Print target MCU info and memory limits | `info` |
| `caps` | Dump machine-readable `.leconfig` JSON profile | `caps` |
| `nodes` | Print board custom nodes and hardware extensions JSON | `nodes` |
| `status` | VM status (RUNNING/STOPPED), active slot, cycle count | `status` |
| `slots` | List all slots, validity flags, and CRC32 | `slots` |
| `select <slot>` | Switch active slot and reload VM on the fly | `select 1` |
| `swap <slot>` | Bump-free online program swap preserving live register state | `swap 1` |
| `run` / `stop` | Start or pause PLC scan execution | `run` |
| `reset` | Clear process image memory (`%I`, `%Q`, `%M`, `%R`) | `reset` |
| `io` | Print live table of digital inputs, outputs, and coils | `io` |
| `force <addr> <v>`| Force input or output high or low for field testing | `force %I0 1` |
| `upload <s> [xmodem]`| Upload `.lebin` using secure XMODEM-CRC transfer | `upload 0 xmodem` |
| `upload <s> hex` | Upload `.lebin` by pasting ASCII hexadecimal string | `upload 0 hex` |

#### Upload using XMODEM-CRC
1. Open PuTTY or TeraTerm connected to the MCU serial port (`115200 8N1`).
2. Type `upload 0 xmodem` and press Enter.
3. The MCU prints:
   ```text
   Ready for XMODEM-CRC upload to Slot 0.
   Start transfer in your terminal...
   C
   ```
4. In TeraTerm, select: **File -> Transfer -> XMODEM -> Send...**
5. Select the compiled `my_logic.lebin` file and ensure **CRC** is selected.
6. The terminal transfers 128-byte blocks with CRC16 validation per packet.
7. Upon completion, the MCU prints:
   ```text
   [OK] XMODEM Transfer Complete! Verified 4 instructions in Slot 0.
   ```
8. Activate and run:
   ```text
   LE> select 0
   Switched to Slot 0. Loaded 4 instructions.
   LE> run
   VM Started.
   ```

#### Upload using hexadecimal paste
When using a simple serial monitor that does not support XMODEM file transfers:
1. Type `upload 0 hex` and press Enter.
2. Open `my_logic.lebin` in a hex editor or copy the hex string generated by `le_disasm` or `le_disasm.py --hex`.
3. Paste the hex string into the terminal window and send an empty newline:
   ```text
   [OK] Hex Upload Complete! Loaded 4 instructions into Slot 0.
   ```

---

### Option 2: Programmatic framed packet protocol (`le_comms`)
For desktop IDEs, SCADA masters, Python automated testing, or custom host software, LogicElements implements a low-overhead binary framing protocol:

`[0xAA (SYNC)] [CMD: 1B] [SEQ: 1B] [LEN: 2B] [PAYLOAD: LEN bytes] [CRC16: 2B]`

#### Upload protocol flow
```
HOST (Desktop / CI Tool)                     TARGET MCU (LogicElements)
        │                                                │
        │── LE_CMD_GET_CAPS (0x02) ─────────────────────►│
        │◄── LE_CMD_CAPS_DATA (0x82, limits, features) ──│
        │                                                │
        │── LE_CMD_GET_CUSTOM_NODES (0x03) ─────────────►│
        │◄── LE_CMD_CUSTOM_NODES_DATA (0x83, JSON) ──────│
        │                                                │
        │── LE_CMD_PROG_BEGIN (0x10, total_size) ───────►│
        │◄── LE_CMD_ACK (0x06) ──────────────────────────│
        │                                                │
        │── LE_CMD_PROG_CHUNK (0x11, offset, data...) ──►│
        │◄── LE_CMD_ACK (0x06) ──────────────────────────│
        │     (Repeat for all chunks, 64-128B each)      │
        │                                                │
        │── LE_CMD_PROG_END (0x12) ─────────────────────►│
        │   [MCU verifies CRC32, commits to Flash,       │
        │    reloads VM & auto-starts execution]         │
        │◄── LE_CMD_ACK (0x06) ──────────────────────────│
```

Upload `.lebin` files directly using the provided Python utility:
```bash
py tools/compiler/le_board.py --port COM3 --upload my_logic.lebin --slot 0
```

---

## Step 4: Implement custom board nodes and hardware extensions (optional)

Microcontrollers frequently feature specialized on-chip peripherals—such as hardware PWM timers, quadrature encoders, accelerated DSP filters, or coprocessors—that you can expose directly to schematic designers.

LogicElements carries custom nodes as `LE_OP_BLOCK` (`0xA0`) with a function id ≥ `LE_FUNC_CUSTOM_BASE (0x80)`, dispatched to the HAL `ext_call`. When the runtime executes such an `LE_OP_BLOCK` call, it dispatches to the adopter callback registered in `le_hal_t`:

```c
le_status_t (*ext_call)(uint8_t func_id, const uint16_t* args, int in_count, int out_count, le_process_image_t* img);
const char* (*get_custom_nodes_json)(void);
```

### 1. Define custom nodes metadata in JSON
Provide a static JSON array describing the node types, numeric function IDs, categories, inputs, and outputs. This string is exposed through `get_custom_nodes_json()` and reported to technicians via the CLI `nodes` command and binary command `0x03` (`LE_CMD_GET_CUSTOM_NODES`):

```c
static const char s_custom_nodes_json[] = 
"["
"  {"
"    \"type\": \"BOARD_PWM\","
"    \"func_id\": 1,"
"    \"category\": \"Hardware\","
"    \"description\": \"Hardware PWM duty cycle generator\","
"    \"inputs\": [\"DUTY\", \"ENABLE\"],"
"    \"outputs\": [\"ACTIVE\"]"
"  }"
"]";

static const char* hal_get_custom_nodes_json(void)
{
    return s_custom_nodes_json;
}
```

### 2. Implement the hardware callback (`ext_call`)
In your HAL implementation, inspect `func_id` and map the operands (`in_a`, `in_b`, `out`) to the process image memory using the accessor helpers:

```c
static le_status_t hal_ext_call(uint8_t func_id, const uint16_t* args, int in_count, int out_count, le_process_image_t* img)
{
    if (!img) {
        return LE_ERROR;
    }

    switch (func_id) {
    case 0x81: { /* BOARD_PWM */
        /* in_a contains float duty cycle (%R address), in_b contains enable flag (%I or %M) */
        float duty = le_process_image_get_float(img, args[0]);
        bool enable = le_process_image_get_bool(img, args[1]);

        if (enable) {
            if (duty < 0.0f) duty = 0.0f;
            if (duty > 100.0f) duty = 100.0f;
            vendor_pwm_set_duty_cycle(duty);
            le_process_image_set_bool(img, args[in_count], true);
        } else {
            vendor_pwm_set_duty_cycle(0.0f);
            le_process_image_set_bool(img, args[in_count], false);
        }
        return LE_OK;
    }
    default:
        return LE_ERROR;
    }
}
```

Assign both callbacks in your `le_hal_t` structure:
```c
static const le_hal_t s_my_hal = {
    /* ... standard HAL callbacks ... */
    .ext_call              = hal_ext_call,
    .get_custom_nodes_json = hal_get_custom_nodes_json,
};
```

For comprehensive recipes, encoder counters, DSP filtering examples, and circuit JSON schemas, refer to the [Custom Nodes Guide](CUSTOM_NODES_GUIDE.md).

---

## Complete reference implementation (`main.c`)

Here is a complete, production-ready `main.c` showing how to initialize the HAL, Storage, VM, Comms, and CLI, while executing a real-time drift-free PLC scan loop in parallel with non-blocking UART communications:

```c
#include <stdio.h>
#include <string.h>
#include "le_vm.h"
#include "le_loader.h"
#include "le_storage.h"
#include "le_comms.h"
#include "le_cli.h"
#include "le_hal.h"
#include "le_port.h"

/* Forward declare your MCU HAL */
extern const le_hal_t* get_my_mcu_hal(void);

int main(void)
{
    /* 1. Initialize Microcontroller Hardware (Clocks, GPIO, UART, Flash) */
    bsp_system_init();
    const le_hal_t* hal = get_my_mcu_hal();
    if (hal->init) hal->init();
    le_hal_set(hal);
    le_port_init();

    /* 2. Initialize LogicElements Engine Subsystems */
    le_vm_t vm;
    le_vm_init(&vm);

    le_storage_t storage;
    le_storage_init(&storage);

    le_comms_t comms;
    le_comms_init(&comms, &vm, &storage);

    le_cli_t cli;
    le_cli_init(&cli, &vm, &storage);

    /* 3. Automatic Bootloader: Load Active Configuration or Fallback to ROM Demo */
    #include "test_circuit.h" // Optional embedded fallback
    const char* boot_source = NULL;
    le_storage_autostart_or_demo(&vm, &storage, 0, test_circuit_program, sizeof(test_circuit_program), &boot_source);

    /* 4. Real-Time Deterministic PLC Scan Loop */
    while (1)
    {
        uint32_t now_us = (uint32_t)hal->get_time_us();

        /* Non-blocking fixed-rate scan gate (drift-free): */
        if (le_vm_scan_due(&vm, now_us))
        {
            if (vm.running)
            {
                /* A. Refresh physical inputs into Process Image (%DIN, %AIN) */
                le_port_read_inputs(&vm.image);

                /* B. Execute LogicElements bytecode instructions */
                le_vm_step_us(&vm, now_us);

                /* C. Flush Process Image outputs (%DOUT, %AOUT) to physical relays/LEDs */
                le_port_write_outputs(&vm.image);
            }
            else
            {
                /* Safety Interlock: De-energize outputs when VM is stopped */
                le_port_deenergize_outputs();
            }
        }

        /* Slack time: Process incoming UART binary packets and interactive CLI */
        le_comms_cli_pump(&comms, &cli, hal);
    }
}
```

---

## Memory footprint and resource configuration

Adopters can fine-tune memory limits, core topology, and feature switches to match any microcontroller by setting defines in `src/runtime/include/le_config.h` (or via compiler `-D` flags in CMake / Makefile or custom `-DLE_CONFIG_FILE="path/to/my_config.h"`):

```c
/* Number of stored program slots in flash */
#define LE_MAX_CONFIG_SLOTS     3       /* 3 configurations */
#define LE_SLOT_SIZE_BYTES      2048    /* 2 KB per slot */

/* Physical board hardware pin capacities */
#define LE_MAX_DIGITAL_IN       64      /* %DIN */
#define LE_MAX_DIGITAL_OUT      64      /* %DOUT */
#define LE_MAX_ANALOG_IN        16      /* %AIN */
#define LE_MAX_ANALOG_OUT       16      /* %AOUT */

/* Unified dynamic RAM workspace: internal registers (%B, %I, %F, %C)
   and element state blocks (timers, filters, relays) all dynamically
   share this unified memory pool. No static per-type register limits! */
#define LE_RAM_WORKSPACE_BYTES  2048    /* unified RAM workspace */
```

### Typical footprint across architectures
| Target Platform | Arch | Flash Footprint | RAM Usage | Max Instructions | Slots Supported |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Microchip ATmega328P** | 8-bit AVR | 11.2 KB | 780 bytes | 48 instructions | 1 slot $\times$ 512B |
| **STM32F0 / Cortex-M0+** | 32-bit ARM | 14.5 KB | 1.2 KB | 128 instructions | 2 slots $\times$ 1024B |
| **STM32F4 / Cortex-M4F** | 32-bit ARM | 18.8 KB | 1.8 KB | 512 instructions | 3 slots $\times$ 2048B |
| **RP2040 / Raspberry Pi Pico** | Dual M0+ | 19.4 KB | 2.1 KB | 1024 instructions| 4 slots $\times$ 4096B |

---

## Board profile specification (`.leconfig`)

To ensure desktop designers never generate logic exceeding your board's physical pins, memory budgets, or flash size, create a `.leconfig` file matching your hardware.

Example `my_board.leconfig`:
```json
{
  "device": {
    "name": "Custom-PLC-CortexM4",
    "firmware_version": "1.0",
    "protocol_version": 1
  },
  "limits": {
    "digital_inputs": 16,
    "digital_outputs": 16,
    "coils": 128,
    "floats": 64,
    "workspace_bytes": 2048,
    "config_slots": 3,
    "slot_size_bytes": 2048
  },
  "features": {
    "protection": true,
    "serial_bus": true
  },
  "custom_nodes": [
    {
      "type": "BOARD_PWM",
      "func_id": 1,
      "category": "Hardware",
      "description": "Hardware PWM duty cycle generator",
      "inputs": ["DUTY", "ENABLE"],
      "outputs": ["ACTIVE"]
    }
  ],
  "pin_map": {
    "inputs": {
      "%I0": { "pin": "PA0", "alias": "ESTOP_BUTTON", "desc": "Emergency Stop Contact" },
      "%I1": { "pin": "PA1", "alias": "START_PB", "desc": "Start Pushbutton" }
    },
    "outputs": {
      "%Q0": { "pin": "PB0", "alias": "MOTOR_CONTACTOR", "desc": "Main Motor Contactor" },
      "%Q1": { "pin": "PB7", "alias": "FAULT_PILOT", "desc": "Red Fault Warning Strobe" }
    }
  }
}
```

### Compiler validation enforcement
When compiling circuits, provide `-b my_board.leconfig`:

```bash
# Using the C++ optimizing compiler:
le_compile circuit.json -b my_board.leconfig -o circuit.lebin

# Or using the Python compiler CLI:
py tools/compiler/le_compiler.py circuit.json -b my_board.leconfig -o circuit.lebin
```

- **Pin aliases**: The schematic can use symbolic names (`ESTOP_BUTTON`, `MOTOR_CONTACTOR`), and the compiler maps them to `%I0` and `%Q0`.
- **Constraint checks**: Rejects circuits using more I/O than the board possesses or referencing opcodes not enabled on the board.
- **Custom node resolution**: Resolves schematic blocks of type `BOARD_PWM` into `LE_OP_BLOCK` calls with a custom function id (>= `LE_FUNC_CUSTOM_BASE (0x80)`).

---

## Field diagnostics and troubleshooting

| Issue | Likely Cause | Solution |
| :--- | :--- | :--- |
| **XMODEM times out with 'C'** | Terminal emulator not sending 128-byte blocks or baud rate mismatch. | Verify serial terminal is set to 115200 8N1. Select "CRC" mode in XMODEM send dialog. |
| **`[FAIL] CRC32 Mismatch` after upload** | UART dropped bytes or flash erase failed before write. | Ensure UART RX ring buffer is interrupt/DMA driven. Verify sector erase was called at `offset == 0`. |
| **Outputs glitch during slot reload** | VM continued stepping while new configuration was being activated. | Call `le_port_deenergize_outputs()` or pause VM (`vm.running = false`) during `le_storage_activate_slot()`. |
| **MCU hard faults during flash write** | Flash programming address not aligned or interrupts not disabled. | On RP2040, wrap flash programming in `save_and_disable_interrupts()`. On STM32, check Flash write alignment requirements. |
| **Memory limits exceeded on small MCU** | Default budget too large for target RAM. | Override `LE_RAM_WORKSPACE_BYTES` and `LE_MAX_CONFIG_SLOTS` with smaller values in your build flags. |


