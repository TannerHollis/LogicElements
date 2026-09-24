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
â”Œâ”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”
â”‚                            HOST / DESKTOP SIDE                              â”‚
â”‚                                                                             â”‚
â”‚   Circuit Schematic (.json)                                                 â”‚
â”‚               â”‚                                                             â”‚
â”‚               â–¼                                                             â”‚
â”‚   le_compile / le_compiler.py -b my_board.leconfig                          â”‚
â”‚               â”‚                                                             â”‚
â”‚               â–¼                                                             â”‚
â”‚   Compiled .lebin (58 bytes per 4 gates, CRC32 verified)                    â”‚
â”‚               â”‚                                                             â”‚
â”‚               â”œâ”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”¬â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”   â”‚
â”‚               â–¼                          â–¼                              â–¼   â”‚
â”‚       Serial Terminal CLI           Python Tools                  Desktop UIâ”‚
â”‚      (PuTTY / TeraTerm)          (le_board.py)                    (Custom)  â”‚
â”‚   [XMODEM-CRC or Hex Paste]    [Framed UART Packets]          [Binary API]  â”‚
â””â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”¼â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”¼â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”¼â”€â”€â”€â”˜
                â”‚                          â”‚                              â”‚
                â””â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”¼â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”˜
                                           â”‚ UART Serial (115200 8N1)
                                           â–¼
â”Œâ”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”
â”‚                        MICROCONTROLLER RUNTIME (C)                          â”‚
â”‚                                                                             â”‚
â”‚   Hardware UART RX (Interrupt / DMA Ring Buffer)                            â”‚
â”‚               â”‚                                                             â”‚
â”‚               â–¼                                                             â”‚
â”‚   â”Œâ”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”  â”‚
â”‚   â”‚ Ingestion Engine:                                                    â”‚  â”‚
â”‚   â”‚   â€¢ le_cli_process_char()    -> Interactive CLI & XMODEM-CRC engine  â”‚  â”‚
â”‚   â”‚   â€¢ le_comms_process_byte()  -> Framed Packet Parser (0xAA SYNC)     â”‚  â”‚
â”‚   â””â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”¬â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”˜  â”‚
â”‚                                      â”‚                                      â”‚
â”‚                                      â–¼                                      â”‚
â”‚   â”Œâ”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”  â”‚
â”‚   â”‚ Non-Volatile Storage Manager (le_storage.c):                         â”‚  â”‚
â”‚   â”‚   â€¢ Validates chunk offset and slot boundary                         â”‚  â”‚
â”‚   â”‚   â€¢ Calls g_le_hal->storage_write() -> Internal Flash / EEPROM / FRAMâ”‚  â”‚
â”‚   â”‚   â€¢ Verifies slot integrity & CRC32                                  â”‚  â”‚
â”‚   â””â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”¬â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”˜  â”‚
â”‚                                      â”‚                                      â”‚
â”‚                                      â–¼                                      â”‚
â”‚   â”Œâ”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”  â”‚
â”‚   â”‚ Virtual Machine Activation (le_vm.c):                                â”‚  â”‚
â”‚   â”‚   â€¢ le_storage_activate_slot() -> Points VM to Flash (XIP) or RAM   â”‚  â”‚
â”‚   â”‚   â€¢ Initializes Process Image (%I, %Q, %M, %R, Timers)              â”‚  â”‚
â”‚   â”‚   â€¢ Resumes deterministic PLC execution loop (le_vm_step)            â”‚  â”‚
â”‚   â””â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”˜  â”‚
â””â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”˜
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
    /* Non-volatile storage (required for multi-slot flashing)            */
    /* ------------------------------------------------------------------ */
    bool        (*storage_read)(uint32_t offset, uint8_t* buf, size_t len);
    bool        (*storage_write)(uint32_t offset, const uint8_t* buf, size_t len);

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

## Step 1: Implement non-volatile storage (`storage_read` and `storage_write`)

LogicElements addresses non-volatile storage as a linear address space divided into slots:
$$\text{Slot Offset} = \text{Slot Index} \times \text{LE\_SLOT\_SIZE\_BYTES}$$

- `LE_MAX_CONFIG_SLOTS` defaults to `3`.
- `LE_SLOT_SIZE_BYTES` defaults to `2048` bytes ($2\text{ KB}$).

### Recipe A: STM32 on-chip flash memory
STM32 flash sectors must be unlocked, erased before writing, and re-locked. Flash memory on STM32F4/G4 typically supports word or half-word writes:

