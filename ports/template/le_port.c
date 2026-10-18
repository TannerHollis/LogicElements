/**
 * @file le_port.c
 * @brief Starter Board Port Template (v3 / le_hal_t contract).
 *
 * This is the canonical reference port for custom microcontrollers. It:
 *   - defines GPIO/ADC/UART/storage/I2C/SPI callback implementations,
 *   - assembles them into a complete `le_hal_t` table (`get_my_mcu_hal()`),
 *   - provides the scan-loop helpers `le_port_init`, `le_port_read_inputs`,
 *     `le_port_write_outputs`, `le_port_deenergize_outputs`.
 *
 * Copy this file and `le_port.h` into your project, fill in the TODO hardware
 * calls, and register the table with `le_hal_set(get_my_mcu_hal())`.
 */

#include "le_port.h"
#include "le_storage.h"
#include "le_nvram.h"
#include <string.h>

/* ========================================================================== */
/* Configuration Slots Storage (Compiled .lebin Program Bytecode)             */
/* ========================================================================== */

/**
 * Total storage capacity required across all configuration slots plus the
 * hidden phantom staging slot (LE_PHYSICAL_SLOT_COUNT = LE_MAX_CONFIG_SLOTS + 1).
 */
#define LE_PORT_STORAGE_TOTAL_BYTES  (LE_SLOT_SIZE_BYTES * LE_PHYSICAL_SLOT_COUNT)

/**
 * Program Storage Backend Selection:
 *   0 = In-Memory RAM Mirror (default for rapid board bring-up & simulation)
 *   1 = Microcontroller On-Chip / External NOR Flash (sector erase + page program)
 */
#ifndef LE_PORT_STORAGE_BACKEND
#define LE_PORT_STORAGE_BACKEND      0
#endif

#if (LE_PORT_STORAGE_BACKEND == 0)
static uint8_t s_port_flash[LE_PORT_STORAGE_TOTAL_BYTES] = {0};
#elif (LE_PORT_STORAGE_BACKEND == 1)
#define LE_PORT_FLASH_BASE           0x08060000UL
#define LE_PORT_FLASH_SECTOR_SIZE    4096UL
#endif

/* ========================================================================== */
/* Operational NVRAM Configuration (NV Registers, SER Log, Event Recording)  */
/* ========================================================================== */

/**
 * NVRAM Backend Selection:
 *   0 = In-Memory RAM Mirror (bring-up / simulation)
 *   1 = External SPI NVRAM / FRAM (e.g. MB85RS64, FM25V02 - byte writes, zero erase)
 *   2 = On-Chip / External NOR Flash with runtime wear leveling (no board wear leveling needed!)
 */
#ifndef LE_PORT_NVRAM_BACKEND
#define LE_PORT_NVRAM_BACKEND        0
#endif

#if (LE_PORT_NVRAM_BACKEND == 0)
static uint8_t s_port_nvram[LE_NVRAM_TOTAL_BYTES] = {0};
#elif (LE_PORT_NVRAM_BACKEND == 1)
#define LE_PORT_NVRAM_SPI_CS_PIN     10
#define LE_PORT_NVRAM_CMD_WREN       0x06
#define LE_PORT_NVRAM_CMD_WRITE      0x02
#define LE_PORT_NVRAM_CMD_READ       0x03
#elif (LE_PORT_NVRAM_BACKEND == 2)
#define LE_PORT_NVRAM_FLASH_BASE     0x08070000UL
#define LE_PORT_NVRAM_SECTOR_SIZE    LE_NVRAM_BLOCK_SIZE
#define LE_PORT_NVRAM_PAGE_SIZE      LE_NVRAM_PAGE_SIZE
#define LE_PORT_NVRAM_SECTOR_COUNT   4UL
#endif

/* ========================================================================== */
/* Pin mapping configuration                                                  */
/* ========================================================================== */

typedef struct {
    uint8_t port_id;
    uint8_t pin_num;
} le_pin_map_t;

static const le_pin_map_t INPUT_MAP[] = {
    { 0, 0 }, /* %DIN[0] -> Port 0, Pin 0 */
    { 0, 1 }, /* %DIN[1] -> Port 0, Pin 1 */
    { 0, 2 }, /* %DIN[2] -> Port 0, Pin 2 */
    { 0, 3 }, /* %DIN[3] -> Port 0, Pin 3 */
};
#define NUM_INPUTS (sizeof(INPUT_MAP) / sizeof(INPUT_MAP[0]))

static const le_pin_map_t OUTPUT_MAP[] = {
    { 1, 0 }, /* %DOUT[0] -> Port 1, Pin 0 */
    { 1, 1 }, /* %DOUT[1] -> Port 1, Pin 1 */
    { 1, 2 }, /* %DOUT[2] -> Port 1, Pin 2 */
    { 1, 3 }, /* %DOUT[3] -> Port 1, Pin 3 */
};
#define NUM_OUTPUTS (sizeof(OUTPUT_MAP) / sizeof(OUTPUT_MAP[0]))

/* ========================================================================== */
/* Scan-loop helpers (used by the documented main() loop)                     */
/* ========================================================================== */

void le_port_init(void)
{
    /* TODO: Configure INPUT_MAP pins as Digital Inputs (with pull-up if needed) */
    /* TODO: Configure OUTPUT_MAP pins as Digital Outputs (push-pull) */

#if (LE_PORT_NVRAM_BACKEND == 0)
    le_nvram_init_direct();
#elif (LE_PORT_NVRAM_BACKEND == 1)
    /* TODO: bsp_spi_cs_init(LE_PORT_NVRAM_SPI_CS_PIN); */
    le_nvram_init_direct();
#elif (LE_PORT_NVRAM_BACKEND == 2)
    le_nvram_init_flash(&s_template_wl_driver);
#endif
}

void le_port_read_inputs(le_process_image_t* img)
{
    if (!img) return;

    for (uint8_t i = 0; i < NUM_INPUTS; i++)
    {
        bool pin_state = false;
        /* TODO: pin_state = (MY_MCU_GPIO_READ(INPUT_MAP[i].port_id, INPUT_MAP[i].pin_num) != 0); */
        (void)pin_state;
        le_process_image_set_bool(img, LE_ADDR_MAKE_DIN(i), pin_state);
    }
}

void le_port_write_outputs(const le_process_image_t* img)
{
    if (!img) return;

    /* 1. Flush Digital Outputs (%DOUT) to physical pins */
    for (uint8_t i = 0; i < NUM_OUTPUTS; i++)
    {
        bool state = le_process_image_get_bool(img, LE_ADDR_MAKE_DOUT(i));
        /* TODO: MY_MCU_GPIO_WRITE(OUTPUT_MAP[i].port_id, OUTPUT_MAP[i].pin_num, state); */
        (void)state;
    }

#if LE_ENABLE_ANALOG
    /* 2. Flush Analog Outputs (%AOUT) to physical DAC channels */
    for (uint16_t ch = 0; ch < img->aout_count; ch++)
    {
        float val = le_pi_get_aout(img, ch);
        /* TODO: Write physical DAC channel: bsp_dac_write(ch, val); */
        (void)val;
    }
#endif
}

void le_port_deenergize_outputs(void)
{
    /* 1. De-energize digital outputs */
    for (uint8_t i = 0; i < NUM_OUTPUTS; i++)
    {
        /* TODO: MY_MCU_GPIO_WRITE(OUTPUT_MAP[i].port_id, OUTPUT_MAP[i].pin_num, false); */
    }

#if LE_ENABLE_ANALOG
    /* 2. De-energize analog outputs (reset DACs to 0.0V) */
    for (uint8_t ch = 0; ch < LE_MAX_ANALOG_OUT; ch++)
    {
        /* TODO: Reset physical DAC channel: bsp_dac_write(ch, 0.0f); */
    }
#endif
}

/* ========================================================================== */
/* HAL callback implementations (dispatched through le_hal_t)                 */
/* ========================================================================== */

static bool port_hal_init(void)
{
    le_port_init();
    return true;
}

static void port_hal_shutdown(void)
{
    le_port_deenergize_outputs();
}

static const char* port_hal_get_platform_name(void)
{
    return "Custom MCU";
}

static bool port_gpio_read(uint8_t pin)
{
    (void)pin;
    return false; /* TODO: read physical pin */
}

static void port_gpio_write(uint8_t pin, bool value)
{
    (void)pin; (void)value; /* TODO: write physical pin */
}

static float port_adc_read(uint8_t channel)
{
    (void)channel;
    return 0.0f; /* TODO: read ADC channel as scaled float */
}

static uint32_t port_adc_read_raw(uint8_t channel)
{
    (void)channel;
    return 0UL; /* TODO: read ADC channel raw value */
}