```c
#include "stm32f4xx_hal.h"
#include "le_hal.h"

/* Reserve dedicated sector in upper flash for LogicElements slots (e.g. Sector 7, 128KB at 0x08060000) */
#define LE_FLASH_BASE_ADDR   0x08060000UL
#define LE_FLASH_SECTOR      FLASH_SECTOR_7

static bool stm32_storage_read(uint32_t offset, uint8_t* buf, size_t len)
{
    uint32_t src_addr = LE_FLASH_BASE_ADDR + offset;
    memcpy(buf, (const void*)src_addr, len);
    return true;
}

static bool stm32_storage_write(uint32_t offset, const uint8_t* buf, size_t len)
{
    uint32_t dest_addr = LE_FLASH_BASE_ADDR + offset;

    HAL_FLASH_Unlock();

    /* If writing to the start of a slot (offset aligned to slot), erase target sector/page */
    if ((offset % LE_SLOT_SIZE_BYTES) == 0) {
        FLASH_EraseInitTypeDef erase_init;
        erase_init.TypeErase    = FLASH_TYPEERASE_SECTORS;
        erase_init.Sector       = LE_FLASH_SECTOR;
        erase_init.NbSectors    = 1;
        erase_init.VoltageRange = FLASH_VOLTAGE_RANGE_3;

        uint32_t sector_error = 0;
        if (HAL_FLASHEx_Erase(&erase_init, &sector_error) != HAL_OK) {
            HAL_FLASH_Lock();
            return false;
        }
    }

    /* Program byte-by-byte */
    for (size_t i = 0; i < len; i++) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, dest_addr + i, buf[i]) != HAL_OK) {
            HAL_FLASH_Lock();
            return false;
        }
    }

    HAL_FLASH_Lock();
    return true;
}
```

### Recipe B: Raspberry Pi Pico (RP2040 flash)
The RP2040 provides 2MB external SPI flash. Writes require disabling interrupts and aligning erase operations to 4096-byte boundaries:

```c
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "le_hal.h"

/* Reserve 64KB at 1.5MB mark in flash */
#define PICO_FLASH_OFFSET  (1536 * 1024)

static bool pico_storage_read(uint32_t offset, uint8_t* buf, size_t len)
{
    const uint8_t* flash_target = (const uint8_t*)(XIP_BASE + PICO_FLASH_OFFSET + offset);
    memcpy(buf, flash_target, len);
    return true;
}

static bool pico_storage_write(uint32_t offset, const uint8_t* buf, size_t len)
{
    uint32_t target_offset = PICO_FLASH_OFFSET + offset;

    uint32_t ints = save_and_disable_interrupts();

    /* Erase 4KB sector if starting at slot boundary */
    if ((offset % 4096) == 0) {
        flash_range_erase(target_offset, 4096);
    }

    /* Program 256-byte flash page */
    flash_range_program(target_offset, buf, len);

    restore_interrupts(ints);
    return true;
}
```

### Recipe C: External I2C/SPI EEPROM or FRAM (zero sector erase)
For microcontrollers with limited flash or for industrial PLCs requiring high endurance, use an I2C/SPI EEPROM (e.g., 24LC256) or FRAM (e.g., MB85RS64V). FRAM requires **no erase cycles** and supports unlimited instant byte-level writes:

```c
static bool fram_storage_read(uint32_t offset, uint8_t* buf, size_t len)
{
    return spi_fram_read(offset, buf, len);
}

static bool fram_storage_write(uint32_t offset, const uint8_t* buf, size_t len)
{
    return spi_fram_write(offset, buf, len);
}
```

### Recipe D: Static RAM partitions (rapid prototyping)
If your MCU does not yet have flash drivers written, `le_storage.c` can maintain slots in static internal SRAM partitions:

```c
static uint8_t s_ram_storage[LE_MAX_CONFIG_SLOTS * LE_SLOT_SIZE_BYTES];

static bool ram_storage_read(uint32_t offset, uint8_t* buf, size_t len) {
    if (offset + len <= sizeof(s_ram_storage)) {
        memcpy(buf, &s_ram_storage[offset], len);
        return true;
    }
    return false;
}

static bool ram_storage_write(uint32_t offset, const uint8_t* buf, size_t len) {
    if (offset + len <= sizeof(s_ram_storage)) {
        memcpy(&s_ram_storage[offset], buf, len);
        return true;
    }
    return false;
}
```

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
        â”‚                                                â”‚
        â”‚â”€â”€ LE_CMD_GET_CAPS (0x02) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â–ºâ”‚
        â”‚â—„â”€â”€ LE_CMD_CAPS_DATA (0x82, limits, features) â”€â”€â”‚
        â”‚                                                â”‚
        â”‚â”€â”€ LE_CMD_GET_CUSTOM_NODES (0x03) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â–ºâ”‚
        â”‚â—„â”€â”€ LE_CMD_CUSTOM_NODES_DATA (0x83, JSON) â”€â”€â”€â”€â”€â”€â”‚
        â”‚                                                â”‚
        â”‚â”€â”€ LE_CMD_PROG_BEGIN (0x10, total_size) â”€â”€â”€â”€â”€â”€â”€â–ºâ”‚
        â”‚â—„â”€â”€ LE_CMD_ACK (0x06) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”‚
        â”‚                                                â”‚
        â”‚â”€â”€ LE_CMD_PROG_CHUNK (0x11, offset, data...) â”€â”€â–ºâ”‚
        â”‚â—„â”€â”€ LE_CMD_ACK (0x06) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”‚
        â”‚     (Repeat for all chunks, 64-128B each)      â”‚
        â”‚                                                â”‚
        â”‚â”€â”€ LE_CMD_PROG_END (0x12) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â–ºâ”‚
        â”‚   [MCU verifies CRC32, commits to Flash,       â”‚
        â”‚    reloads VM & auto-starts execution]         â”‚
        â”‚â—„â”€â”€ LE_CMD_ACK (0x06) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”‚