static void port_dac_write(uint8_t channel, float value)
{
    (void)channel; (void)value; /* TODO: write DAC channel */
}

static uint32_t port_get_time_ms(void)
{
    /* TODO: return current millisecond tick (e.g. SysTick or free-running timer) */
    return 0;
}

static uint64_t port_get_time_us(void)
{
    /* TODO: return current microsecond timestamp */
    return (uint64_t)port_get_time_ms() * 1000ULL;
}

static bool port_set_timer_rate(uint32_t rate_hz, uint32_t period_us)
{
    (void)rate_hz; (void)period_us;
    /* TODO (Optional): If using hardware timer interrupts / RTOS pacing,
     * reconfigure hardware timer ARR / reload register or SysTick rate here:
     * e.g. bsp_timer_set_frequency(rate_hz);
     */
    return true;
}

static size_t port_uart_available(void)
{
    return 0; /* TODO: return number of buffered UART RX bytes */
}

static size_t port_uart_read(uint8_t* buf, size_t max_len)
{
    (void)buf; (void)max_len;
    return 0; /* TODO: non-blocking read into buf */
}

static size_t port_uart_write(const uint8_t* buf, size_t len)
{
    (void)buf; (void)len;
    /* TODO: transmit len bytes over UART */
    return len;
}

static bool port_storage_read(const le_storage_sector_t* sector, uint32_t offset_in_sector, uint8_t* buf, size_t len)
{
    if (!buf || !sector) return false;

#if (LE_PORT_STORAGE_BACKEND == 0)
    /* Backend 0: In-Memory RAM Mirror (Bring-up / Simulation) */
    uint32_t rel = sector->phys_addr + offset_in_sector;
    if (rel + len > sizeof(s_port_flash)) return false;
    memcpy(buf, &s_port_flash[rel], len);
    return true;

#elif (LE_PORT_STORAGE_BACKEND == 1)
    /* Backend 1: On-Chip or External NOR Flash (Memory-Mapped Read) */
    const uint8_t* flash_ptr = (const uint8_t*)(sector->phys_addr + offset_in_sector);
    memcpy(buf, flash_ptr, len);
    return true;
#endif
}

static bool port_storage_erase_sector(const le_storage_sector_t* sector)
{
    if (!sector) return false;
    if (sector->erase_size == 0) return true;

#if (LE_PORT_STORAGE_BACKEND == 0)
    uint32_t rel = sector->phys_addr;
    if (rel + sector->size_bytes > sizeof(s_port_flash)) return false;
    memset(&s_port_flash[rel], 0xFF, sector->size_bytes);
    return true;

#elif (LE_PORT_STORAGE_BACKEND == 1)
    /* TODO: bsp_flash_erase_sector(sector->phys_addr); */
    return true;
#endif
}

static bool port_storage_write_page(const le_storage_sector_t* sector, uint32_t offset_in_sector, const uint8_t* buf, size_t len)
{
    if (!buf || !sector) return false;

#if (LE_PORT_STORAGE_BACKEND == 0)
    /* Backend 0: In-Memory RAM Mirror (Bring-up / Simulation) */
    uint32_t rel = sector->phys_addr + offset_in_sector;
    if (rel + len > sizeof(s_port_flash)) return false;
    memcpy(&s_port_flash[rel], buf, len);
    return true;

#elif (LE_PORT_STORAGE_BACKEND == 1)
    /* Backend 1: On-Chip or External NOR Flash (Page Program) */
    /* return bsp_flash_program(sector->phys_addr + offset_in_sector, buf, len); */
    (void)offset_in_sector; (void)buf; (void)len;
    return true;
#endif
}

static const uint8_t* port_storage_get_xip_ptr(const le_storage_sector_t* sector, uint32_t offset_in_sector)
{
    if (!sector) return NULL;
#if (LE_PORT_STORAGE_BACKEND == 0)
    uint32_t rel = sector->phys_addr + offset_in_sector;
    if (rel < sizeof(s_port_flash)) return &s_port_flash[rel];
    return NULL;
#elif (LE_PORT_STORAGE_BACKEND == 1)
    return (const uint8_t*)(sector->phys_addr + offset_in_sector);
#endif
}

/* ========================================================================== */
/* Operational NVRAM Functions (Retentive Registers, SER, Event Recorder)   */
/* ========================================================================== */

#if (LE_PORT_NVRAM_BACKEND == 2)
static bool template_wl_erase(uint32_t sector_idx, uint32_t phys_addr)
{
    (void)sector_idx; (void)phys_addr;
    /* TODO: bsp_flash_erase_sector(phys_addr); */
    return true;
}

static bool template_wl_program(uint32_t phys_addr, const uint8_t* data, size_t len)
{
    (void)phys_addr; (void)data; (void)len;
    /* TODO: bsp_flash_program(phys_addr, data, len); */
    return true;
}

static bool template_wl_read(uint32_t phys_addr, uint8_t* data, size_t len)
{
    (void)phys_addr; (void)data; (void)len;
    /* TODO: memcpy(data, (const void*)phys_addr, len); */
    return true;
}

static const le_flash_wl_driver_t s_template_wl_driver = {
    .flash_base_offset = LE_PORT_NVRAM_FLASH_BASE,
    .sector_size = LE_PORT_NVRAM_SECTOR_SIZE,
    .page_size = LE_PORT_NVRAM_PAGE_SIZE,
    .sector_count = LE_PORT_NVRAM_SECTOR_COUNT,
    .erase_sector = template_wl_erase,
    .program = template_wl_program,
    .read = template_wl_read,
};
#endif

static bool port_nvram_read(uint32_t offset, uint8_t* buf, size_t len)
{
    if (!buf || (offset + len > LE_NVRAM_TOTAL_BYTES)) return false;

#if (LE_PORT_NVRAM_BACKEND == 0)
    /* Backend 0: In-Memory RAM Mirror */
    memcpy(buf, &s_port_nvram[offset], len);
    return true;

#elif (LE_PORT_NVRAM_BACKEND == 1)
    /* Backend 1: External SPI NVRAM / FRAM (e.g. MB85RS64, FM25V02) */
    /* uint8_t cmd[4] = {
     *     LE_PORT_NVRAM_CMD_READ,
     *     (uint8_t)((offset >> 16) & 0xFF),
     *     (uint8_t)((offset >> 8) & 0xFF),
     *     (uint8_t)(offset & 0xFF)
     * };
     * bsp_spi_select(LE_PORT_NVRAM_SPI_CS_PIN);
     * bsp_spi_transmit(cmd, sizeof(cmd));
     * bsp_spi_receive(buf, len);
     * bsp_spi_deselect(LE_PORT_NVRAM_SPI_CS_PIN);
     */
    (void)offset; (void)buf; (void)len;
    return true;

#elif (LE_PORT_NVRAM_BACKEND == 2)
    /* Backend 2: On-Chip Flash with runtime wear leveling */
    return le_nvram_read(offset, buf, len);
#endif
}

static bool port_nvram_write(uint32_t offset, const uint8_t* buf, size_t len)
{
    if (!buf || (offset + len > LE_NVRAM_TOTAL_BYTES)) return false;

#if (LE_PORT_NVRAM_BACKEND == 0)
    /* Backend 0: In-Memory RAM Mirror */
    memcpy(&s_port_nvram[offset], buf, len);
    return true;

#elif (LE_PORT_NVRAM_BACKEND == 1)
    /* Backend 1: External SPI NVRAM / FRAM (e.g. MB85RS64, FM25V02) */
    /* uint8_t wren = LE_PORT_NVRAM_CMD_WREN;
     * bsp_spi_select(LE_PORT_NVRAM_SPI_CS_PIN);
     * bsp_spi_transmit(&wren, 1);
     * bsp_spi_deselect(LE_PORT_NVRAM_SPI_CS_PIN);
     *
     * uint8_t cmd[4] = {
     *     LE_PORT_NVRAM_CMD_WRITE,
     *     (uint8_t)((offset >> 16) & 0xFF),
     *     (uint8_t)((offset >> 8) & 0xFF),
     *     (uint8_t)(offset & 0xFF)
     * };
     * bsp_spi_select(LE_PORT_NVRAM_SPI_CS_PIN);
     * bsp_spi_transmit(cmd, sizeof(cmd));
     * bsp_spi_transmit(buf, len);
     * bsp_spi_deselect(LE_PORT_NVRAM_SPI_CS_PIN);
     */
    (void)offset; (void)buf; (void)len;
    return true;

#elif (LE_PORT_NVRAM_BACKEND == 2)
    /* Backend 2: On-Chip Flash with runtime wear leveling */
    return le_nvram_write(offset, buf, len);
#endif
}