```

Upload `.lebin` files directly using the provided Python utility:
```bash
py tools/compiler/le_board.py --port COM3 --upload my_logic.lebin --slot 0
```

---

## Step 4: Implement custom board nodes and hardware extensions (optional)

Microcontrollers frequently feature specialized on-chip peripheralsâ€”such as hardware PWM timers, quadrature encoders, accelerated DSP filters, or coprocessorsâ€”that you can expose directly to schematic designers.

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

For comprehensive recipes, encoder counters, DSP filtering examples, and circuit JSON schemas, refer to the [Custom Nodes Guide](docs/CUSTOM_NODES_GUIDE.md).

---

## Complete reference implementation (`main.c`)

Here is a complete, production-ready `main.c` showing how to initialize the HAL, Storage, VM, and CLI, while executing a real-time 1 kHz PLC scan loop in parallel with non-blocking UART upload:

```c
#include <stdio.h>
#include <string.h>
#include "le_vm.h"
#include "le_loader.h"
#include "le_storage.h"
#include "le_cli.h"
#include "le_hal.h"
#include "le_port.h"

/* Forward declare your MCU HAL */
extern const le_hal_t* get_my_mcu_hal(void);

int main(void)
{
    /* 1. Initialize Microcontroller Hardware (Clocks, GPIO, UART, Flash) */
    le_port_init();
    const le_hal_t* hal = get_my_mcu_hal();
    le_hal_set(hal);

    /* 2. Initialize LogicElements Engine Subsystems */
    le_storage_t storage;
    le_storage_init(&storage, hal);

    le_vm_t vm;
    le_vm_init(&vm);

    le_cli_t cli;
    le_cli_init(&cli, &vm, &storage);

    /* 3. Automatic Bootloader: Load Active Configuration from Flash */
    uint8_t active_slot = le_storage_get_active_slot(&storage);
    le_slot_info_t info;
    le_storage_get_slot_info(&storage, active_slot, &info);

    if (info.valid) {
        le_storage_activate_slot(&storage, active_slot, &vm);
        vm.running = true;
    } else {
        /* Flash empty: attempt fallback to Slot 0 or wait in CLI mode */
        if (le_storage_activate_slot(&storage, 0, &vm)) {
            vm.running = true;
        }
    }

    /* 4. Real-Time PLC Scan Loop */
    uint32_t last_scan_time = hal->get_time_ms();

    while (1)
    {
        uint32_t now = hal->get_time_ms();

        /* Process Incoming UART Characters (Non-blocking) */
        uint8_t rx_buf[32];
        size_t available = hal->uart_read(rx_buf, sizeof(rx_buf));
        for (size_t i = 0; i < available; i++) {
            le_cli_process_char(&cli, rx_buf[i]);
        }

        /* Execute PLC Scan Cycle at 1 kHz (every 1 ms) */
        if (now != last_scan_time)
        {
            last_scan_time = now;

            if (vm.running)
            {
                /* A. Refresh physical inputs into Process Image (%I) */
                le_port_read_inputs(&vm.image);

                /* B. Execute LogicElements bytecode instructions */
                le_vm_step(&vm, now);

                /* C. Flush Process Image outputs (%Q) to physical relays/LEDs */
                le_port_write_outputs(&vm.image);
            }
            else
            {
                /* Safety Interlock: De-energize outputs when VM is stopped / uploading */
                le_port_deenergize_outputs();
            }
        }
    }
}
```

---

## Memory footprint and resource configuration

Adopters can fine-tune memory limits to match any microcontroller by setting defines in `src/runtime/include/le_types.h` (or via compiler `-D` flags in CMake / Makefile):

```c
/* Number of stored program slots in flash */
#define LE_MAX_CONFIG_SLOTS     3       /* 3 configurations */
#define LE_SLOT_SIZE_BYTES      2048    /* 2 KB per slot */

/* Register memory is fixed-size; element state lives in the state workspace. */
#define LE_MAX_DIGITAL_IN       64      /* %I (8 bytes) */
#define LE_MAX_DIGITAL_OUT      64      /* %Q (8 bytes) */
#define LE_MAX_COILS            128     /* %M (16 bytes) */
#define LE_MAX_FLOATS           64      /* %R (256 bytes) */

/* Zero-heap state workspace shared by ALL stateful elements: the compiler
   bakes each block state (defaults + properties) into a state image in the
   .lebin and the loader copies it into this RAM workspace at load. */
#define LE_RAM_WORKSPACE_BYTES 2048    /* unified RAM workspace (registers + state) */
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
| **Memory limits exceeded on small MCU** | Default budget too large for target RAM. | Override `LE_MAX_CONFIG_SLOTS`, `LE_MAX_FLOATS`, and `LE_MAX_COILS` with smaller values in your build flags. |