static bool port_nvram_write_page(uint32_t phys_addr, const uint8_t* buf, size_t len)
{
    if (!buf) return false;

#if (LE_PORT_NVRAM_BACKEND == 0)
    if (phys_addr + len > sizeof(s_port_nvram)) return false;
    memcpy(&s_port_nvram[phys_addr], buf, len);
    return true;

#elif (LE_PORT_NVRAM_BACKEND == 1)
    /* Byte NVRAM / FRAM supports page or byte writes directly */
    return port_nvram_write(phys_addr, buf, len);

#elif (LE_PORT_NVRAM_BACKEND == 2)
    return template_wl_program(phys_addr, buf, len);
#endif
}

static bool port_nvram_erase_sector(uint32_t sector_idx, uint32_t phys_addr)
{
#if (LE_PORT_NVRAM_BACKEND == 0)
    (void)sector_idx;
    uint32_t sec_size = LE_NVRAM_BLOCK_SIZE;
    if (phys_addr + sec_size > sizeof(s_port_nvram)) return false;
    memset(&s_port_nvram[phys_addr], 0xFF, sec_size);
    return true;

#elif (LE_PORT_NVRAM_BACKEND == 1)
    /* Byte NVRAM / FRAM requires no sector erase */
    (void)sector_idx; (void)phys_addr;
    return true;

#elif (LE_PORT_NVRAM_BACKEND == 2)
    return template_wl_erase(sector_idx, phys_addr);
#endif
}

#if LE_ENABLE_SERIAL_BUS
static bool port_i2c_write(uint8_t addr_7bit, const uint8_t* tx_data, size_t len)
{
    (void)addr_7bit; (void)tx_data; (void)len;
    return true; /* TODO: perform I2C write */
}

static bool port_i2c_read(uint8_t addr_7bit, uint8_t* rx_data, size_t len)
{
    (void)addr_7bit; (void)rx_data; (void)len;
    return true; /* TODO: perform I2C read */
}

static bool port_i2c_write_read(uint8_t addr_7bit, const uint8_t* tx_data, size_t tx_len,
                                uint8_t* rx_data, size_t rx_len)
{
    (void)addr_7bit; (void)tx_data; (void)tx_len; (void)rx_data; (void)rx_len;
    return true; /* TODO: perform I2C write-then-read */
}

static bool port_spi_transfer(uint8_t cs_pin, const uint8_t* tx_data, uint8_t* rx_data, size_t len)
{
    (void)cs_pin; (void)tx_data; (void)rx_data; (void)len;
    return true; /* TODO: perform SPI transfer with chip-select control */
}
#endif

#include "le_custom_node.h"

static le_status_t port_ext_call(uint8_t func_id, const uint16_t* args,
                                 int in_count, int out_count, le_process_image_t* img)
{
    return le_custom_nodes_dispatch(func_id, args, in_count, out_count, img);
}

static const char* port_get_custom_nodes_json(void)
{
    return le_custom_nodes_get_json();
}

/* ========================================================================== */
/* HAL table                                                                  */
/* ========================================================================== */

const le_hal_t s_my_mcu_hal = {
    .init = port_hal_init,
    .shutdown = port_hal_shutdown,
    .get_platform_name = port_hal_get_platform_name,

    .gpio_read = port_gpio_read,
    .gpio_write = port_gpio_write,

    .adc_read = port_adc_read,
    .adc_read_raw = port_adc_read_raw,
    .dac_write = port_dac_write,

    .get_time_ms = port_get_time_ms,
    .get_time_us = port_get_time_us,
    .set_timer_rate = port_set_timer_rate,

    .uart_available = port_uart_available,
    .uart_read = port_uart_read,
    .uart_write = port_uart_write,

    .storage_read = port_storage_read,
    .storage_erase_sector = port_storage_erase_sector,
    .storage_write_page = port_storage_write_page,
    .storage_get_xip_ptr = port_storage_get_xip_ptr,

    .nvram_read = port_nvram_read,
    .nvram_write = port_nvram_write,
    .nvram_write_page = port_nvram_write_page,
    .nvram_erase_sector = port_nvram_erase_sector,

#if LE_ENABLE_SERIAL_BUS
    .i2c_write = port_i2c_write,
    .i2c_read = port_i2c_read,
    .i2c_write_read = port_i2c_write_read,
    .spi_transfer = port_spi_transfer,
#endif

    .ext_call = port_ext_call,
    .get_custom_nodes_json = port_get_custom_nodes_json,
};

const le_hal_t* get_my_mcu_hal(void)
{
    return &s_my_mcu_hal;
}
